#include <stdio.h>
#include "string.h"
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

static bool * cell_of_bitmap(bool *bitmap_base, int node_cnt, int node_key_a, int node_key_b){
	return bitmap_base + (node_cnt * node_key_a + node_key_b);
}

// debug
static void print_tempList(Temp_tempList list, Temp_map map){
	while(list){
		string name = Temp_look(map, list->head);
		printf("%s, ", name);
		list = list->tail;
	}
}

static void print_table(G_nodeList flow_nodes, G_table in_table, G_table table, Temp_map map){
	printf("===================================================\n");
	while(flow_nodes){
		G_node n = flow_nodes->head;
		flow_nodes = flow_nodes->tail;
		Temp_tempList in_set = (Temp_tempList)G_look(in_table, n);
		Temp_tempList out_set = (Temp_tempList)G_look(table, n);
		AS_instr instr = (AS_instr)G_nodeInfo(n);
		AS_print(stdout, instr, map);
		printf("\tin: ");
		print_tempList(in_set, map);
		printf("\n\tout: ");
		print_tempList(out_set, map);
		printf("\n\n");
	}
}

static string node_2_temp(G_node cur_node, Temp_map map){
	Temp_temp cur_temp = (Temp_temp)G_nodeInfo(cur_node);
	string name = Temp_look(map, cur_temp);
	return name;
}

static void print_liveness_result(FILE *out, struct Live_graph lg, Temp_map map){
	G_graph c_graph = lg.graph;
	bool *adj_map = lg.adj_map;
	Live_moveList moveList = lg.moves;
	G_table spill_rank = lg.spill_priority;


	G_nodeList all_nodes = G_nodes(c_graph);
	// node count
	fprintf(out, "node count of conlict graph: %d\n\n", G_graph_nodecount(c_graph));

	for(; all_nodes != NULL; all_nodes = all_nodes->tail){
		G_node cur_node = all_nodes->head;
		string name = node_2_temp(cur_node, map);
		fprintf(out, "Temp Node: %s\n", name);
		// show conlict nodes
		fprintf(out, "conflict nodes: ");
		G_nodeList succs = G_succ(cur_node);
		for(; succs != NULL; succs = succs->tail){
			string name = node_2_temp(succs->head, map);
			fprintf(out, "%s, ", name);
			// check every edge in adjList is in adjmap
			assert(*cell_of_bitmap(adj_map, G_graph_nodecount(c_graph), G_node_key(cur_node), G_node_key(succs->head)));
			assert(*cell_of_bitmap(adj_map, G_graph_nodecount(c_graph), G_node_key(succs->head), G_node_key(cur_node)));
		}
		fprintf(out, "\n");
		if(!strcmp(name, "%ebp")){
			int priority = *(int *)G_look(spill_rank, cur_node);
			fprintf(out, "spill_priority: %d\n", priority);
		}
	}

	fprintf(out, "=====================================================");
	fprintf(out, "moveList\n");
	for(; moveList != NULL; moveList = moveList->tail){
		G_node src = moveList->src;
		G_node dst = moveList->dst;
		string src_name = node_2_temp(src, map);
		string dst_name = node_2_temp(dst, map);
		fprintf(out, "%s ---> %s\n", src_name, dst_name);
	}
}

static Temp_tempList precolored_regs = NULL;

/* generate precolored machine registers */
Temp_tempList Precolored_regs(){
	if(precolored_regs)
		return precolored_regs;

	// ebp is used for frame pointer, can not used to be store temp
	precolored_regs = Temp_TempList(F_eax(),
                      	Temp_TempList(F_ebx(),
                      		Temp_TempList(F_ecx(),
                      			Temp_TempList(F_edx(),
                      				Temp_TempList(F_esi(),
                      					Temp_TempList(F_edi(), NULL)
									)
								)
							)
						)
					  );
	return precolored_regs;
}

Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
	Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
	lm->src = src;
	lm->dst = dst;
	lm->tail = tail;
	return lm;
}

/* return the map temp of a G_node */
Temp_temp Live_gtemp(G_node n) {
	//your code here.
	return G_nodeInfo(n);
}

/* Union operation on two moveList */
Live_moveList Live_Union_moveList(Live_moveList l, Live_moveList r){
	Live_moveList result = l;
	while(r){
		G_node src = r->src;
		G_node dst = r->dst;
		if(!Live_In_moveList(src, dst, l)){
		// app at the head
			result = Live_MoveList(src, dst, result);
		}
		r = r->tail;
	}
	return result;
}

/* Substract operation on two moveList */
Live_moveList Live_Sub_moveList(Live_moveList l, Live_moveList r){
	Live_moveList result = NULL;
	while(l){
		G_node src = l->src;
		G_node dst = l->dst;
		if(!Live_In_moveList(src, dst, r)){
			result = Live_MoveList(src, dst, result);
		}
		l = l->tail;
	}
	return result;
}

/* Intersect operation on two moveList */
Live_moveList Live_Intersect_moveList(Live_moveList l, Live_moveList r){
	Live_moveList result = NULL;
	while(r){
		G_node src = r->src;
		G_node dst = r->dst;
		if(Live_In_moveList(src, dst, l)){
			result = Live_MoveList(src, dst, result);
		}
		r = r->tail;
	}
	return result;
}

/* if a node is in moveList */
bool Live_In_moveList(G_node src, G_node dst, Live_moveList list){
	while(list){
		if(src == list->src && dst == list->dst){
			return TRUE;
		}
		list = list->tail;
	}
	return FALSE;
}

/* get G_node from temp_2_node table using temp name
 * if the temp have not initilize a G_node, initialize it
 */
static G_node get_Gnode(G_graph g, Temp_temp t, TAB_table temp_2_node){
	G_node result = (G_node) TAB_look(temp_2_node, t);
	if(!result){
		result = G_Node(g, t);
		TAB_enter(temp_2_node, t, result);
	}
	return result;
}



/* link two node */
static void link(struct Live_graph *lg, Temp_temp temp_a, Temp_temp temp_b, TAB_table temp_2_node){
	// do not need link
	if(temp_a == temp_b || temp_a == F_FP() || temp_b == F_FP())
		return;
	
	G_node node_a = get_Gnode(lg->graph, temp_a, temp_2_node);
	G_node node_b = get_Gnode(lg->graph, temp_b, temp_2_node);
	// update adjacent bit map
	bool * cell = cell_of_bitmap(lg->adj_map, G_graph_nodecount(lg->graph), G_node_key(node_a),G_node_key(node_b));
	if(! *cell){	// if not adjacent before
		*cell = TRUE;
		cell = cell_of_bitmap(lg->adj_map, G_graph_nodecount(lg->graph), G_node_key(node_b), G_node_key(node_a));
		*cell = TRUE;

		// precolored node only use adjacent bitmap to store edge
		// the degree of precolored node is infinite max, so precolored node do not need succ set
		// normal node use adjacent list (G_nodeList succs, preds)too
		if(!Temp_In_tempList(temp_a, Precolored_regs())){
			G_addEdge(node_a, node_b);
		}
		if(!Temp_In_tempList(temp_b, Precolored_regs())){
			G_addEdge(node_b, node_a);
		}
	}
	
	// update spill priority
	// the more a node is conflict with other nodes, the higher it's spill priority is
	if(!Temp_In_tempList(temp_a, Precolored_regs())){
		int *i = (int *)G_look(lg->spill_priority, node_a);
		*i = *i + 1;
	}
	if(!Temp_In_tempList(temp_b, Precolored_regs())){
		int *i = (int *)G_look(lg->spill_priority, node_b);
		*i = *i + 1; 
	}
}


/* liveness Analysis, return conflict graph */
struct Live_graph Live_liveness(G_graph flow) {
	//your code here.
	struct Live_graph lg;
    G_table in_table = G_empty();
	G_table out_table = G_empty();

	G_nodeList flow_nodes = G_nodes(flow);	// node of data flow

	// initialize live_in and live_out fow each node in flow graph
	for(; flow_nodes != NULL; flow_nodes = flow_nodes->tail){
		G_enter(in_table, flow_nodes->head, NULL);
		G_enter(out_table, flow_nodes->head, NULL);	
	}
	
	bool changed = TRUE;	// loop until in and out set on every node do not change any more
	Temp_map map = Temp_layerMap(F_tempMap, Temp_name());
	while(changed){
		changed = FALSE;
		flow_nodes = G_nodes(flow);
		
		for(; flow_nodes != NULL; flow_nodes = flow_nodes->tail){
			G_node cur_flow_node = flow_nodes->head;
			Temp_tempList in_set_initial = (Temp_tempList)G_look(in_table, cur_flow_node);
			Temp_tempList out_set_initial = (Temp_tempList)G_look(out_table, cur_flow_node);
			Temp_tempList in_set_after = NULL;
			Temp_tempList out_set_after = NULL;
			// update in_set and out_set			
			G_nodeList succ_list = G_succ(cur_flow_node);
			// out = U in[succ[n]]
			for(; succ_list != NULL; succ_list = succ_list->tail){
				G_node succ_node = succ_list->head;
				Temp_tempList succ_in_set = (Temp_tempList)G_look(in_table, succ_node);
				out_set_after = Temp_Union_tempList(out_set_after, succ_in_set);
			}
			// in[n] = use[n] U (out[n] - def[n])
			in_set_after = Temp_Union_tempList(FG_use(cur_flow_node), Temp_Sub_tempList(out_set_after, FG_def(cur_flow_node)));

			if(!Temp_Equal_tempList(in_set_initial, in_set_after)){
				changed = TRUE;
				G_enter(in_table, cur_flow_node, in_set_after);	// this will overwrite the old value in table
			}
			if(!Temp_Equal_tempList(out_set_initial, out_set_after)){
				changed = TRUE;
				G_enter(out_table, cur_flow_node, out_set_after);
			}
		}	
		 
	}
	flow_nodes = G_nodes(flow);
	print_table(flow_nodes, in_table, out_table, map);

	lg.graph = G_Graph();
	lg.spill_priority = G_empty();
	lg.moves = NULL;


	// generate conflisct graph
	TAB_table temp_2_node = TAB_empty();
	// insert pre-colored node into conflict graph
	for(Temp_tempList pre_regs = Precolored_regs(); pre_regs; pre_regs = pre_regs->tail){
		get_Gnode(lg.graph, pre_regs->head, temp_2_node);
	}
	// insert other temp into conflict graph, initialize a spill priority value for each node
	flow_nodes = G_nodes(flow);
	for(; flow_nodes != NULL; flow_nodes = flow_nodes->tail){
		for(Temp_tempList def_list = FG_def(flow_nodes->head); def_list; def_list = def_list->tail){
			// assign color do not use ebp and esp, becuase esp use instruction is inserted in next step, 
			//	so do not have to take care of it
			if(def_list->head != F_FP()){	// example: movl 4(ebp), xxx
				int * priority = checked_malloc(sizeof(int));
				*priority = 0;
				G_node node = get_Gnode(lg.graph, def_list->head, temp_2_node);
				G_enter(lg.spill_priority, node, priority);
			}
		}
	}
	// initialize adjacent bit map
	int node_cnt = G_graph_nodecount(lg.graph);
	lg.adj_map = checked_malloc((node_cnt * node_cnt) * sizeof(bool));

	// add conflict edge
	// all precolored nodes conflict with each other
	for(Temp_tempList p1 = Precolored_regs(); p1; p1 = p1->tail){
		for(Temp_tempList p2 = Precolored_regs(); p2; p2 = p2->tail){
			if(p1->head != p2->head)
				link(&lg, p1->head, p2->head, temp_2_node);
		}
	}

	// link normal nodes
	flow_nodes = G_nodes(flow);
	for(; flow_nodes != NULL; flow_nodes = flow_nodes->tail){
		G_node cur_node = flow_nodes->head;
		Temp_tempList out_set = (Temp_tempList) G_look(out_table, cur_node);
		// consider move instruction
		AS_instr instr = G_nodeInfo(cur_node);
		if(instr->kind == I_MOVE){
			// remove move src from out set 
			out_set = Temp_Sub_tempList(out_set, FG_use(cur_node));
			// initialize lg.moves
			for(Temp_tempList def_list = FG_def(cur_node); def_list; def_list = def_list->tail){
            	for(Temp_tempList use_list = FG_use(cur_node); use_list; use_list = use_list->tail){
					if(use_list->head != F_FP() && def_list->head != F_FP()){
						G_node src_node = get_Gnode(lg.graph, use_list->head, temp_2_node);
						G_node dst_node = get_Gnode(lg.graph, def_list->head, temp_2_node);
						lg.moves = Live_MoveList(src_node, dst_node, lg.moves);
					}
				}
			}
		}

		// link (def[n], out[n]/move_src[n])
		for(Temp_tempList def_list = FG_def(cur_node); def_list; def_list = def_list->tail){
        	for(Temp_tempList out_list = out_set; out_list; out_list = out_list->tail){
				link(&lg, def_list->head, out_list->head, temp_2_node);
			}
		}
	}

	//return lg;

	FILE * log = fopen("liveness_log", "w");
	print_liveness_result(log, lg, map);

	return lg;
}


