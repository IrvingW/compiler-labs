#ifndef LIVENESS_H
#define LIVENESS_H


typedef struct Live_moveList_ *Live_moveList;
struct Live_moveList_ {
	G_node src, dst;
	Live_moveList tail;
};

Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail);

// operation on two Live_moveList
Live_moveList Live_Union_moveList(Live_moveList l, Live_moveList r);
Live_moveList Live_Sub_moveList(Live_moveList l, Live_moveList r);
Live_moveList Live_Intersect_moveList(Live_moveList l, Live_moveList r);

bool Live_In_moveList(G_node src, G_node dst, Live_moveList list);

struct Live_graph {
	bool *adj_map;
	G_graph graph;
	Live_moveList moves;
	G_table spill_priority;
};

// return the map temp value of a G_node
Temp_temp Live_gtemp(G_node n);

struct Live_graph Live_liveness(G_graph flow);

Temp_tempList Precolored_regs();

#endif
