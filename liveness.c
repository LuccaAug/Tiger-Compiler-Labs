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

static G_table in, out;

Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
	Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
	lm->src = src;
	lm->dst = dst;
	lm->tail = tail;
	return lm;
}


Temp_temp Live_gtemp(G_node n) {
	//your code here.

	return NULL;
}

static Temp_tempList minus(Temp_tempList a, Temp_tempList b) {
	Temp_tempList i, j, r = NULL;
	for (i = a; i; i = i->tail) {
		int found = 0;
		for (j = b; j; j = j->tail)
			if (i->head.num == j->head.num) {
				found = 1;
				break;
			}
		if (!found) r = Temp_TempList(i->head, r);
	}
	return r;
}

static Temp_tempList plus(Temp_tempList a, Temp_tempList b) {
	Temp_tempList i, j, r = NULL;
	for (i = a; i; i = i->tail)
		r = Temp_TempList(i->head, NULL);
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

	G_nodeList nodes = G_nodes(flow), i;
	for (i = nodes; i; i = i->tail) {
		G_node n = i->head;
		Temp_tempList def = FG_def(n);
		Temp_tempList use = FG_use(n);


	}

}

struct Live_graph Live_liveness(G_graph flow) {
	//your code here.
	struct Live_graph lg;
	return lg;
}


