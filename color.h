/*
 * color.h - Data structures and function prototypes for coloring algorithm
 *             to determine register allocation.
 */
#ifndef COLOR_H
#define COLOR_H

#include "liveness.h"

struct COL_result {
    Temp_map coloring; 
    Temp_tempList spills;
};

struct COL_result COL_color(G_graph ig, Temp_map initial, Temp_tempList regs);

static void Build(struct Live_graph lg, Temp_map initial);

static void AddEdge(G_node u, G_node v);

static void MakeWorklist(G_graph g, Temp_tempList r);

static G_nodeList Adjacent(G_node n);

static AS_instrList NodeMoves(G_node n);

static int MoveRelated(G_node n);

static void Simplify();

static void DecrementDegree(G_node n);

static void EnableMoves(G_nodeList nlist);

static void Coalesce();

static void AddWorkList(G_node n);

static int OK(G_node t, G_node u);

static int Conservative(G_node u, G_node v);

static G_node GetAlias(G_node n);

static void Combine(G_node u, G_node v);

static void Freeze();

static void FreezeMoves(G_node u);

static void SelectSpill();

static void AssignColors();

static void RewriteProgram();

#endif
