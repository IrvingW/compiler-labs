#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "liveness.h"
#include "color.h"
#include "regalloc.h"
#include "table.h"
#include "flowgraph.h"

#define K 6		// eax ebx ecx edx esi edi
// array of char pointer
static char * reg_names[7] = {"unused", "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi"};

// data structure

static G_nodeList precolored;			// machine register, preassigned a color
static G_nodeList initial;				// temporary register, not precolored and not yet processed
static G_nodeList simplifyWorklist;		// list of low-degree non-move-related nodes
static G_nodeList freezeWorklist;		// low-degree move related nodes
static G_nodeList spillWorklist;		// high-degree nodes
static G_nodeList spilledNodes;			// nodes marked for spilling during this round; initially empty
static G_nodeList coalescedNodes;		// registers that have been coalesced; when u<-v is coalesced, v is added to this set and u put back on some work-list
static G_nodeList coloredNodes;			// nodes successfully colored		
static G_nodeList selectStack;			// stack containing temporaries removed from the graph

// move list
static Live_moveList coalescedMoves;	// moves that have been coalesced
static Live_moveList constrainedMoves;	// moves whose source and target interfere
static Live_moveList frozenMoves;		// moves that will no longer be considerd for coalescing
static Live_moveList worklistMoves;		// moves enableed for possible coalescing
static Live_moveList activeMoves;		// moves not yet ready for coalescing

static bool * adjSet;					// if (u, v) is true, then (v, u) is true also;
static G_table adjList;					// do not used for precolored nodes
static G_table degree;					// containing the current degree of each code
static G_table moveList;				// a mapping from a node to the list of moves it is asscociated with
static G_table alias;					// when move (u,v) has been coalesced, and v put in coalescedNodes, then alias(v) = u
static G_table color;					// the color chosen by the algorithm for a node; for precolored nodes this is initialized to the given color

// used to decide which node to spill each turn
static G_table spill_priority;

// conflict graph
static G_graph conflict_graph;
 
// initialize all data structure use live_graph
static void Build(struct Live_graph lg){
	// ues lg.adj_map and spill_priority and graph
	adjSet = lg.adj_map;
	spill_priority = lg.spill_priority;
	conflict_graph = lg.graph;

	degree = G_empty();
	alias = G_empty();
	color = G_empty();
	adjList = G_empty();
	moveList = G_empty();

	// go through all nodes in conflict graph
	G_nodeList all_nodes = G_nodes(lg.graph);
	for(; all_nodes != NULL; all_nodes = all_nodes->tail){
		// initialize degree table and adjList table
		G_node cur_node = all_nodes->head;
		// all edge between nodes are double direct except precolored nodes
		int *d = (int *)checked_malloc(sizeof(int));
		G_nodeList succ_list = G_succ(cur_node);
		G_nodeList adj_nodes = succ_list;	// adj_nodes could include precolored node
		G_enter(adjList, cur_node, adj_nodes);
		
		for(; succ_list; succ_list = succ_list->tail){
			*d = *d + 1;
		}
		G_enter(degree, cur_node, d);

		// initialize color table and precolored and initial
		// normal node's color is initialized with 0
		int *c = (int *) checked_malloc(sizeof(int));
		Temp_temp temp = Live_gtemp(cur_node);
		if(temp == F_eax()){
			*c = 1;
			precolored = G_NodeList(cur_node, precolored);
		}else if(temp == F_ebx()){
			*c = 2;
			precolored = G_NodeList(cur_node, precolored);
		}else if(temp == F_ecx()){
			*c = 3;
			precolored = G_NodeList(cur_node, precolored);
		}else if(temp == F_edx()){
			*c = 4;
			precolored = G_NodeList(cur_node, precolored);
		}else if(temp == F_esi()){
			*c = 5;
			precolored = G_NodeList(cur_node, precolored);
		}else if(temp == F_edi()){
			*c = 6;
			precolored = G_NodeList(cur_node, precolored);
		}else{
			*c = 0;
			initial = G_NodeList(cur_node, initial);	// put all normal nodes into initial set
		}
		G_enter(color, cur_node, c);

		// initialize alias using itself
		G_enter(alias, cur_node, cur_node);

		// use lg.moves initialize moveList
		Live_moveList moves = lg.moves;
		
		Live_moveList relate_move = NULL;
		while(moves){
			if((moves->src == cur_node) || (moves->dst == cur_node)){
				// the move instruction relates to cur_node
				relate_move = Live_MoveList(moves->src, moves->dst, relate_move);
			}
			moves = moves->tail;
		}
		G_enter(moveList, cur_node, relate_move);
	}
	// initialize data structures
	spillWorklist = NULL;
	spilledNodes = NULL;
	coalescedNodes = NULL;
	coloredNodes = NULL;
	selectStack = NULL;

	coalescedMoves = NULL;
	constrainedMoves = NULL;
	frozenMoves = NULL;
	worklistMoves = NULL;
	activeMoves = NULL;
}

static Live_moveList NodeMoves(G_node n){
	Live_moveList result = NULL;
	Live_moveList moves_union = Live_Union_moveList(activeMoves, worklistMoves);
	Live_moveList moveList_n = (Live_moveList) G_look(moveList, n);
	result = Live_Intersect_moveList(moveList_n, moves_union);
	return result;
}

static bool MoveRelated(G_node n){
	return (NodeMoves(n) != NULL);
}

void MakeWorklist(){
	for(; initial != NULL; initial = initial->tail){
		G_node n = initial->head;
		int d_n = *(int *)G_look(degree, n);
		if(d_n >= K){
			spillWorklist = G_NodeList(n, spillWorklist);
		}else if(MoveRelated(n)){
			freezeWorklist = G_NodeList(n, freezeWorklist);
		}else{
			simplifyWorklist = G_NodeList(n, simplifyWorklist);
		}

	}
}

static void EnableMoves(G_nodeList nodes){
	for(; nodes != NULL; nodes = nodes->tail){
		G_node n = nodes->head;
		Live_moveList nodeMoves = NodeMoves(n);
		for(; nodeMoves != NULL; nodeMoves = nodeMoves->tail){
			if(Live_In_moveList(nodeMoves->src, nodeMoves->dst, activeMoves)){
				activeMoves = Live_Sub_moveList(activeMoves, 
												Live_MoveList(nodeMoves->src, nodeMoves->dst, NULL)
												);
				worklistMoves = Live_MoveList(nodeMoves->src, nodeMoves->dst, worklistMoves);
			}
		}
	}
}


static G_nodeList Adjacent(G_node n){
	G_nodeList u = G_Union_nodeList(selectStack, coalescedNodes);
	G_nodeList adjList_n = (G_nodeList) G_look(adjList, n);
	return G_Sub_nodeList(adjList_n, u);
}

static void DecrementDegree(G_node n){
	int *d_n = (int *) G_look(degree, n);
	int deg = *d_n;
	*d_n = deg -1;
	if(deg == K){
		EnableMoves(G_NodeList(n, Adjacent(n)));
		spillWorklist = G_Sub_nodeList(spillWorklist, G_NodeList(n, NULL));
		if(MoveRelated(n)){
			freezeWorklist = G_NodeList(n, freezeWorklist);
		}else{
			simplifyWorklist = G_NodeList(n, simplifyWorklist);
		}
	}

}


void Simplify(){
	G_node n = simplifyWorklist->head;
	simplifyWorklist = simplifyWorklist->tail;
	selectStack = G_NodeList(n, selectStack);
	G_nodeList adj_n = Adjacent(n);
	for(; adj_n != NULL; adj_n = adj_n->tail){
		G_node m = adj_n->head;
		DecrementDegree(m);
	}
	
}


static bool if_adj(int node_key_a, int node_key_b){
	bool * adj = adjSet + (G_graph_nodecount(conflict_graph) * node_key_a + node_key_b);
	return *adj;
}

static bool * adj_cell(int node_key_a, int node_key_b){
	bool * cell = adjSet + (G_graph_nodecount(conflict_graph) * node_key_a + node_key_b);
	return cell;
}

static void AddEdge(G_node u, G_node v){
	bool adj_uv = if_adj(G_node_key(u), G_node_key(v));
	if((!adj_uv) && (u != v)){
		bool * cell_uv = adj_cell(G_node_key(u), G_node_key(v));
		bool * cell_vu = adj_cell(G_node_key(v), G_node_key(u));
		*cell_uv = TRUE;
		*cell_vu = TRUE;
		
		G_nodeList adj_list_u = (G_nodeList) G_look(adjList, u);
		G_nodeList adj_list_v = (G_nodeList) G_look(adjList, v);
		int *degree_u = (int *) G_look(degree, u);
		int *degree_v = (int *) G_look(degree, v);
		if(!G_In_nodeList(u, precolored)){
			G_enter(adjList, u, G_NodeList(v, adj_list_u));
			*degree_u = *degree_u + 1;
		}
		if(!G_In_nodeList(v, precolored)){
			G_enter(adjList, v, G_NodeList(u, adj_list_v));
			*degree_v = *degree_v + 1;
		}

	}
}

static void Combine(G_node u, G_node v){
	if(G_In_nodeList(v, freezeWorklist)){
		freezeWorklist = G_Sub_nodeList(freezeWorklist, G_NodeList(v, NULL));
	}else{
		spillWorklist = G_Sub_nodeList(spillWorklist, G_NodeList(v, NULL));
	}
	coalescedNodes = G_NodeList(v, coalescedNodes);
	G_enter(alias, v, u);
	Live_moveList u_moveList = (Live_moveList) G_look(moveList, u);
	Live_moveList v_moveList = (Live_moveList) G_look(moveList, v);
	G_enter(moveList, u, Live_Union_moveList(u_moveList, v_moveList));
	EnableMoves(G_NodeList(v, NULL));

	G_nodeList v_adj_list = Adjacent(v);
	for(; v_adj_list != NULL; v_adj_list = v_adj_list->tail){
		G_node t = v_adj_list->head;
		AddEdge(t, u);
		DecrementDegree(t);
	}
	int d_u = *(int *)G_look(degree, u);
	if(d_u >= K && G_In_nodeList(u, freezeWorklist)){
		freezeWorklist = G_Sub_nodeList(freezeWorklist, G_NodeList(u, NULL));
		spillWorklist = G_NodeList(u, spillWorklist);
	}
}

static bool Conservative(G_nodeList list){
	int k = 0;
	for(; list != NULL; list = list->tail){
		G_node node = list->head;
		int d_n = *(int *) G_look(degree, node);
		if(d_n >= K)
			k ++;
	}	
	return (k < K);
}

static bool OK(G_node l, G_node r){
	int d_l = *(int *) G_look(degree, l);
	bool adj_lr = if_adj(G_node_key(l), G_node_key(r));
	return (d_l < K || G_In_nodeList(l, precolored) || adj_lr);
}

static void AddWorkList(G_node n){
	int degree_n = *(int *)G_look(degree, n);
	if( (!G_In_nodeList(n, precolored)) && (!MoveRelated(n)) && (degree_n < K) ){
		freezeWorklist = G_Sub_nodeList(freezeWorklist, G_NodeList(n, NULL));
		simplifyWorklist = G_NodeList(n, simplifyWorklist);
	}
}

static G_node GetAlias(G_node n)
{
	if(G_In_nodeList(n, coalescedNodes)){
		G_node a_n = (G_node) G_look(alias, n);
		return GetAlias(a_n);
	}else{
		return n;
	}
}

void Coalesce(){
	// copy(x, y)
	G_node src = worklistMoves->src;
	G_node dst = worklistMoves->dst;
	G_node x = GetAlias(src);
	G_node y = GetAlias(dst);
	G_node u, v;
	if(G_In_nodeList(y, precolored)){
		u = y;
		v = x;
	}else{
		u = x;
		v = y;
	}
	worklistMoves = worklistMoves->tail;

	bool adj_uv = if_adj(G_node_key(u), G_node_key(v));
	if(u == v){
		coalescedMoves = Live_MoveList(src, dst, coalescedMoves);
		AddWorkList(u);
	}else if(G_In_nodeList(v, precolored) || adj_uv){
		constrainedMoves = Live_MoveList(src, dst, constrainedMoves);
		AddWorkList(u);
		AddWorkList(v);
	}else{
		bool ok_tu = TRUE;
		G_nodeList adj_list = Adjacent(v);
		while(ok_tu && adj_list != NULL){
			if(!OK(adj_list->head, u)){
				ok_tu = FALSE;
			}
			adj_list = adj_list->tail;
		}
		G_nodeList adj_union = G_Union_nodeList(Adjacent(u), Adjacent(v));
		if((G_In_nodeList(u, precolored) && ok_tu)
			|| (!G_In_nodeList(u, precolored) && Conservative(adj_union))
			){
				coalescedMoves = Live_MoveList(src, dst, coalescedMoves);
				Combine(u, v);
				AddWorkList(u);
			}
		else
			activeMoves = Live_MoveList(src, dst, activeMoves);
	}

}

static void FreezeMoves(G_node u){
	Live_moveList moves = NodeMoves(u);
	for(; moves; moves = moves->tail){
		G_node x = moves->src;
		G_node y = moves->dst;
		G_node v;
		if(GetAlias(y) == GetAlias(u)){
			v = GetAlias(x);
		}else{
			v = GetAlias(y);
		}
		activeMoves = Live_Sub_moveList(activeMoves, Live_MoveList(x, y, NULL));
		frozenMoves = Live_MoveList(x, y, frozenMoves);
		
		int degree_v = *(int *)G_look(degree, v);
		if(NodeMoves(v) == NULL && degree_v < K){
			freezeWorklist = G_Sub_nodeList(freezeWorklist, G_NodeList(v, NULL));
			simplifyWorklist = G_NodeList(v, simplifyWorklist);
		}
	}
}

void Freeze(){
	G_node u = freezeWorklist->head;
	freezeWorklist = freezeWorklist->tail;
	simplifyWorklist = G_NodeList(u, simplifyWorklist);
	FreezeMoves(u);
}

void SelectSpill(){
	G_node m = spillWorklist->head;
	int max = *(int *) G_look(spill_priority, m);
	for(G_nodeList list = spillWorklist->tail; list; list = list->tail){
		int priority = *(int *) G_look(spill_priority, list->head);
		if(priority > max){
			max = priority;
			m = list->head;
		}
	}

	spillWorklist = G_Sub_nodeList(spillWorklist, G_NodeList(m, NULL));
	simplifyWorklist = G_NodeList(m, simplifyWorklist);
	FreezeMoves(m);
}

void AssignColors(){	
	while(selectStack != NULL){
		G_node cur = selectStack->head;
		selectStack = selectStack->tail;

		bool used [K + 1];
		for(int i = 1; i < K + 1; i ++){
			used[i] = FALSE;
		}
		G_nodeList cur_adj = G_succ(cur);
		for(; cur_adj != NULL; cur_adj = cur_adj->tail){
			int c = *(int *)G_look(color, GetAlias(cur_adj->head));
			used[c] = TRUE;
		}
		bool assigned = FALSE;
		for(int i = 1; i < K + 1; i++){
			if(!used[i]){
				int *assign_color = (int *)G_look(color, cur);
				*assign_color = i;
				assigned = TRUE;
				coloredNodes = G_NodeList(cur, coloredNodes);
			}
		}
		if(!assigned){
			spilledNodes = G_NodeList(cur, spilledNodes);
		}
	}
	// assign color to coalesced nodes
	G_nodeList coalesced_list = coalescedNodes;
	for(; coalesced_list != NULL; coalesced_list = coalesced_list->tail){
		int alias_color = *(int *)G_look(color, GetAlias(coalesced_list->head));
		int *c = (int *)G_look(color, coalesced_list->head);
		*c = alias_color;
	}

}

static Temp_tempList * instr_use(AS_instr instr){
	switch(instr->kind){
		case I_OPER:
			return &instr->u.OPER.src;
		case I_LABEL:
			return NULL;
		case I_MOVE:
			return &instr->u.MOVE.src;
		default:
			assert(0);	// code should not reach here
	}
	return NULL;
}

static Temp_tempList * instr_def(AS_instr instr){
	switch(instr->kind){
		case I_OPER:
			return &instr->u.OPER.dst;
		case I_LABEL:
			return NULL;
		case I_MOVE:
			return &instr->u.MOVE.dst;
		default:
			assert(0);	// code should not reach here
	}
	return NULL;

}

void RewriteProgram(F_frame f, AS_instrList *instr_list){
	AS_instrList result = *instr_list;
	while(spilledNodes){
		G_node cur = spilledNodes->head;
		spilledNodes = spilledNodes->tail;
		Temp_temp spill_temp = Live_gtemp(cur);

		int offset_in_frame = F_Spill(f);
		AS_instrList last_instr = NULL;
		AS_instrList next_instr = NULL;
		AS_instrList list = result;
		
		while(list){
			next_instr = list->tail;
			Temp_tempList *use_list = instr_use(list->head);
			Temp_tempList *def_list = instr_def(list->head);
			// if the instruction use spilled temp
			if(use_list && Temp_In_tempList(spill_temp, *use_list)){
				// replace it with a new temp
				// and spill the new temp to frame
				Temp_temp t2 = Temp_newtemp();
				*use_list = Temp_Replace_tempList(*use_list, spill_temp, t2);
				char *buf = checked_malloc(sizeof(char)*40);
				sprintf(buf, "movl %d(%%ebp), `d0\n", offset_in_frame);
				// insert load instruction before origin use
				AS_instr load_instr = AS_Oper(buf, Temp_TempList(t2, NULL), NULL, NULL);
				AS_instrList new_instr = AS_InstrList(load_instr, list);
				if(last_instr){
					last_instr->tail = new_instr;
				}else{
					result = new_instr;	// add at head of programe
				}

			}
			last_instr = list;
			// if the instruction define the spilled temp
			if(def_list && Temp_In_tempList(spill_temp, *def_list)){
				Temp_temp t1 = Temp_newtemp();
				*def_list = Temp_Replace_tempList(*def_list, spill_temp, t1);
				// insert store instruction after def 
				char *buf = checked_malloc(sizeof(char)*40);
				sprintf(buf, "movl `s0, %d(%%ebp)\n", offset_in_frame);
				AS_instr store_instr = AS_Oper(buf, NULL, Temp_TempList(t1, NULL), NULL);
				AS_instrList new_instr = AS_InstrList(store_instr, next_instr);
				list->tail = new_instr;
				last_instr = new_instr;
			}

			list = next_instr;
		}
	}

	*instr_list = result;
	// clear data structure
}

static Temp_map map_temps(){
	Temp_map result = Temp_empty();
	G_nodeList node_list = G_nodes(conflict_graph);
	for(; node_list != NULL; node_list = node_list->tail){
		int c = *(int *) G_look(color, node_list->head);
		Temp_enter(result, Live_gtemp(node_list->head), reg_names[c]);
	}
	// color for ebp and esp
	Temp_enter(result, F_FP(), "%ebp");
	//Temp_enter(result, F_SP(), "%esp");	// esp has not been add into programe
	return result;
}


struct RA_result RA_regAlloc(F_frame f, AS_instrList il) {
	//your code here
	struct Live_graph lg;
	bool spill = TRUE;
	AS_instrList instr_list = il;
	while(spill){
		// generate data flow graph from programe
		G_graph flow_graph = FG_AssemFlowGraph(instr_list, f);
	 	// generate conflict graph from data flow graph
		lg = Live_liveness(flow_graph);
		Build(lg);	// build will clear all data structure so do not have to clear in rewrite
		MakeWorklist();
		while(simplifyWorklist || spillWorklist || worklistMoves || freezeWorklist){
			if(simplifyWorklist){
				Simplify();
			}else if(worklistMoves){
				Coalesce();
			}else if(freezeWorklist){
				Freeze();
			}else if(spillWorklist){
				SelectSpill();
			}
		}
		AssignColors();
		if(spilledNodes){
			RewriteProgram(f, &instr_list);
		}else{
			spill = FALSE;
		}
	}
	/* TODO */
	// remove colesced move

	struct RA_result ret;
	ret.il = instr_list;
	ret.coloring = map_temps();
	return ret;
}
