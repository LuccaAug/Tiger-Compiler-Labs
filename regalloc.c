#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "color.h"
#include "liveness.h"
#include "regalloc.h"
#include "table.h"
#include "flowgraph.h"

static TAB_table offs;

static void SetOffset(Temp_temp t, int off) {
    counter c = Counter(off);
    TAB_enter(offs, t, c);
}

static int GetOffset(Temp_temp t) {
    counter c = TAB_look(offs, t);
    return c->num;
}

static int inTempList(Temp_temp t, Temp_tempList l) {
    for (; l; l = l->tail)
        if (t->num == l->head->num)
            return 1;
    return 0;
}

static int needGet(AS_instr i, Temp_temp t) {
    if (i->kind == I_OPER) 
        return inTempList(t, i->u.OPER.src);
    if (i->kind == I_MOVE)
        return inTempList(t, i->u.MOVE.src);
}

static int needPut(AS_instr i, Temp_temp t) {
    if (i->kind == I_OPER) 
        return inTempList(t, i->u.OPER.dst);
    if (i->kind == I_MOVE)
        return inTempList(t, i->u.MOVE.dst);
}

static void show(AS_instrList il) {
    for (; il; il = il->tail) 
        switch (il->head->kind) {
            case I_MOVE:
                printf("%s\n", il->head->u.MOVE.assem);
                break;
            case I_OPER:
                printf("%s\n", il->head->u.OPER.assem);
                break;
            case I_LABEL:
                printf("%s\n", il->head->u.LABEL.assem);
        }
}

static AS_instrList RewriteProgram(AS_instrList il, Temp_tempList spill) {
    offs = TAB_empty();
    Temp_tempList s = spill;
    AS_instrList ilist = il, res = NULL, cur = NULL;
    for (; ilist; ilist = ilist->tail) {
        int c = -50;
        AS_instr inst = ilist->head;
        AS_instrList get = NULL, put = NULL, last = NULL;
        if (ilist->head->kind != I_LABEL) 
            for (s = spill; s; s = s->tail) {
                c += 4;
                Temp_temp t = s->head;
                if (needGet(inst, t)) {
                    AS_instr d = AS_Move(String_format("movl %d(`s0), `d0\n", c),
                        Temp_TempList(t, NULL), Temp_TempList(F_EBP(), NULL));
                    get = AS_InstrList(d, get);
                }
                if (needPut(inst, t)) {
                    AS_instr d = AS_Move(String_format("movl `s0, %d(`s1)\n", c),
                        NULL, Temp_TempList(t, Temp_TempList(F_EBP(), NULL)));                
                    put = AS_InstrList(d, put);
                }
            }
        for (last = get; last; last = last->tail) 
            if (!last->tail)
                break;
        put = AS_InstrList(inst, put);
        if (!get)
            get = put;
        else
            last->tail = put;
        if (!res)
            res = get;
        else
            cur->tail = get;
        for (cur = get; cur->tail; cur = cur->tail);
    }
    il->tail = res->tail;
    return res;
}



struct RA_result RA_regAlloc(F_frame f, AS_instrList il) {
	struct RA_result ret;
	G_graph fg = FG_AssemFlowGraph(il, f);
	Temp_map initial = Temp_layerMap(Temp_empty(), F_tempMap);
	Temp_tempList regs = F_registers();
	struct COL_result col_result = COL_color(fg, initial, regs);
    if (col_result.spills) {
        printf("need rewrite1\n");
        return RA_regAlloc(f, RewriteProgram(il, col_result.spills));
    }
	ret.coloring = col_result.coloring;
	ret.il = il;

	return ret;
}