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
static G_table t_degree, t_move;
static G_nodeList simplifyWorklist, freezeWorklist, spillWorklist, worklistMoves,
                  selectStack;

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
        counter c = Counter(G_degree(nlist->head));
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

static void DecrementDegree(G_node n) {
    counter c = G_look(t_degree, n);
    c->num--;
    G_enter(t_degree, n, c);

    if (c->num < k) {
        G_nodeList tmp = G_NodeList(n, G_succ(n));
        EnableMoves(tmp);
        EnableMoves(G_pred(n));

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
