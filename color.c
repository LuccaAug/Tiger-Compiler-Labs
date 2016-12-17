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
#include "table.h"
#include "counter.h"
#include "liveness.h"

static int k;
static G_table t_degree, t_move, alias, moveList, adjSet, adjList;
static G_nodeList coalescedNodes, precolored;
static G_nodeList simplifyWorklist, freezeWorklist, spillWorklist, selectStack;
static AS_instrList coalescedMoves, constrainedMoves, frozenMoves, 
                    worklistMoves, activeMoves;


static int inAdjSet(G_node u, G_node v) {
    G_nodeList adj = G_adj(u);
    for (; adj; adj = adj->tail)
        if (adj->head == v)
            return 1;
    return 0;
}

static void addEdge(G_node u, G_node v) {
    if (!inAdjSet(u, v) && u != v) {
        G_addEdge(u, v);
        G_addEdge(v, u);
        if (!G_inNodeList(u, precolored)) {

        }
    }
}

static void addNode(G_nodeList *nlist, G_node n) {
    *nlist = G_NodeList(n, *nlist);
}

 static void delNode(G_nodeList *nlist, G_node n) {
    G_nodeList i = *nlist, pre = NULL;
    for (; i; i = i->tail) {
        if (i->head == n) {
            if (pre)
                pre->tail = i->tail;
            else
                *nlist = (*nlist)->tail;
            break;
        }
        pre = i;
    }
 }

 static void addMove(AS_instrList *ilist, AS_instr i) {
    *ilist = AS_InstrList(i, *ilist);
}

 static void delMove(AS_instrList *ilist, AS_instr i) {
    AS_instrList i = *ilist, pre = NULL;
    for (; i; i = i->tail) {
        if (i->head == n) {
            if (pre)
                pre->tail = i->tail;
            else
                *ilist = (*ilist)->tail;
            break;
        }
        pre = i;
    }
 }

static void increase_move(G_node n) {
    counter c = G_look(t_move, n);
    if (!c) 
        c = Counter(1);
    else
        c->num++;
    G_enter(t_move, n, c);
}

static void initial(Live_graph lg) {
    t_degree = G_empty();
    t_move   = G_empty();
        
    G_graph g = lg.graph;
    G_nodeList nlist = G_nodes(g);
    for (; nlist; nlist = nlist->tail) {
        G_node n = nlist->head;
        counter c = Counter(G_degree(n));
        G_enter(t_degree, n, c);
    }

    Live_moveList mlist = lg.moves;
    for (; mlist; mlist = mlist->tail) {
        G_node src = mlist->src;
        G_node dst = mlist->dst;
        increase_move(src);
        increase_move(dst);
    }
}

static void count_regs(Temp_tempList r) {
    k = 0;
    Temp_tempList tlist = r;
    for (; tlist; tlist = tlistt->tail) k++;
}

static int MoveRelated(G_node n) {

}

static void makeWorklist(G_graph g, Temp_tempList r) {
    selectStack = NULL;
    count_regs(r);
    G_nodeList nlist = G_nodes(g);
    for (; nlist; nlist = nlist->tail) {
        G_node n = nlist->head;
        counter c = G_look(t_degree, n);
        if (c->num >= k) 
            spillWorklist = G_nodeList(n, spillWorklist);
        else if (MoveRelated(n))
            freezeWorklist = G_nodeList(n, freezeWorklist);
        else
            simplifyWorklist = G_nodeList(n, simplifyWorklist);
    }
}

static int end() {
    return !simplifyWorklist && !worklistMoves &&
           !freezeWorklist   && !spillWorklist;
}

static AS_instrList NodeMoves(G_node n) {

}

static int inMoves(AS_instr i, AS_instrList ilist) {
    for (; ilist; ilist = ilist->tail)
        if (i == ilist->head) 
            return 1;
    return 0;
}

static void EnableMoves(G_nodeList nlist) {
    for (; nlist; nlist = nlist->tail) {
        G_node n = nlist->head;
        AS_instrList ilist = NodeMoves(n);
        for (; ilist; ilist = ilist->tail) {
            AS_instr m = ilist->head;
            if (inMoves(m, activeMoves)){
                delMove(activeMoves, m);
                addMove(worklistMoves, m);
            }
        }
    }
}

static G_nodeList Adjacent(G_node n) {
    G_nodeList nlist = NULL, adj = G_adj(n);
    for (; adj; adj = adj->tail) {
        G_nodeList i;
        int found = 0;
        G_node m = adj->head;
        for (i = selectStack; i; i = i->tail)
            if (i->head == m) {
                found = 1;
                break;
            }
        if (found) continue;
        for (i = coalescedNodes; i; i = i->tail)
            if (i->head == m) {
                found = 1;
                break;
            }
        if (found) continue;
        nlist = G_NodeList(m, nlist);
    }
    return nlist;
}

static void DecrementDegree(G_node n) {
    counter c = G_look(t_degree, n);
    c->num--;
    G_enter(t_degree, n, c);

    if (c->num < k) {
        G_nodeList tmp = G_NodeList(n, Adjacent(n));
        EnableMoves(tmp);
        if (MoveRelated(n))
            addNode(&freezeWorklist, n);
        else
            addNode(&simplifyWorklist, n);
    }
}

static void deal_with_neighbors(G_nodeList neighbor) {
    for (; neighbor; neighbor = neighbor->tail) {
        G_node n = neighbor->head;
        DecrementDegree(n);
    }
}

static void simplify() {
    G_node n = simplifyWorklist->head;
    simplifyWorklist = simplifyWorklist->tail;

    selectStack = G_nodeList(n, selectStack);

    deal_with_neighbors(G_succ(n));
    deal_with_neighbors(G_pred(n));
}

static G_node GetAlias(G_node n) {
    if (G_inNodeList(n, coalescedNodes)) {
        G_node m = G_look(alias, n);
        return GetAlias(m);
    }
    return n;
}

static void AddWorkList(G_node n) {
    counter c = G_look(t_degree, n);
    if (!G_inNodeList(n, precolored) && !MoveRelated(n) && c->num < k) {
        delNode(&freezeWorklist, n);
        addNode(&simplifyWorklist, n);
    }
}

static int OK(G_node t, G_node u) {
    counter c = G_look(t_degree, t);
    return c->num < k || G_inNodeList(t, precolored) || inAdjSet(t, u);
}

static int Safe(G_node u, G_node v) {
    G_nodeList adj = Adjacent(v);
    for (; adj; adj = adj->tail) {
        G_node t = adj->head;
        if (!OK(t, u)) return 0;
    }
    return 1;
}

static int Conservative(G_node u, G_node v) {
    int kk = 0;
    G_nodeList uadj = Adjacent(u), vadj = Adjacent(v);
    for (; uadj; uadj = uadj->tail) {
        counter c = G_look(t_degree, uadj->head);
        if (c->num > k) kk++;
    }
    for (; vadj; vadj = vadj->tail) {
        counter c = G_look(t_degree, vadj->head);
        if (c->num > k) k++;
    }
    return kk < k;
}

static combine_moveList(G_node u, G_node v) {
    AS_instrList last = NULL,
                 ulist = G_look(moveList, u),
                 vlist = G_look(moveList, v);

    for (last = ulist; last; last = last->tail)
        if (!last->tail)
            break;

    if (!last)
        G_enter(moveList, u, vlist);
    else {
        last->tail = vlist;
        G_enter(moveList, u, ulist);
    }
}

static void Combine(G_node u, G_node v) {
    if (G_inNodeList(v, freezeWorklist))
        delNode(&freezeWorklist, v);
    else
        delNode(&spillWorklist, v);

    addNode(&coalescedNodes, v);
    G_enter(alias, v, u);
    combine_moveList(u, v);
    EnableMoves(v);
    G_nodeList adj = Adjacent(v);
    for (; adj; adj = adj->tail) {
        G_node t = adj->head;
        addEdge(t, u);
        DecrementDegree(t);
    }
    counter c = G_look(t_degree, u);
    if (c->num >= k && G_inNodeList(u, freezeWorklist)) {
        delNode(&freezeWorklist, u);
        addNode(&spillWorklist, u);
    }
}

static void coalesce() {
    AS_instr m = worklistMoves->head;
    delMove(&worklistMoves, m);
    G_node x = GetAlias(m->u.MOVE.dst->head);
    G_node y = GetAlias(m->u.MOVE.src->head);
    G_node u, v;
    if (G_inNodeList(y, precolored))
        u = y, v = x;
    else
        u = x, v = y;
    if (u == v) {
        addMove(coalescedMoves, m);
        AddWorkList(u);
    }
    else if (G_inNodeList(v, precolored) || adjacent(u, v)) {
        addMove(&constrainedMoves, m);
        AddWorkList(u);
        AddWorkList(v);
    }
    else if ((G_inNodeList(u, precolored) && Safe(u, v)) ||
            (!G_inNodeList(u, precolored) && Conservative(u, v))) {
        addMove(&coalescedMoves, m);
        Combine(u, v);
        AddWorkList(u);
    }
    else
        addMove(&activeMoves, m);
}


struct COL_result COL_color(Live_graph lg, Temp_map initial, Temp_tempList regs) {
	//your code here.
	struct COL_result ret;

    initial(lg);

    makeWorklist(lg.graph, regs);

    do {
        if (simplifyWorklist)    simplify();
        else if (worklistMoves)  coalesce();
        else if (freezeWorklist) freeze();
        else if (spillWorklist)  select_spill();
    } while (!end());



	return ret;
}
