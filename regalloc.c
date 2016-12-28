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

static AS_instrList RewriteProgram(AS_instrList il, Temp_tempList spill) {
    offs = TAB_empty();
    Temp_tempList s = spill;
    // for (; s; s = s->tail) {
    //     printf("rewrite %d %d\n", s->head->num, (++i) * 4);
    //     SetOffset(s->head, (++i) * 4);
    // }
    AS_instrList ilist = il, res = NULL, cur = NULL;
    for (; ilist; ilist = ilist->tail) {
        int c = -64;
        AS_instr inst = ilist->head;
        AS_instrList get = NULL, put = NULL, last = NULL;
        for (s = spill; s; s = s->tail) {
            c += 4;
            Temp_temp t = s->head;
            if (needGet(inst, t)) {
                AS_instr d = AS_Move(String_format("+movl %d(%ebp), `d0\n", c),
                    Temp_TempList(t, NULL), Temp_TempList(F_EBP(), NULL));
                get = AS_InstrList(d, get);
            }
            if (needPut(inst, t)) {
                AS_instr d = AS_Move(String_format("-movl `s0, %d(%ebp)\n", c),
                    NULL, Temp_TempList(t, NULL));                
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
    return res;
}

// static int getOffset(TAB_table table,F_frame f,Temp_temp temp){
//     F_access f_access = TAB_look(table, temp);
//     if (f_access == NULL){
//         f_access = F_allocLocal(f, TRUE);
//         TAB_enter(table, temp, f_access);
//     }
//     return f_access->u.offset * 4;
// }
// static Temp_temp getNewTemp(TAB_table table, Temp_temp oldTemp){
//     Temp_temp newTemp = TAB_look(table, oldTemp);
//     if (newTemp == NULL){
//         newTemp = Temp_newtemp();
//         TAB_enter(table, oldTemp, newTemp);
//     }
//     return newTemp;
// }
// static void rewriteProgram(F_frame f,Temp_tempList temp_tempList,AS_instrList il){
//     printf("rewrite\n");
//     AS_instrList pre = NULL, cur = il;
//         TAB_table tempMapOffset = TAB_empty();
//     while (cur != NULL){
//         AS_instr as_Instr = cur->head;
//         Temp_tempList defTempList = NULL;
//         Temp_tempList useTempList = NULL;
//         switch (as_Instr->kind){
//             case I_OPER:
//                   defTempList = as_Instr->u.OPER.dst;
//                   useTempList = as_Instr->u.OPER.src;
//                   break;
//             case I_MOVE:
//                   defTempList = as_Instr->u.MOVE.dst;
//                   useTempList = as_Instr->u.MOVE.src;
//                   break;
//             default:
//                   break;
//         }
//         if(useTempList!=NULL||defTempList!=NULL){
//             TAB_table oldMapNew = TAB_empty();
//             while (useTempList != NULL){
//                 if (inTempList(useTempList->head, temp_tempList)){
//                     assert(pre);
//                     Temp_temp newTemp = getNewTemp(oldMapNew, useTempList->head);
//                     int offset = getOffset(tempMapOffset,f,useTempList->head);
//                     string instr = String_format("movl %d(`s0),`d0\n", offset);
//                     AS_instr as_instr = AS_Move(instr, Temp_TempList(newTemp,NULL),Temp_TempList(F_EBP(),NULL));
//                                         useTempList->head = newTemp;
//                                         pre = pre->tail = AS_InstrList(as_instr,cur);
//                 }
//                 useTempList = useTempList->tail;
//             }
//             while (defTempList != NULL){
//                 if (inTempList(defTempList->head, temp_tempList)){
//                     assert(pre);
//                     Temp_temp newTemp = getNewTemp(oldMapNew, defTempList->head);
//                     int offset = getOffset(tempMapOffset, f, defTempList->head);
//                     string instr = String_format("movl `s0,%d(`s1)\n", offset);
//                     AS_instr as_instr = AS_Move(instr,NULL, Temp_TempList(newTemp,Temp_TempList(F_EBP(),NULL)));
//                     cur->tail = AS_InstrList(as_instr, cur->tail);
//                                         defTempList->head = newTemp;
//                 }
//                 defTempList = defTempList->tail;
//             }
//         }
//         pre = cur;
//         cur = cur->tail;
//     }
// }


struct RA_result RA_regAlloc(F_frame f, AS_instrList il) {
	//your code here.
	struct RA_result ret;
	G_graph fg = FG_AssemFlowGraph(il, f);
    // {
    //     printf("Flow Graph\n");
    //     G_nodeList nlist = G_nodes(fg);
    //     for (; nlist; nlist = nlist->tail) {
    //         AS_instr inst = G_nodeInlist->head;
    //         string s;
    //         switch (inst->kind) {
    //             case I_OPER:
    //                 s = inst->u.OPER.assem;
    //                 break;
    //             case I_MOVE:
    //                 s = inst->u.MOVE.assem;
    //                 break;
    //             case I_LABEL:
    //                 s = inst->u.LABEL.assem;
    //         }
    //         fprintf(stdout, "%s\n", s);
    //     }
    //     printf("-------------------------------------------\n");
    // }
	Temp_map initial = Temp_layerMap(Temp_empty(), F_tempMap);
	Temp_tempList regs = F_registers();
	struct COL_result col_result = COL_color(fg, initial, regs);
    if (col_result.spills) {
        return RA_regAlloc(f, RewriteProgram(il, col_result.spills));
    }
	ret.coloring = col_result.coloring;
	ret.il = il;

	return ret;
}
