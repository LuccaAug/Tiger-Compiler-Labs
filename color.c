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
#include "memory.h"
#include "liveness.h"
#include "flowgraph.h"

static int k;
static TAB_table tempMap;
static G_table t_degree, t_move, alias, moveList, adjSet, adjList, color;
static G_nodeList coalescedNodes, precolored, coloredNodes, spilledNodes;
static G_nodeList simplifyWorklist, freezeWorklist, spillWorklist, selectStack;
static AS_instrList coalescedMoves, constrainedMoves, frozenMoves, 
                    worklistMoves, activeMoves;

static void initialize() {
    k = 0;
    tempMap = TAB_empty();
    t_degree = G_empty();
    t_move   = G_empty();
    alias    = G_empty();
    moveList = G_empty();
    adjSet   = G_empty();
    adjList  = G_empty();
    color    = G_empty();
    coalescedNodes   = NULL;
    precolored       = NULL;
    coloredNodes     = NULL;
    spilledNodes     = NULL;
    simplifyWorklist = NULL;
    freezeWorklist   = NULL;
    spillWorklist    = NULL;
    selectStack      = NULL;
    coalescedMoves   = NULL;
    constrainedMoves = NULL;
    frozenMoves      = NULL;
    worklistMoves    = NULL;
    activeMoves      = NULL;
}

static int degree(G_node n) {
    counter c = G_look(t_degree, n);
    return c->num;
}

static void PutDegree(G_node n, int i) {
    counter c = Counter(i);
    G_enter(t_degree, n, c);
}

static int GetColor(G_node n) {
    counter c = G_look(color, n);
    return c->num;
}

static int ColorIt(G_node n, int v) {
    counter c = Counter(v);
    G_enter(color, n, c);
}

static int inAdjSet(G_node u, G_node v) {
    G_nodeList nlist = G_look(adjSet, u);
    for (; nlist; nlist = nlist->tail)
        if (nlist->head == v)
            return 1;
    return 0;
}

static void addToAdjSet(G_node u, G_node v) {
    // add (u, v)
    G_nodeList nlist = G_look(adjSet, u);
    nlist = G_NodeList(v, nlist);
    G_enter(adjSet, u, nlist);
    // add (v, u)
    nlist = G_look(adjSet, v);
    nlist = G_NodeList(u, nlist);
    G_enter(adjSet, v, nlist);
}

// add v to adjList[u]
static void addToAdjList(G_node u, G_node v) { 
    G_nodeList nlist = G_look(adjList, u);
    nlist = G_NodeList(v, nlist);
    G_enter(adjList, u, nlist);
}

static void increaseDegree(G_node n) {
    counter c = G_look(t_degree, n);
    c->num++;
    G_enter(t_degree, n, c);
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

static void delMove(AS_instrList *ilist, AS_instr n) {
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

static void makeTempMap(G_graph cg, Temp_map initial) {
    TAB_table tab = initial->tab;
    G_nodeList nlist = G_nodes(cg);
    for (; nlist; nlist = nlist->tail) {
        G_node n = nlist->head;
        PutDegree(n, 0);
        Temp_temp t = G_nodeInfo(n);
        TAB_enter(tempMap, t, n);
        Temp_temp r = TAB_look(tab, t);
        if (r) precolored = G_NodeList(n, precolored);
    }
}

static void count_regs(Temp_tempList r) {
    Temp_tempList tlist = r;
    for (; tlist; tlist = tlist->tail) k++;
}

static int end() {
    return !simplifyWorklist && !worklistMoves &&
           !freezeWorklist   && !spillWorklist;
}

static int inMoves(AS_instr i, AS_instrList ilist) {
    for (; ilist; ilist = ilist->tail)
        if (i == ilist->head) 
            return 1;
    return 0;
}

static void deal_with_neighbors(G_nodeList neighbor) {
    for (; neighbor; neighbor = neighbor->tail) {
        G_node n = neighbor->head;
        DecrementDegree(n);
    }
}

static int Safe(G_node u, G_node v) {
    G_nodeList adj = Adjacent(v);
    for (; adj; adj = adj->tail) {
        G_node t = adj->head;
        if (!OK(t, u)) return 0;
    }
    return 1;
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

struct COL_result COL_color(G_graph fg, Temp_map initial, Temp_tempList regs) {
    //your code here.
    struct COL_result ret;

    struct Live_graph lg = Live_liveness(fg);

    Build(lg, initial);

    MakeWorklist(lg.cg, regs);

    do {
        if (simplifyWorklist)    Simplify();
        else if (worklistMoves)  Coalesce();
        else if (freezeWorklist) Freeze();
        else if (spillWorklist)  SelectSpill();
    } while (!end());

    AssignColors();

    if (spilledNodes) {
    	for (; spilledNodes; spilledNodes = spilledNodes->tail)
    		ret.spills = Temp_TempList(G_nodeInfo(spilledNodes->head), ret.spills);
    }

    Temp_temp regArray[k];
	int i = 0;
	for (; regs; regs = regs->tail) regArray[i++] = regs->head;

    TAB_table tab = TAB_empty();
    G_nodeList nlist = G_nodes(lg.cg);
    for (; nlist; nlist = nlist->tail) {
    	G_node n = nlist->head;
    	TAB_enter(tab, G_nodeInfo(n), regArray[GetColor(n)]);
    }
    ret.coloring = newMap(tab, NULL);

    return ret;
}

static void Build(struct Live_graph lg, Temp_map initial) {
    initialize();
    makeTempMap(lg.cg, initial);
    G_graph fg = lg.fg, cg = lg.cg;
    G_nodeList nlist = G_nodes(fg);
    for (; nlist; nlist = nlist->tail) {
        G_node m = nlist->head;
        AS_instr i = G_nodeInfo(m);
        if (i->kind == I_MOVE) {
            Temp_tempList def = FG_def(m), use = FG_use(m);
            if (!def || !use) continue;
            Temp_tempList tlist = plus(use, def);
            for (; tlist; tlist = tlist->tail) {
                G_node n = TAB_look(tempMap, tlist->head);
                AS_instrList ilist = G_look(moveList, n);
                ilist = AS_InstrList(i, ilist);
                G_enter(moveList, n, ilist);
            }
            addMove(&worklistMoves, i);
        }
    }
    nlist = G_nodes(cg);
    for (; nlist; nlist = nlist->tail) {
        G_node u = nlist->head;
        G_nodeList vlist = G_adj(u);
        for (; vlist; vlist = vlist->tail) 
            AddEdge(u, vlist->head);
    }
}

static void AddEdge(G_node u, G_node v) {
    if (!inAdjSet(u, v) && u != v) {
        addToAdjSet(u, v);
        if (!G_inNodeList(u, precolored)) {
            addToAdjList(u, v);
            increaseDegree(u);
        }
        if (!G_inNodeList(v, precolored)) {
            addToAdjList(v, u);
            increaseDegree(v);
        }
    }
}

static void MakeWorklist(G_graph g, Temp_tempList r) {
    count_regs(r);
    G_nodeList nlist = G_nodes(g);
    for (; nlist; nlist = nlist->tail) {
        G_node n = nlist->head;
        counter c = G_look(t_degree, n);
        if (c->num >= k) 
            spillWorklist = G_NodeList(n, spillWorklist);
        else if (MoveRelated(n))
            freezeWorklist = G_NodeList(n, freezeWorklist);
        else
            simplifyWorklist = G_NodeList(n, simplifyWorklist);
    }
}

static G_nodeList Adjacent(G_node n) {
    G_nodeList nlist = NULL, adj = G_look(adjList, n);
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

static AS_instrList NodeMoves(G_node n) {
    AS_instrList r = NULL,
                 ilist = G_look(moveList, n);
    
    for (; ilist; ilist = ilist->tail) {
        int found = 0;
        AS_instr i = ilist->head;
        AS_instrList j = activeMoves;
        for (; j; j = j->tail) 
            if (i == j->head) {
                found = 1;
                break;
            }
        if (found) goto ADD;
        for (j = worklistMoves; j; j = j->tail)
            if (i == j->head) {
                found = 1;
                break;
            }
        if (!found) continue;
        ADD:
        r = AS_InstrList(i, r);
    }

    return r;
}

static int MoveRelated(G_node n) {
    return NodeMoves(n) != NULL;
}

static void Simplify() {
    G_node n = simplifyWorklist->head;
    simplifyWorklist = simplifyWorklist->tail;

    selectStack = G_NodeList(n, selectStack);

    deal_with_neighbors(G_succ(n));
    deal_with_neighbors(G_pred(n));
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

static void EnableMoves(G_nodeList nlist) {
    for (; nlist; nlist = nlist->tail) {
        G_node n = nlist->head;
        AS_instrList ilist = NodeMoves(n);
        for (; ilist; ilist = ilist->tail) {
            AS_instr m = ilist->head;
            if (inMoves(m, activeMoves)){
                delMove(&activeMoves, m);
                addMove(&worklistMoves, m);
            }
        }
    }
}

static void Coalesce() {
    AS_instr m = worklistMoves->head;
    delMove(&worklistMoves, m);
    G_node x = GetAlias(TAB_look(tempMap, m->u.MOVE.dst->head));
    G_node y = GetAlias(TAB_look(tempMap, m->u.MOVE.src->head));
    G_node u, v;
    if (G_inNodeList(y, precolored))
        u = y, v = x;
    else
        u = x, v = y;
    if (u == v) {
        addMove(&coalescedMoves, m);
        AddWorkList(u);
    }
    else if (G_inNodeList(v, precolored) || inAdjSet(u, v)) {
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

static G_node GetAlias(G_node n) {
    if (G_inNodeList(n, coalescedNodes)) {
        G_node m = G_look(alias, n);
        return GetAlias(m);
    }
    return n;
}

static void Combine(G_node u, G_node v) {
    if (G_inNodeList(v, freezeWorklist))
        delNode(&freezeWorklist, v);
    else
        delNode(&spillWorklist, v);

    addNode(&coalescedNodes, v);
    G_enter(alias, v, u);
    combine_moveList(u, v);
    EnableMoves(G_NodeList(v, NULL));
    G_nodeList adj = Adjacent(v);
    for (; adj; adj = adj->tail) {
        G_node t = adj->head;
        AddEdge(t, u);
        DecrementDegree(t);
    }
    counter c = G_look(t_degree, u);
    if (c->num >= k && G_inNodeList(u, freezeWorklist)) {
        delNode(&freezeWorklist, u);
        addNode(&spillWorklist, u);
    }
}

static void Freeze() {
    G_node u = freezeWorklist->head;
    delNode(&freezeWorklist, u);
    addNode(&simplifyWorklist, u);
    FreezeMoves(u);
}

static void FreezeMoves(G_node u) {
    AS_instrList ilist = NodeMoves(u);
    for (; ilist; ilist = ilist->tail) {
        AS_instr m = ilist->head;
        G_node x = TAB_look(tempMap, m->u.MOVE.dst->head),
               y = TAB_look(tempMap, m->u.MOVE.src->head), v;
        if (GetAlias(y) == GetAlias(u)) 
            v = GetAlias(x);
        else
            v = GetAlias(y);
        delMove(&activeMoves, m);
        addMove(&frozenMoves, m);

        if (!NodeMoves(v) && degree(v) < k) {
            delNode(&freezeWorklist, v);
            addNode(&simplifyWorklist, v);
        }
    }
}

static void SelectSpill() {
    G_node m = spillWorklist->head;
    delNode(&spillWorklist, m);
    addNode(&simplifyWorklist, m);
    FreezeMoves(m);
}

static void AssignColors() {
    while (selectStack) {
        G_node n = selectStack->head;
        delNode(&selectStack, n);
        int okColors[k];
        memset(okColors, 0, sizeof(okColors));
        G_nodeList nlist = G_look(adjList, n);
        for (; nlist; nlist = nlist->tail) {
            G_node w = GetAlias(nlist->head);
            if (G_inNodeList(w, coloredNodes) ||
                G_inNodeList(w, precolored)) {
                okColors[GetColor(w)] = 1;
            }
        }
        
        int i, available = k;
        for (i = 0; i < k; i++) available -= okColors[i];

        if (available == 0)
            addNode(&spilledNodes, n);
        else {
            addNode(&coloredNodes, n);
            for (i = 0; i < k; i++)
                if (!okColors[i]) {
                    ColorIt(n, i);
                    break;
                }
        }
    }
    
    G_nodeList nlist = coalescedNodes;
    for (; nlist; nlist = nlist->tail) {
        G_node n = nlist->head, a = GetAlias(n);
        ColorIt(n, GetColor(a));
    }
}

static void RewriteProgram() {
    //TO DO:
}