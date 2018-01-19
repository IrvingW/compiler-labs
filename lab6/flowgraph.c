#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "errormsg.h"
#include "table.h"

Temp_tempList FG_def(G_node n) {
	//your code here.
	AS_instr instr = G_nodeInfo(n);
	switch (instr->kind){
		case I_OPER:
			return instr->u.OPER.dst;
		case I_LABEL:
			return NULL;
		case I_MOVE:
			return instr->u.MOVE.dst;
		default:
			assert(0);	// the code should not reach here
	}
	return NULL;
}

Temp_tempList FG_use(G_node n) {
	//your code here.
	AS_instr instr = G_nodeInfo(n);
	switch(instr->kind){
		case I_OPER:
			return instr->u.OPER.src;
		case I_LABEL:
			return NULL;
		case I_MOVE:
			return instr->u.MOVE.src;
		default:
			assert(0);	// the code should not reach here
	}
	return NULL;
}

bool FG_isMove(G_node n) {
	//your code here.
	AS_instr instr = G_nodeInfo(n);
	return (instr->kind == I_MOVE);
}

/* generate data flow graph */
G_graph FG_AssemFlowGraph(AS_instrList il, F_frame f) {
	//your code here.
	G_graph result = G_Graph();
	TAB_table label2node = TAB_empty();
	TAB_table instr2node = TAB_empty();
	
	G_node last_node = NULL;
	for(AS_instrList instrs = il; instrs; instrs = instrs->tail){
		AS_instr instr = instrs->head;
		G_node instr_node = G_Node(result, instr);
		TAB_enter(instr2node, instr, instr_node);
		if(last_node != NULL)
			G_addEdge(last_node, instr_node);
		last_node = instr_node;		
		if(instr->kind == I_LABEL)
			TAB_enter(label2node, instr->u.LABEL.label, instr_node);
	}

	// link to jump target in data flow graph
	for(AS_instrList instrs = il; instrs; instrs = instrs->tail){
		AS_instr instr = instrs->head;
		if(instr->kind == I_OPER){
			G_node instr_node = TAB_look(instr2node, instr);
			if(instr->u.OPER.jumps != NULL){
				Temp_labelList label_list = instr->u.OPER.jumps->labels;
				for(; label_list != NULL; label_list = label_list->tail){
					G_addEdge(instr_node, TAB_look(label2node, label_list->head));
				}
			}
		}
	}

	return result;
}
