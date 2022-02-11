#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "liveness.h"
#include "table.h"

static G_table in, out, conflict;
static Temp_tempList regs;
static TAB_table regMap;

Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
	Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
	lm->src = src;
	lm->dst = dst;
	lm->tail = tail;
	return lm;
}


Temp_temp Live_gtemp(G_node n) {
	return G_nodeInfo(n);
}

Temp_tempList minus(Temp_tempList a, Temp_tempList b) {
	Temp_tempList i, j, r = NULL;
	for (i = a; i; i = i->tail) {
		int found = 0;
		for (j = b; j; j = j->tail)
			if (i->head->num == j->head->num) {
				found = 1;
				break;
			}
		if (!found) r = Temp_TempList(i->head, r);
	}
	return r;
}

Temp_tempList plus(Temp_tempList a, Temp_tempList b) {
	Temp_tempList i, j, r = NULL;
	for (i = a; i; i = i->tail) 
		r = Temp_TempList(i->head, r);
	for (i = b; i; i = i->tail) {
		int found = 0;
		for (j = a; j; j = j->tail)
			if (i->head->num == j->head->num) {
				found = 1;
				break;
			}
		if (!found) r = Temp_TempList(i->head, r);
	}
	return r;
}

static int equal(Temp_tempList a, Temp_tempList b) {
	Temp_tempList i, j;
	
	int amount_a = 0, amount_b = 0;
	for (i = a; i; i = i->tail) amount_a++;
	for (i = b; i; i = i->tail) amount_b++;
	if (amount_a != amount_b) return 0;
	
	for (i = a; i; i = i->tail) {
		int found = 0;
		for (j = b; j; j = j->tail)
			if (i->head->num == j->head->num) {
				found = 1;
				break;
			}
		if (!found) return 0;
	}

	return 1;
}

static void watch_lg(G_graph g) {
	G_nodeList nlist = G_nodes(g);
	for (; nlist; nlist = nlist->tail) {
		G_node n = nlist->head;
		AS_instr inst = G_nodeInfo(n);
		Temp_tempList def = FG_def(n), use = FG_use(n),
					  live_in = G_look(in, n), live_out = G_look(out, n);

		Temp_tempList h = def;
		for (; h; h = h->tail);
		h = use;
		for (; h; h = h->tail);
		h = live_in;
		for (; h; h = h->tail);
		h = live_out;
		for (; h; h = h->tail);
	}
}

static void Liveness_initial(G_graph flow) {
	in = G_empty();
	out = G_empty();
	G_nodeList nodes = G_nodes(flow), n;
	for (n = nodes; n; n = n->tail) {
		G_enter(in, n->head, NULL);
		G_enter(out, n->head, NULL);
	}
}

static void Liveness_Analysis(G_graph flow) {
	Liveness_initial(flow);
	int done = 0;

	while (!done) {
		done = 1;
		G_nodeList nodes = G_nodes(flow), i;
		for (i = nodes; i; i = i->tail) {
			G_node n = i->head;
			Temp_tempList def = FG_def(n), use = FG_use(n),
						  live_in = G_look(in, n), live_out = G_look(out, n);
			Temp_tempList tmp = minus(live_out, def);
			Temp_tempList new_in = plus(use, tmp), new_out = NULL;
			G_nodeList succ = G_succ(n);
			for (; succ; succ = succ->tail) {
				tmp = G_look(in, succ->head);
				new_out = plus(new_out, tmp);
			}

			G_enter(in, n, new_in);
			G_enter(out, n, new_out);
			if (!equal(new_in, live_in) || !equal(new_out, live_out)) done = 0;
		}
	}
}

static void getAllRegs(G_graph g) {
	regs = NULL;
	G_nodeList l = G_nodes(g);
	for (; l; l = l->tail) {
		G_node n = l->head;
		regs = plus(regs, plus(FG_use(n), FG_def(n)));
	}
}

static void Conflict_initial(G_graph g) {
	regMap = TAB_empty();
	Temp_tempList i;
	for (i = regs; i; i = i->tail) {
		G_node n = G_Node(g, i->head);
		TAB_enter(regMap, i->head, n);
	}
}

static G_graph Conflict_Analysis(G_graph flow) {
	G_graph g = G_Graph();
	getAllRegs(flow);
	Conflict_initial(g);
	G_nodeList nlist;
	for (nlist = G_nodes(flow); nlist; nlist = nlist->tail) {
		G_node n = nlist->head;
		AS_instr inst = G_nodeInfo(n);
		Temp_tempList def = FG_def(n);
		for(; def; def = def->tail) {
			Temp_tempList outs = G_look(out, n);
			for (; outs; outs = outs->tail) {
				G_node a = TAB_look(regMap, def->head);
				G_node b = TAB_look(regMap, outs->head);
				if (a != b)
					switch (inst->kind) {
						case I_OPER: 
						case I_LABEL:
							G_addEdge(a, b);
							break;
						case I_MOVE: {
							Temp_tempList use = FG_use(n);
							G_node c = use ? TAB_look(regMap, use->head) : NULL;
							if (b != c) 
								G_addEdge(a, b);
						} break;
						default:
							assert(0);
					}
			}
		}
	}
	return g;
}

static Live_moveList getMoveList(G_graph flow) {
	Live_moveList r = NULL;
	G_nodeList nlist = G_nodes(flow);
	for (; nlist; nlist = nlist->tail) {
		G_node n = nlist->head;
		AS_instr inst = G_nodeInfo(n);
		if (inst->kind == I_MOVE) {
			Temp_tempList def = FG_def(n), use = FG_use(n);
			if (!def || !use) continue;
			G_node dst = TAB_look(regMap, def);
			G_node src = TAB_look(regMap, use);
			r = Live_MoveList(src, dst, r);
		}
	}
	return r;
}

struct Live_graph Live_liveness(G_graph flow) {
	struct Live_graph lg;
	Liveness_Analysis(flow);
	lg.cg = Conflict_Analysis(flow);
	lg.fg = flow;
	lg.moves = getMoveList(flow);
	return lg;
}
