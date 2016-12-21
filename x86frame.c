#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"

/*Lab5: Your implementation here.*/
const int F_wordSize = 4;
const int F_MAX_REG = 6;
static Temp_temp fp = NULL;

struct F_frame_ {
    int local;
    Temp_label name;
    F_accessList formals;
};

struct F_access_ {
    enum {inFrame, inReg} kind;
    union {
        int offset;
        Temp_temp reg;
    } u;
};

static F_access InFrame(int offs) {
    F_access a = checked_malloc(sizeof(*a));
    a->kind = inFrame;
    a->u.offset = offs;
    return a;
}

static F_access InReg(Temp_temp t) {
    F_access a = checked_malloc(sizeof(*a));
    a->kind = inReg;
    a->u.reg = t;
    return a;
}

F_accessList F_AccessList(F_access head, F_accessList tail) {
    F_accessList f = checked_malloc(sizeof(*f));
    f->head = head;
    f->tail = tail;
    return f;
}

F_frag F_StringFrag(Temp_label label, string str) {
    F_frag f = checked_malloc(sizeof(*f));
    f->kind = F_stringFrag;
    f->u.stringg.label = label;
    f->u.stringg.str = str;
    return f;
}

F_frag F_ProcFrag(T_stm body, F_frame frame) {
    F_frag f = checked_malloc(sizeof(*f));
    f->kind = F_procFrag;
    f->u.proc.body = body;
    f->u.proc.frame = frame;
	return f;
}

F_fragList F_FragList(F_frag head, F_fragList tail) {
    F_fragList f = checked_malloc(sizeof(*f));
    f->head = head;
    f->tail = tail;
	return f;
}

string F_getlabel(F_frame frame) {
    return Temp_labelstring(frame->name);
}

F_access F_allocLocal(F_frame f, bool escape) {
    f->local++;
    if (escape) {
        return InFrame(-F_wordSize * f->local);
    } else {
        return InReg(Temp_newtemp());
    }
}

F_accessList F_formals(F_frame f) {
    return f->formals;
}

static F_accessList makeFormalAccessList(F_frame f, U_boolList formals) {
    U_boolList fmls = formals;
    F_accessList head = NULL, tail = NULL;
    int i = 0;
    for (; fmls; fmls = fmls->tail, i++) {
        F_access ac = NULL;
        if (i < F_MAX_REG && !fmls->head) {
            ac = InReg(Temp_newtemp());
        } else {
            /*keep a space for return*/
            ac = InFrame((i/* + 1*/) * F_wordSize);
        }
        if (head) {
            tail->tail = F_AccessList(ac, NULL);
            tail = tail->tail;
        } else {
            head = F_AccessList(ac, NULL);
            tail = head;
        }
    }
    return head;
}

Temp_label F_name(F_frame f) {
    return f->name;
}

F_frame F_newFrame(Temp_label name, U_boolList formals) {
    F_frame f = checked_malloc(sizeof(*f));
    f->name = name;
    f->formals = makeFormalAccessList(f, formals);
    f->local = 0;
    return f;
}

F_frag F_string(Temp_label lab, string str) {
    F_frag f = checked_malloc(sizeof(*f));
    f->kind = F_stringFrag;
    f->u.stringg.label = lab;
    f->u.stringg.str = str;
    return f;
}

F_frag F_newProcFrag(T_stm body, F_frame frame) {
    F_frag f = checked_malloc(sizeof(*f));
    f->kind = F_procFrag;
    f->u.proc.body = body;
    f->u.proc.frame = frame;
    return f;
}

Temp_temp F_FP(void) {
    if (!fp) 
        fp = Temp_newtemp();
    return fp;
}

T_exp F_Exp(F_access access, T_exp framePtr) {
    if (access->kind == inFrame) 
        return T_Mem(T_Binop(T_plus, framePtr, T_Const(access->u.offset)));
    else 
        return T_Temp(access->u.reg);
}

T_exp F_externalCall(string str, T_expList args) {
    return T_Call(T_Name(Temp_namedlabel(str)), args);
}

T_stm F_procEntryExit1(F_frame frame, T_stm stm) {
    return stm;
}

static Temp_tempList registers = NULL;
static Temp_temp eax = NULL, ebx = NULL, ecx = NULL, edx = NULL, 
                 esi = NULL, edi = NULL, esp = NULL, ebp = NULL;

void precolor() {
    Temp_enter(F_tempMap, eax, "%eax");
    Temp_enter(F_tempMap, ebx, "%ebx");
    Temp_enter(F_tempMap, ecx, "%ecx");
    Temp_enter(F_tempMap, edx, "%edx");
    Temp_enter(F_tempMap, esi, "%esi");
    Temp_enter(F_tempMap, edi, "%edi");
    Temp_enter(F_tempMap, esp, "%ebp");
    Temp_enter(F_tempMap, ebp, "%esp");
}

Temp_tempList F_registers() {
    if (!registers) {
        if (!eax) eax = Temp_newtemp();
        if (!ebx) ebx = Temp_newtemp();
        if (!ecx) ecx = Temp_newtemp();
        if (!edx) edx = Temp_newtemp();
        if (!esi) esi = Temp_newtemp();
        if (!edi) edi = Temp_newtemp();
        if (!esp) esp = Temp_newtemp();
        if (!ebp) ebp = Temp_newtemp();

        registers = Temp_TempList(eax,
                    Temp_TempList(ebx,
                    Temp_TempList(ecx,
                    Temp_TempList(edx,
                    Temp_TempList(esi,
                    Temp_TempList(edi, NULL))))));
    }
    precolor();
    return registers;
}