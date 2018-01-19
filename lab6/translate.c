#include <stdio.h>
#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"

//LAB5: you can modify anything you want.

// The outermost Tr_level
static Tr_level outermost = NULL;
// global frag table
static F_fragList frag_table = NULL;




Tr_access Tr_Access(Tr_level level, F_access access){
	Tr_access a = (Tr_access)  checked_malloc(sizeof(*a));
	a->level = level;
	a->access = access;
	return a;
}


Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail){
	Tr_accessList list = (Tr_accessList)checked_malloc(sizeof(*list));
	list->head = head;
	list->tail = tail;
	return list;
}

static Tr_accessList makeTr_formals(Tr_level level, F_accessList F_formals){
	if(F_formals){
		return Tr_AccessList(Tr_Access(level, F_formals->head), makeTr_formals(level, F_formals->tail));
	}else{
		return NULL;
	}

}

/* get the formals of a Tr_level */
Tr_accessList Tr_formals(Tr_level level)
{
	F_frame frame = level->frame;
	F_accessList formals = F_formals(frame)->tail;  // get rid of static link
	return makeTr_formals(level, formals);
}


Tr_access Tr_allocLocal(Tr_level level, bool escape)
{
	F_access F_a = F_allocLocal(level->frame, escape); 
	return Tr_Access(level, F_a);
}


Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formal_escape)
{
	Tr_level level = (Tr_level) checked_malloc(sizeof(*level));
	level->parent = parent;
	// the first argument is static link
	level->frame = F_newFrame(name, U_BoolList(TRUE, formal_escape));
	
	return level;
}


Tr_level Tr_outermost(void){
	if(outermost != NULL){
    	return outermost;
	}else{
 	  	outermost = (Tr_level) checked_malloc(sizeof(*outermost));
     	outermost->parent = NULL;
     	outermost->frame = F_newFrame(Temp_namedlabel("tigermain"), NULL);
    	//outermost->formals = NULL;
     	return outermost;

	}
}


Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail){
	Tr_expList list = (Tr_expList) checked_malloc(sizeof(*list));
	list->head = head;
	list->tail = tail;
	return list;
}

static patchList PatchList(Temp_label *head, patchList tail)
{
	patchList list;

	list = (patchList)checked_malloc(sizeof(struct patchList_));
	list->head = head;
	list->tail = tail;
	return list;
}

void doPatch(patchList tList, Temp_label label)
{
	for(; tList; tList = tList->tail)
		*(tList->head) = label;
}

patchList joinPatch(patchList first, patchList second)
{
	if(!first) return second;
	for(; first->tail; first = first->tail);
	first->tail = second;
	return first;
}


F_fragList Tr_getResult(void){
	return frag_table;
}



static T_exp targetFP(Tr_level now_level, Tr_level target_level){
	T_exp fp = T_Temp(F_FP());	
	while(now_level && now_level != target_level){
		fp = F_Exp(F_formals(now_level->frame)->head, fp);  // the first arg in frame is static link
		now_level = now_level->parent;
	}	
	return fp;
}       

static Tr_exp Tr_Ex(T_exp ex)
{
	Tr_exp e = (Tr_exp) checked_malloc(sizeof(* e));
	e->kind = Tr_ex;
	e->u.ex = ex;
	return e;
}

static Tr_exp Tr_Nx(T_stm nx){
	Tr_exp e = (Tr_exp) checked_malloc(sizeof(*e));
	e->kind = Tr_nx;
	e->u.nx = nx;
	return e;
}

static Tr_exp Tr_Cx(struct Cx cx){
	Tr_exp e = (Tr_exp) checked_malloc(sizeof(*e));
	e->kind = Tr_cx;
	e->u.cx = cx;
	return e;
}

static T_exp unEx(Tr_exp e){
	switch (e->kind)
	{
		case Tr_ex:
			return e->u.ex;

		case Tr_cx:
			{
				Temp_temp r = Temp_newtemp();
				Temp_label t = Temp_newlabel(), f = Temp_newlabel();
				doPatch(e->u.cx.trues, t);
				doPatch(e->u.cx.falses, f);

				/*	T_Move(T_Temp(r), T_Const(1)
				 *	e->u.cx.stm
				 *	T_Label(f)
				 *	T_Move(T_Temp(r), T_Const(0)
				 *	T_Label(t)
				 *	T_Temp(r)
				 */

				return T_Eseq(T_Move(T_Temp(r), T_Const(1)),
							T_Eseq(e->u.cx.stm,
								T_Eseq(T_Label(f),
									T_Eseq(T_Move(T_Temp(r), T_Const(0)),
										T_Eseq(T_Label(t),
											T_Temp(r)
										)
									)
								)
							)
						);
			}
			
		case Tr_nx:
			return T_Eseq(e->u.nx, T_Const(0));

	}
	assert(0); // should never reach here
}

static T_stm unNx(Tr_exp e){
	switch (e->kind)
	{
		case Tr_ex:
			return T_Exp(e->u.ex);
		case Tr_nx:
			return e->u.nx;		
		case Tr_cx:
			{
				Temp_label done = Temp_newlabel();
				doPatch(e->u.cx.trues, done);
				doPatch(e->u.cx.falses, done);
				return T_Seq(e->u.cx.stm, T_Label(done));
			}
	}
	assert(0); // should never reach here

}

static struct Cx unCx(Tr_exp e)
{
	switch (e->kind)
	{
		case Tr_ex:
			{
				struct Cx cx;
				cx.stm = T_Cjump(T_ne, e->u.ex, T_Const(0), NULL, NULL); // 0 to false, other to true
				cx.trues = PatchList(&(cx.stm->u.CJUMP.true), NULL);
				cx.falses = PatchList(&(cx.stm->u.CJUMP.false), NULL);
				return cx;
			}
		case Tr_nx:
			assert(0); // should never reach here

		case Tr_cx:
			return e->u.cx;
	}
	assert(0); // should never reach here
}


Tr_exp Tr_Nop(){
	return Tr_Ex(T_Const(0));
}

Tr_exp Tr_Nil(){
	return Tr_Ex(T_Const(0));
	/*TODO */
}

Tr_exp Tr_Int(int v){
	return Tr_Ex(T_Const(v));
}

Tr_exp Tr_String(string s){
	Temp_label label = Temp_newlabel();  // why not Temp_namedlabel(s) ?
	F_frag frag = F_StringFrag(label, s);
	frag_table = F_FragList(frag, frag_table);
	return Tr_Ex(T_Name(label));

}
Tr_exp Tr_Jump(Temp_label label){
	return Tr_Nx(T_Jump(T_Name(label), Temp_LabelList(label, NULL)));
}
Tr_exp Tr_Call(Tr_level target_level, Temp_label label, Tr_expList params, Tr_level cur_level){
	// get all parameters
	T_expList params_t = NULL;
	while(params){
		params_t = T_ExpList(unEx(params->head), params_t);
		params = params->tail;
		// have been reversed twice, so the params_t's order is equal to original parameters' order
	}
	if(target_level == outermost){
		// outermost level frame do not have static link
		return Tr_Ex(T_Call(T_Name(label), params_t));
	}

	return Tr_Ex(T_Call(T_Name(label), T_ExpList(targetFP(cur_level, target_level->parent), params_t)));  // why level->parent?
}


Tr_exp Tr_OpArithm(A_oper oper, Tr_exp left, Tr_exp right){
	T_exp left_t = unEx(left);
	T_exp right_t = unEx(right);
	T_exp result;

	switch (oper)
	{
		case A_plusOp:
			result = T_Binop(T_plus, left_t, right_t);
			break;
		case A_minusOp:
			result = T_Binop(T_minus, left_t, right_t);
			break;
		case A_timesOp:
			result = T_Binop(T_mul, left_t, right_t);
			break;
		case A_divideOp	:
			result = T_Binop(T_div, left_t, right_t);
			break;
		default:
			assert(0);
			// should never reach here
	}
	return Tr_Ex(result);
}


Tr_exp Tr_OpCmp(A_oper oper, Tr_exp left, Tr_exp right, int isStr){
	T_exp left_t;
	T_exp right_t;
	struct Cx result;	
	// if compare with str, call extern funtion
	if(isStr){
		T_expList params_t = T_ExpList(unEx(left), T_ExpList(unEx(right), NULL));
		left_t = F_externalCall("StrCmp", params_t); // if equal, return 0
		right_t = T_Const(0);
	}else{
		left_t = unEx(left);
		right_t = unEx(right);
	}

	switch (oper)
	{
		case A_eqOp:
			result.stm = T_Cjump(T_eq, left_t, right_t, NULL, NULL);	
			break;
		case A_neqOp:
			result.stm = T_Cjump(T_ne, left_t, right_t, NULL, NULL);	
			break;
		case A_ltOp:
			result.stm = T_Cjump(T_lt, left_t, right_t, NULL, NULL);	
			break;
		case A_leOp:
			result.stm = T_Cjump(T_le, left_t, right_t, NULL, NULL);	
			break;
		case A_gtOp:
			result.stm = T_Cjump(T_gt, left_t, right_t, NULL, NULL);	
			break;
		case A_geOp:
			result.stm = T_Cjump(T_ge, left_t, right_t, NULL, NULL);	
			break;
		default:
			assert(0);
	}
	result.trues = PatchList(&(result.stm->u.CJUMP.true), NULL);
	result.falses = PatchList(&(result.stm->u.CJUMP.false), NULL);
	return Tr_Cx(result);
}


Tr_exp Tr_Assign(Tr_exp var, Tr_exp value){
	return Tr_Nx(T_Move(unEx(var), unEx(value)));	
}

// statement sequence
Tr_exp Tr_Seq(Tr_exp seq, Tr_exp e){
	// the last exp of the seq may be return reslt of function
	return Tr_Ex(T_Eseq(unNx(seq), unEx(e)));
	//return Tr_Nx(T_Seq(unNx(left), unNx(right)));
}

Tr_exp Tr_IfThen(Tr_exp test, Tr_exp then){
	Temp_label t = Temp_newlabel();
	Temp_label f = Temp_newlabel();
	struct Cx cx = unCx(test);
	doPatch(cx.trues, t);
	doPatch(cx.falses, f);

	/*
	 *	cx->stm
	 *	T_Label(t)
	 *	unNx(then)
	 *	T_label(f)
	 */
	// if-then must produce no value
	return Tr_Nx(T_Seq(cx.stm,
					T_Seq(T_Label(t),
						T_Seq(unNx(then), T_Label(f)
							)
						)
					)
				);
}
Tr_exp Tr_IfThenElse(Tr_exp test, Tr_exp then, Tr_exp elsee){
	struct Cx cx = unCx(test);
	Temp_label t = Temp_newlabel();
	Temp_label f = Temp_newlabel();
	Temp_label done = Temp_newlabel();
	doPatch(cx.trues, t);
	doPatch(cx.falses, f);

	Temp_temp r = Temp_newtemp();

	/*
	 *	cx.stm
	 *	T_Label(t)
	 *	T_Move(T_Temp(r), unEx(then))
	 *	T_Jump(T_Name(done), Temp_LabelList(done, NULL))
	 *	T_Label(f)
	 *	T_Move(T_Temp(r), unEx(elsee))
	 *  T_Label(done) 
	 *	T_Temp(r)
	 */

	return Tr_Ex(T_Eseq(cx.stm,
					T_Eseq(T_Label(t),
						T_Eseq(T_Move(T_Temp(r), unEx(then)), 
							T_Eseq(T_Jump(T_Name(done), Temp_LabelList(done,  NULL)),
								T_Eseq(T_Label(f),
									T_Eseq(T_Move(T_Temp(r), unEx(elsee)),
										T_Eseq(T_Label(done), T_Temp(r)
											)
										)
									)
								)
							)
						)
					)
				);

}


Tr_exp Tr_While(Tr_exp test, Tr_exp body, Temp_label done){
	struct Cx cx = unCx(test);
	Temp_label test_label = Temp_newlabel();
	Temp_label body_label = Temp_newlabel();
	doPatch(cx.trues, body_label);
	doPatch(cx.falses, done);

	/*
	 *  T_Label(test)
	 *	cx.stm
	 *	T_Label(body)
	 *	unNx(body)
	 *	T_Jump(T_Name(test), Temp_labelList(test, NULL)
	 *	T_Label(done)
	 */

	return Tr_Nx(T_Seq(T_Label(test_label),
					T_Seq(cx.stm, 
						T_Seq(T_Label(body_label),
							T_Seq(unNx(body),
								T_Seq(T_Jump(T_Name(test_label), Temp_LabelList(test_label, NULL)), T_Label(done)
									)
								)
							)
						)
					)
				);
}

Tr_exp Tr_For(Tr_access access, Tr_level level, Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label done){
	Tr_exp i = Tr_simpleVar(access, level);
	Temp_temp limit = Temp_newtemp();
	Temp_label loop = Temp_newlabel();
	Temp_label incr = Temp_newlabel();

	/*
	 *	T_Move(unEx(i), unEx(lo))
	 *	T_Move(T_Temp(limit), unEx(hi))
	 *	T_Cjump(T_gt, unEx(i), T_Temp(limit), done, loop)
	 *	T_Label(loop)
	 *	unNx(body)
	 *	T_Cjump(T_ge, unEx(i), T_Temp(limit), done, incr)
	 *	T_Label(incr)
	 *	T_Move(unEx(i), T_Binop(T_plus, unEx(i), T_Const(1)))
	 *	T_Jump(T_Name(loop), Temp_LabelList(loop, NULL))
	 *	T_Label(done)
	 */

	return Tr_Nx(T_Seq(T_Move(unEx(i), unEx(lo)),
					T_Seq(T_Move(T_Temp(limit), unEx(hi)),
						T_Seq(T_Cjump(T_gt, unEx(i), T_Temp(limit), done, loop),
							T_Seq(T_Label(loop),
								T_Seq(unNx(body),
									T_Seq(T_Cjump(T_ge, unEx(i), T_Temp(limit), done, incr),
										T_Seq(T_Label(incr), 
											T_Seq(T_Move(unEx(i), T_Binop(T_plus, unEx(i), T_Const(1))),
												T_Seq(T_Jump(T_Name(loop), Temp_LabelList(loop, NULL)), T_Label(done)
													)
												)
											)
										)
									)
								)
							)
						)
					)	
				);
}

static T_stm assign_fields(Temp_temp base_addr, int index, Tr_expList fields){
	if(fields){
		/*
		 *	T_Seq(T_Move(T_Mem(T_Binop(T_plus, T_Temp(base_addr), T_Const(F_wordSize * index)), unEx(fields->head)), assign_fields(base_addr, index - 1, fields->tail))	
		 */
		return T_Seq(T_Move(
						T_Mem(
							T_Binop(
								T_plus, T_Temp(base_addr), T_Const(F_wordSize * index)
							)
						),
						unEx(fields->head)
					), 
					assign_fields(base_addr, index - 1, fields->tail)
				);
					
	}else{
		return T_Exp(T_Const(0));
	}
}

Tr_exp Tr_Record(int fields_cnt, Tr_expList fields){
	/*
	 *	T_Move(
			T_Temp(base_addr), 
			F_externalCall("Malloc", 
				T_ExpList(T_Const(F_wordSize * fields_cnt), NULL)
				)
			)
		assign_fields(base_addr, fields_cnt - 1, fields)
		T_Temp(base_addr)
	 */

	Temp_temp base_addr = Temp_newtemp();
	return Tr_Ex(
				T_Eseq(
					T_Move(
						T_Temp(base_addr), F_externalCall("allocRecord", T_ExpList(T_Const(F_wordSize * fields_cnt), NULL))
					),
					T_Eseq(assign_fields(base_addr, fields_cnt - 1, fields), T_Temp(base_addr)
					)
				)
			);

	// the last index is fields_cnt -1
}

/*Tr_exp Tr_Array(Tr_exp size, Tr_exp init){
	Temp_temp limit = Temp_newtemp();
	Temp_temp init_value = Temp_newtemp();
	Temp_temp base_addr = Temp_newtemp();
	Temp_temp i = Temp_newtemp();

	Temp_label test = Temp_newlabel();
	Temp_label body = Temp_newlabel();
	Temp_label done = Temp_newlabel();
	 /*
	  *	T_Move(T_Temp(limit), unEx(size))
	  *	T_Move(T_Temp(init_value), unEx(init))
	  *	T_Move(T_Temp(base_addr), F_externalCall("Malloc", T_ExpList(T_Binop(T_mul, T_Temp(limit), T_Const(F_wordSize)), NULL)))
	  *	T_Move(T_Temp(i), T_Const(0))
	  *	T_Label(test)
	  *	T_Cjump(T_ge, T_Temp(i), T_Temp(size), done, body)
	  *	T_Label(body)
	  *
	  *	// initialize first and then incr i
	  *	T_Move(T_binOp(T_plus, T_Temp(base_addr), T_binOp(T_mul, T_Temp(i), T_Const(F_wordSize))) , T_Temp(init_value))
	  *	T_Move(T_Temp(i), T_binOp(T_plus, T_Const(1), T_Temp(i)))
	  *	T_Jump(T_Name(test), Temp_LabelList(test, NULL))
	  *	T_Label(done)
	  *	T_Temp(base_addr)
	  *	
	  *

	return Tr_Ex(T_Eseq(T_Move(T_Temp(limit), unEx(size)),
                T_Eseq(T_Move(T_Temp(init_value), unEx(init)),
                        T_Eseq(T_Move(T_Temp(base_addr), F_externalCall("Malloc", T_ExpList(T_Binop(T_mul, T_Temp(limit), T_Const(F_wordSize)), NULL))),
                            T_Eseq(T_Move(T_Temp(i), T_Const(0)),
                                T_Eseq(T_Label(test),
                                    T_Eseq(T_Cjump(T_lt, T_Temp(i), T_Temp(limit), body, done),
                                        T_Eseq(T_Label(body),
                                            T_Eseq(T_Move(T_Mem(T_Binop(T_plus, T_Temp(base_addr), T_Binop(T_mul, T_Temp(i), T_Const(F_wordSize)))), T_Temp(init_value)),
                                                T_Eseq(T_Move(T_Temp(i), T_Binop(T_plus, T_Temp(i), T_Const(1))),
                                                    T_Eseq(T_Jump(T_Name(test), Temp_LabelList(test, NULL)),
                                                        T_Eseq(T_Label(done),
                                                            T_Temp(base_addr)
															)
														)
													)
												)
											)
										)
									)
								)
							)
						)
					)
				);
}
*/
Tr_exp Tr_Array(Tr_exp size, Tr_exp init)
{
    return Tr_Ex(F_externalCall("initArray", T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
}

Tr_exp Tr_simpleVar(Tr_access access, Tr_level level){
	T_exp fp = targetFP(level, access->level);
	return Tr_Ex(F_Exp(access->access, fp));
}

Tr_exp Tr_fieldVar(Tr_exp base_addr, int offset){
	return Tr_Ex(T_Mem(
					T_Binop(T_plus, unEx(base_addr), T_Const(offset * F_wordSize))
					)
				);
			
}

Tr_exp Tr_subscriptVar(Tr_exp base_addr, Tr_exp offset){
	return Tr_Ex(T_Mem(
						T_Binop(T_plus, unEx(base_addr), T_Binop(T_mul, unEx(offset), T_Const(F_wordSize)))
					)
				);
}
 
 
T_stm Tr_procEntryExit(Tr_level level, Tr_exp body)
{
	// add a move result to eax action
    return T_Move(T_Temp(F_RV()), unEx(body));
}

void Tr_Func(Tr_exp body, Tr_level level, bool ret){
	F_frag frag;
	if(ret){
		T_stm b = Tr_procEntryExit(level, body);
		frag = F_ProcFrag(b, level->frame);
	}else{
		frag = F_ProcFrag(unNx(body), level->frame);
	}
	frag_table = F_FragList(frag, frag_table);
}
