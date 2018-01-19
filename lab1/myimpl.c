#include <stdio.h>
#include <string.h>
#include "prog1.h"
#include "slp.h"	// all tree nodes
#include "util.h"    // string, checked_malloc, U_boolList

// define structure and constructor of table
// table is used to store the mapping relationship between id and value
typedef struct table *Table_;
struct table {string id; int value; Table_ tail;};

Table_ Table(string id, int value, Table_ tail)
{
	Table_ t = checked_malloc(sizeof(*t));
	t->id = id;
	t->value = value;
	t->tail = tail;
	return t;	
}

// define structure and constructor of IntAndTable
//	which is used to store both calculated value and generated new table
struct IntAndTable {int i; Table_ t;};
struct IntAndTable makeIntAndTable(int i, Table_ table)
{
	struct IntAndTable result;
	result.i = i;
	result.t = table;
	return result;
}

// define some function about table
Table_ update(Table_ t, string key, int value);
int lookup(Table_ t, string key);


// interpret functions that will call by interp
Table_ interpPrint(A_expList el, Table_ t);
Table_ interpStm(A_stm stm, Table_ t);
struct IntAndTable interpExp(A_exp e, Table_ t);


// choose the bigger number between two int
int max(int x, int y);

// count max args
int countArgs(A_expList el);
// count max args in a Exp or ExpList
int maxargsExp(A_exp e);
int maxargsExpList(A_expList el);


// the function that will call by exam function 
int maxargs(A_stm stm)
{
	switch (stm->kind)
	{
		case A_compoundStm:
			return max(maxargs(stm->u.compound.stm1), maxargs(stm->u.compound.stm2));
			break;
		case A_assignStm:
			return maxargsExp(stm->u.assign.exp);
			break;
		case A_printStm:
			return max(countArgs(stm->u.print.exps), maxargsExpList(stm->u.print.exps));
			break;
		default:
			break;
	}
}

void interp(A_stm stm)
{
	interpStm(stm, NULL);
}

/* ================= implement functions called by  maxargs()  =======================*/


int max(int x, int y)
{
	return x > y ? x : y;
}


// count args number in a print statement
int countArgs(A_expList el)
{
	switch (el->kind)
	{
		case A_pairExpList:
			return 1+countArgs(el->u.pair.tail);
			break;
		case A_lastExpList:
			return 1;
			break;
		default:
			break;
	}
}


// find out max args count in a expression
int maxargsExp(A_exp e)
{
	switch (e->kind)
	{
		case A_idExp:
		case A_numExp:
			// id and num expression will not add args which is in print statement
			return 0;
			break;
		case A_opExp:
			return max(maxargsExp(e->u.op.left), maxargsExp(e->u.op.right));
			break;
		case A_eseqExp:
			return max(maxargs(e->u.eseq.stm), maxargsExp(e->u.eseq.exp));
			break;
		default:
			break;
	}
}

// find out max arg count in a expression list
int maxargsExpList(A_expList el)
{
	switch (el->kind)
	{
		case A_pairExpList:
			return max(maxargsExp(el->u.pair.head), maxargsExpList(el->u.pair.tail));
			break;
		case A_lastExpList:
			return maxargsExp(el->u.last);
			break;
		default:
			break;
	}
}

/*===================  implement functions called by interp() =============================*/

// interpret statement
Table_ interpStm(A_stm s, Table_ t)
{
	switch (s->kind)
	{
		case A_compoundStm:
			{
				Table_ table = interpStm(s->u.compound.stm1, t);
				return interpStm(s->u.compound.stm2, table);
			}
			break;
		case A_assignStm:
			{
				struct IntAndTable res = interpExp(s->u.assign.exp, t);
				return update(res.t, s->u.assign.id, res.i);
			}
			break;
		case A_printStm:
			return interpPrint(s->u.print.exps, t);
			break;
		default:
			break;
	}
}

// interpret expression
struct IntAndTable interpExp(A_exp e, Table_ t)
{
	switch (e->kind)
	{
		case A_idExp:
			return makeIntAndTable(lookup(t, e->u.id), t);
			break;
		case A_numExp:
			return makeIntAndTable(e->u.num, t);
			break;
		case A_opExp:
			{
				struct IntAndTable left_result = interpExp(e->u.op.left, t);
				struct IntAndTable right_result = interpExp(e->u.op.right, left_result.t);
				switch (e->u.op.oper)
				{
					case A_plus:
						return makeIntAndTable((left_result.i + right_result.i), right_result.t); 
						break;
					case A_minus:
						return makeIntAndTable((left_result.i - right_result.i), right_result.t); 
						break;
					case A_times:
						return makeIntAndTable((left_result.i * right_result.i), right_result.t); 
						break;
					case A_div:
						//assert(right_result.i);  // check for not devide by zero
						return makeIntAndTable((left_result.i / right_result.i), right_result.t); 
						break;
					default:
						break;
				}
			}
			break;
		case A_eseqExp:
			{
				Table_ table = interpStm(e->u.eseq.stm, t);
				return interpExp(e->u.eseq.exp, table);
			}
			break;
		default:
			break;
	}
}



// interpret print statement
Table_ interpPrint(A_expList el, Table_ t)
{
	switch (el->kind)
	{
		case A_pairExpList:
			{
				struct IntAndTable result = interpExp(el->u.pair.head, t);
				printf("%d ", result.i);
				return interpPrint(el->u.pair.tail, result.t);
			}
			break;
		case A_lastExpList:
			{
				struct IntAndTable result = interpExp(el->u.last, t);
				printf("%d\n", result.i);  // print a value
				return result.t;
			}
			break;
		default:
			break;
	}
}

// update table list
Table_ update(Table_ t, string key, int value)
{
	return Table(key, value, t);   // just insert at the beginning of list
}

// look up for value of id in table list

int lookup(Table_ t, string key)
{
	assert(t);  // table not empey

	if (strcmp(t->id, key) == 0)
	{
		return t->value;
	} else
	{
		return lookup(t->tail, key);
	}
}

