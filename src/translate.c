#include <stdio.h>
#include "string.h"
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "temp.h"
#include "tree.h"
#include "frame.h"
#include "translate.h"
#include "printtree.h"

#define F_WORD_SIZE 4

struct Tr_level_ {
    Tr_level parent;
    Temp_label name;
    F_frame frame;
    Tr_accessList formals;
};

struct Tr_access_ {
    Tr_level level;
    F_access access;
};

static Tr_accessList makeFormalAccessList(Tr_level);
static Tr_access Tr_Access(Tr_level, F_access);

struct Cx {
    patchList trues;
    patchList falses;
    T_stm stm;
};

struct Tr_exp_ {
    enum {Tr_ex, Tr_nx, Tr_cx} kind;
    union {
        T_exp ex;    
        T_stm nx;    
        struct Cx cx;
    } u;
};

struct patchList_ {
    Temp_label * head;
    patchList tail;
};
static Tr_exp Tr_Ex(T_exp);
static Tr_exp Tr_Nx(T_stm);
static Tr_exp Tr_Cx(patchList, patchList, T_stm);
static patchList PatchList(Temp_label *, patchList);
static T_exp unEx(Tr_exp);
static T_stm unNx(Tr_exp);
static struct Cx unCx(Tr_exp);
static void doPatch(patchList, Temp_label);
static patchList joinPatch(patchList, patchList);
static Tr_exp Tr_StaticLink(Tr_level, Tr_level);

Tr_expList Tr_ExpList(Tr_exp h, Tr_expList t) {
    Tr_expList l = checked_malloc(sizeof(*l));
    l->head = h;
    l->tail = t;
    return l;
}

void Tr_expList_prepend(Tr_exp h, Tr_expList * l) {
    Tr_expList newhead = Tr_ExpList(h, NULL);
    newhead->tail = *l;
    *l = newhead;
}

static Tr_exp Tr_Ex(T_exp exp) {
    Tr_exp e = checked_malloc(sizeof(*e));
    e->kind = Tr_ex;
    e->u.ex = exp;
    return e;
}

static Tr_exp Tr_Nx(T_stm stm) {
    Tr_exp e = checked_malloc(sizeof(*e));
    e->kind = Tr_nx;
    e->u.nx = stm;
    return e;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm) {
    Tr_exp e = checked_malloc(sizeof(*e));
    e->kind = Tr_cx;
    e->u.cx.trues  = trues;
    e->u.cx.falses = falses;
    e->u.cx.stm    = stm;
    return e;
}

static T_exp unEx(Tr_exp e) {
    switch (e->kind) {
    case Tr_ex:
        return e->u.ex;
    case Tr_nx:
        return T_Eseq(e->u.nx, T_Const(0));
    case Tr_cx: {
        Temp_temp r = Temp_newtemp(); 
        Temp_label t = Temp_newlabel(), f = Temp_newlabel(); 
        doPatch(e->u.cx.trues, t);
        doPatch(e->u.cx.falses, f);
        return T_Eseq(T_Move(T_Temp(r), T_Const(1)),
                      T_Eseq(e->u.cx.stm,
                             T_Eseq(T_Label(f),
                                    T_Eseq(T_Move(T_Temp(r), T_Const(0)),
                                           T_Eseq(T_Label(t), T_Temp(r))))));
        }
    }
    assert(0 && "only 3 condition");
}

static T_stm unNx(Tr_exp e) {
    switch (e->kind) {
    case Tr_ex:
        return T_Exp(e->u.ex);
    case Tr_nx:
        return e->u.nx;
    case Tr_cx: {
        Temp_temp r = Temp_newtemp();
        Temp_label t = Temp_newlabel(), f = Temp_newlabel();
        doPatch(e->u.cx.trues, t);
        doPatch(e->u.cx.falses, f);
        return T_Exp(T_Eseq(T_Move(T_Temp(r), T_Const(1)),
                            T_Eseq(e->u.cx.stm,
                                   T_Eseq(T_Label(f),
                                          T_Eseq(T_Move(T_Temp(r), T_Const(0)),
                                                 T_Eseq(T_Label(t), T_Temp(r)))))));

        }
    }
    assert(0);
}

static struct Cx unCx(Tr_exp e){
    switch (e->kind) {
    case Tr_cx:
        return e->u.cx;
    case Tr_ex: {
        struct Cx cx;
        cx.stm = T_Cjump(T_ne, e->u.ex, T_Const(0), NULL, NULL);
        cx.trues = PatchList(&(cx.stm->u.CJUMP.true), NULL);
        cx.falses = PatchList(&(cx.stm->u.CJUMP.false), NULL);
        return cx;
    }
    case Tr_nx:
        assert(0);
    }
    assert(0);
}

Tr_exp Tr_simpleVar(Tr_access ac, Tr_level l) {
    T_exp addr = T_Temp(F_EBP());
    while (l != ac->level) { 
        F_access sl = F_formals(l->frame)->head;
        addr = F_Exp(sl, addr);
        l = l->parent;
    }
    return Tr_Ex(F_Exp(ac->access, addr)); 
}

Tr_exp Tr_fieldVar(Tr_exp base, int offs) {
    return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(base), T_Const(offs * F_WORD_SIZE))));
}

Tr_exp Tr_subscriptVar(Tr_exp base, Tr_exp index) {
    return Tr_Ex(T_Mem(T_Binop(T_plus,
                               unEx(base),
                               T_Binop(T_mul, unEx(index), T_Const(F_WORD_SIZE)))));
}

static F_fragList fragList       = NULL;
void Tr_procEntryExit(Tr_level level, Tr_exp body, S_symbol result) {
    T_stm stm;
    if (result)
        stm = T_Seq(T_Label(level->name), T_Move(T_Temp(F_EAX()), unEx(body)));
    else
        stm = T_Seq(T_Label(level->name), unNx(body));

    F_frag procfrag = F_ProcFrag(stm, level->frame);
    fragList = F_FragList(procfrag, fragList);
}

static F_fragList stringFragList = NULL;
Tr_exp Tr_stringExp(string s) { 
    Temp_label slabel = Temp_newlabel();
    F_frag fragment = F_StringFrag(slabel, s);
    stringFragList = F_FragList(fragment, stringFragList);
    return Tr_Ex(T_Name(slabel));
}

Tr_exp Tr_intExp(int i) {
    return Tr_Ex(T_Const(i));
}

static Temp_temp nilTemp = NULL;
Tr_exp Tr_nilExp() {
    return Tr_Ex(T_Const(0));
}

Tr_exp Tr_noExp() {
    return Tr_Ex(T_Const(0));
}

Tr_exp Tr_recordExp(int n, Tr_expList l) {
    Temp_temp r = Temp_newtemp();
    T_stm alloc = T_Move(T_Temp(r),
                         T_Eseq(T_Exp(F_externalCall(String("allocRecord"), T_ExpList(T_Const(n * F_WORD_SIZE), NULL))), T_Temp(F_EAX())));

    int i = n - 1;
    T_stm seq = T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(i-- * F_WORD_SIZE))), 
                             unEx(l->head));

    for (l = l->tail; l; l = l->tail, i--) {
        seq = T_Seq(T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(i * F_WORD_SIZE))), 
                           unEx(l->head)),
                    seq);
    }
    return Tr_Ex(T_Eseq(T_Seq(alloc, seq), T_Temp(r)));
}

Tr_exp Tr_arrayExp(Tr_exp size, Tr_exp init) {
    return Tr_Ex(T_Eseq(T_Exp(F_externalCall(String("initArray"), 
                                T_ExpList(unEx(size), T_ExpList(unEx(init), NULL)))), T_Temp(F_EAX())));
}

Tr_exp Tr_seqExp(Tr_expList l) {
    T_exp resl = unEx(l->head); 
    for (l = l->tail; l; l = l->tail) {
        resl = T_Eseq(T_Exp(unEx(l->head)), resl);
    }
    return Tr_Ex(resl);
}

Tr_exp Tr_doneExp() { return Tr_Ex(T_Name(Temp_newlabel())); } 

Tr_exp Tr_whileExp(Tr_exp test, Tr_exp body, Tr_exp done) {
    Temp_label testl = Temp_newlabel(), bodyl = Temp_newlabel(), 
               donel = unEx(done)->u.NAME;
    return Tr_Nx(
             T_Seq(
               T_Label(testl),
               T_Seq(
                 T_Cjump(T_eq, unEx(test), T_Const(0), donel, bodyl),
                 T_Seq(
                   T_Label(bodyl),
                   T_Seq(
                     unNx(body),
                     T_Seq(
                        T_Jump(T_Name(testl), Temp_LabelList(testl, NULL)),
                        T_Label(donel)))))));
}

Tr_exp Tr_assignExp(Tr_exp lval, Tr_exp exp) { return Tr_Nx(T_Move(unEx(lval), unEx(exp))); }

Tr_exp Tr_breakExp(Tr_exp b) { return Tr_Nx(T_Jump(T_Name(unEx(b)->u.NAME), Temp_LabelList(unEx(b)->u.NAME, NULL))); }

Tr_exp Tr_arithExp(A_oper op, Tr_exp left, Tr_exp right) {
    T_binOp opp;
    switch(op) {
    case A_plusOp  : opp = T_plus; break;
    case A_minusOp : opp = T_minus; break;
    case A_timesOp : opp = T_mul; break;
    case A_divideOp: opp = T_div; break;
    default: assert(0);
    }
    return Tr_Ex(T_Binop(opp, unEx(left), unEx(right)));
}

Tr_exp Tr_eqExp(A_oper op, Tr_exp left, Tr_exp right) {
    T_relOp opp;
    if (op == A_eqOp) 
        opp = T_eq; 
    else 
        opp = T_ne;
    T_stm cond = T_Cjump(opp, unEx(left), unEx(right), NULL, NULL);
    patchList trues = PatchList(&cond->u.CJUMP.true, NULL);
    patchList falses = PatchList(&cond->u.CJUMP.false, NULL);
    return Tr_Cx(trues, falses, cond);
}

Tr_exp Tr_eqStringExp(A_oper op, Tr_exp left, Tr_exp right) {
    T_exp resl = F_externalCall(String("stringEqual"), T_ExpList(unEx(left), T_ExpList(unEx(right), NULL)));
    if (op == A_eqOp) return Tr_Ex(resl);
    else if (op == A_neqOp){
        T_exp e = (resl->kind == T_CONST && resl->u.CONST != 0) ? T_Const(0): T_Const(1);
        return Tr_Ex(e);
    } else {
        if (op == A_ltOp) return (resl->u.CONST < 0) ? Tr_Ex(T_Const(0)) : Tr_Ex(T_Const(1));
        else return (resl->u.CONST > 0) ? Tr_Ex(T_Const(0)) : Tr_Ex(T_Const(1));
    }
}

Tr_exp Tr_eqRef(A_oper op, Tr_exp left, Tr_exp right) {
    T_relOp opp;
    if (op == A_eqOp) opp = T_eq; else opp = T_ne;
    T_stm cond = T_Cjump(opp, unEx(left), unEx(right), NULL, NULL);
    patchList trues = PatchList(&cond->u.CJUMP.true, NULL);
    patchList falses = PatchList(&cond->u.CJUMP.false, NULL);
    return Tr_Cx(trues, falses, cond);
}

Tr_exp Tr_relExp(A_oper op, Tr_exp left, Tr_exp right) {
    T_relOp oper;
    switch(op) {
        case A_ltOp: oper = T_lt; break;
        case A_leOp: oper = T_le; break;
        case A_gtOp: oper = T_gt; break;
        case A_geOp: oper = T_ge; break;
        default: assert(0);
    }
    T_stm cond = T_Cjump(oper, unEx(left), unEx(right), NULL, NULL);
    patchList trues = PatchList(&cond->u.CJUMP.true, NULL);
    patchList falses = PatchList(&cond->u.CJUMP.false, NULL);
    return Tr_Cx(trues, falses, cond);
}

Tr_exp Tr_ifExp(Tr_exp test, Tr_exp then, Tr_exp elsee) {
    Tr_exp result = NULL;
    Temp_label t = Temp_newlabel(), f = Temp_newlabel(), 
               z = Temp_newlabel(), join = Temp_newlabel();
    struct Cx cond = unCx(test);
    doPatch(cond.trues, t);
    doPatch(cond.falses, f);
    if (!elsee) {
        if (then->kind == Tr_nx) {
            result = Tr_Nx(T_Seq(cond.stm, 
                                 T_Seq(T_Label(t),
                                       T_Seq(then->u.nx, T_Label(f))))); 
        } else if (then->kind == Tr_cx) {
            result = Tr_Nx(T_Seq(cond.stm,
                                 T_Seq(T_Label(t),
                                       T_Seq(then->u.cx.stm, T_Label(f))))); 
        } else { 
            result = Tr_Nx(T_Seq(cond.stm, 
                                 T_Seq(T_Label(t),
                                       T_Seq(T_Exp(unEx(then)), T_Label(f)))));
        }
    } else {
        Temp_temp r = Temp_newtemp();
        T_stm joinJump = T_Jump(T_Name(join), Temp_LabelList(join, NULL));
        T_stm thenStm;
        if (then->kind == Tr_ex) 
            thenStm = T_Exp(then->u.ex);
        else 
            thenStm = (then->kind == Tr_nx) ? then->u.nx : then->u.cx.stm;
        T_stm elseeStm;
        if (elsee->kind == Tr_ex) 
            elseeStm = T_Exp(elsee->u.ex);
        else 
            elseeStm = (elsee->kind == Tr_nx) ? elsee->u.nx : elsee->u.cx.stm;

        if (then->kind != Tr_nx)
            result = Tr_Ex(T_Eseq(cond.stm, 
                                  T_Eseq(T_Label(t), 
                                         T_Eseq(T_Move(T_Temp(r), unEx(then)),
                                                T_Eseq(joinJump,
                                                       T_Eseq(T_Label(f),
                                                              T_Eseq(T_Move(T_Temp(r), unEx(elsee)), 
                                                                     T_Eseq(T_Label(join), T_Temp(r)))))))));
        else
            result = Tr_Nx(T_Seq(cond.stm, 
                                 T_Seq(T_Label(t), 
                                       T_Seq(thenStm,
                                             T_Seq(joinJump, 
                                                   T_Seq(T_Label(f),
                                                         T_Seq(elseeStm, 
                                                               T_Seq(joinJump, T_Label(join))))))))); 
    }
    return result;
}

static Tr_exp Tr_StaticLink(Tr_level now, Tr_level def) {
    T_exp addr = T_Temp(F_EBP());
    while(now && (now != def->parent)) {
        F_access sl = F_formals(now->frame)->head;
        addr = F_Exp(sl, addr);
        now = now->parent;
    }
    return Tr_Ex(addr);
}

static T_expList Tr_expList_convert(Tr_expList l) {
    T_expList h = NULL, t = NULL;
    for (; l; l = l->tail) {
        T_exp tmp = unEx(l->head);
        if (h) {
            t->tail =  T_ExpList(tmp, NULL);
            t = t->tail;
        } else {
            h = T_ExpList(tmp, NULL);
            t = h;
        }   
    }
    return h;
}

Tr_exp Tr_callExp(Temp_label label, Tr_level fun, Tr_level call, Tr_expList * l, Ty_ty result) {
    T_expList args = NULL;
    if (needStaticLink(Temp_labelstring(label)))
        Tr_expList_prepend(Tr_StaticLink(call, fun), l);
    args = Tr_expList_convert(*l);
    if (result->kind == Ty_void) 
        return Tr_Ex(T_Call(T_Name(label), args));
    else
        return Tr_Ex(T_Eseq(T_Exp(T_Call(T_Name(label), args)),T_Temp(F_EAX())));
}

static patchList PatchList(Temp_label * h, patchList t) {
    patchList p = checked_malloc(sizeof(*p));
    p->head = h;
    p->tail = t;
    return p;
}

static void doPatch(patchList t, Temp_label label) {
    for (; t; t = t->tail) {
        *(t->head) = label;
    }
}

static patchList joinPatch(patchList fir, patchList scd) {
    if (!fir) return scd;
    for (; fir->tail; fir = fir->tail);
    fir->tail = scd;
    return fir;
}

F_fragList Tr_getResult() {
    F_fragList cur = NULL, prev = NULL;
    for (cur = fragList; cur; cur = cur->tail)
        prev = cur;
    if (prev) prev->tail = stringFragList;
    return fragList;
}

Tr_level Tr_newLevel(Tr_level p, Temp_label n, U_boolList f) {
    Tr_level l = checked_malloc(sizeof(*l));
    l->parent = p;
    l->name = n;
    l->frame = F_newFrame(n, U_BoolList(TRUE, f));
    l->formals = makeFormalAccessList(l);
    return l;
}

Tr_access Tr_allocLocal(Tr_level l, bool escape) {
    Tr_access a = checked_malloc(sizeof(*a));
    a->level = l;
    a->access = F_allocLocal(l->frame, escape);
    return a;
}

Tr_accessList Tr_AccessList(Tr_access h, Tr_accessList t) {
    Tr_accessList al = checked_malloc(sizeof(*al));
    al->head = h;
    al->tail = t;
    return al;
} 

Tr_accessList Tr_formals(Tr_level l) {
    return l->formals;
}

static Tr_accessList makeFormalAccessList(Tr_level l) {
    Tr_accessList head = NULL, tail = NULL;
    F_accessList  acsl = F_formals(l->frame)->tail;
    for (; acsl; acsl = acsl->tail) {
        Tr_access ac = Tr_Access(l, acsl->head);
        if (head) {
            tail->tail = Tr_AccessList(ac, NULL);
            tail = tail->tail;
        } else {
            head = Tr_AccessList(ac, NULL);
            tail = head;
        }
    }
    return head;
}

static Tr_level outer = NULL;
Tr_level Tr_outermost(void) {
    if (!outer) outer = Tr_newLevel(NULL, Temp_namedlabel("tigermain"), NULL);
    return outer;
}

static Tr_access Tr_Access(Tr_level l, F_access a) {
    Tr_access T_a = checked_malloc(sizeof(*T_a));
    T_a->level  = l;
    T_a->access = a;
    return T_a;
}
