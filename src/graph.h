/*
 * Tipo abstrato de dados que representam um grafo dirigido
 */

typedef struct G_graph_ *G_graph;
typedef struct G_node_ *G_node;

typedef struct G_nodeList_ *G_nodeList;
struct G_nodeList_ { G_node head; G_nodeList tail;};

G_graph G_Graph(void); 
G_node G_Node(G_graph g, void *info);

G_nodeList G_NodeList(G_node head, G_nodeList tail);

G_nodeList G_nodes(G_graph g);

bool G_inNodeList(G_node a, G_nodeList l);

void G_addEdge(G_node from, G_node to);

void G_rmEdge(G_node from, G_node to);

void G_show(FILE *out, G_nodeList p, void showInfo(void *));

G_nodeList G_succ(G_node n);

G_nodeList G_pred(G_node n);

bool G_goesTo(G_node from, G_node n);

int G_degree(G_node n);

G_nodeList G_adj(G_node n);

void *G_nodeInfo(G_node n);

typedef struct TAB_table_  *G_table;

G_table G_empty(void);

void G_enter(G_table t, G_node node, void *value);

void *G_look(G_table t, G_node node);

typedef struct G_edge_ *G_edge;
struct G_edge_{
    G_node src, dst;
};
G_edge G_Edge(G_node, G_node);

typedef struct G_edgeList_ *G_edgeList;
struct G_edgeList_ {
    G_edge head;
    G_edgeList tail;
};
G_edgeList G_EdgeList(G_edge, G_edgeList);

typedef struct counter_ *counter;

struct counter_ {
    int num;
};

counter Counter(int);