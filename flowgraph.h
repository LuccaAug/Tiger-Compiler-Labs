/*
 * flowgraph.h - Protótipo das funções que representam o grafo de fluxo de controle
 */
#ifndef FLOWGRAPH_H
#define FLOWGRAPH_H

Temp_tempList FG_def(G_node n);
Temp_tempList FG_use(G_node n);
bool FG_isMove(G_node n);
G_graph FG_AssemFlowGraph(AS_instrList il, F_frame f);

#endif