#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "env.h"
#include "semant.h"
#include "helper.h"
#include "translate.h"

/*Lab5: Your implementation of lab5.*/

struct expty 
{
	Tr_exp exp; 
	Ty_ty ty;
};

//In Lab4, the first argument exp should always be **NULL**.
struct expty expTy(Tr_exp exp, Ty_ty ty)
{
	struct expty e;
	e.exp = exp;
	e.ty = ty;
	return e;
}

F_fragList SEM_transProg(A_exp exp){
	//TODO LAB5: do not forget to add the main frame
	Tr_level outermost = Tr_outermost();
	struct expty e = transExp(E_base_venv(), E_base_tenv(), exp, outermost, NULL);
	Tr_Func(e.exp, outermost, FALSE);
	return Tr_getResult();
}


/* check if two Ty_ty are equal */
bool typeEq(Ty_ty a, Ty_ty b)
{
	// for array and record, only the same address can be seen as the same type in tiger language
	int kind = a->kind;
	if(a->kind == Ty_array){
		return a == b;
	}else if(a->kind == Ty_record){
		return a == b || b->kind == Ty_nil;
	}else if(b->kind == Ty_record){
		return a == b || a->kind == Ty_nil;
	}else{  // for other condition, compare their type directly
		return a->kind == b->kind;
	}
}

/* return actual type of Ty_name type
 * 
 * example: 
 *	type a = array of int
 * 	type b = a
 *
 * the actual kind of b is Ty_array  */ 

Ty_ty actual_ty(Ty_ty ty){
	// stop when encouter a Ty_name
	if(ty->kind == Ty_name){
		return ty->u.name.ty;
	}else{
		return ty;
	}
}

Ty_fieldList actual_tys(Ty_fieldList fs_ty)	
{
	if(fs_ty){
		return Ty_FieldList(Ty_Field(fs_ty->head->name, actual_ty(fs_ty->head->ty)), actual_tys(fs_ty->tail));
	}else{
		return NULL;
	}
}

/* check the result of S_look in tenv
 * report an error if type do not exist */
Ty_ty checked_S_look_type(S_table tenv, S_symbol name, A_pos pos){
	Ty_ty ret = S_look(tenv, name);
	if(!ret){
		EM_error(pos, "undefined type %s", S_name(name));
		ret = Ty_Int();
	}
	return ret;
}
	
/* transExp */

struct expty transExp(S_table venv, S_table tenv, A_exp a, Tr_level level, Temp_label label)
{
	switch(a->kind) {
		case A_varExp:
			return transVar(venv, tenv, a->u.var, level, label);			  
		
		case A_nilExp:
			return expTy(Tr_Nil(), Ty_Nil());
		
		case A_intExp:
			return expTy(Tr_Int(a->u.intt), Ty_Int());
		
		case A_stringExp:
			return expTy(Tr_String(a->u.stringg), Ty_String());				 
		
		// patern: ID(params)
		// A_CallExp(A_pos pos, S_symbol func, A_expList args)	
		case A_callExp:{
			E_enventry entry = S_look(venv, a->u.call.func);
			A_expList args;
			Ty_tyList formals;
			Tr_expList params_tr = NULL;
			struct expty arg_expty;
			if(entry && entry->kind == E_funEntry){
				// check all args type
				for (args = a->u.call.args, formals = entry->u.fun.formals; args&& formals; args = args->tail, formals = formals->tail)
				{
					arg_expty = transExp(venv, tenv, args->head, level, NULL);
					if(!typeEq(arg_expty.ty, formals->head)){
						EM_error(args->head->pos, "para type mismatcpara type mismatchh", S_name(a->u.call.func));
					}
					params_tr = Tr_ExpList(arg_expty.exp, params_tr);
				}

				if(args != NULL){
					EM_error(a->pos, "too many params in function %s", S_name(a->u.call.func));
				}
				if(formals != NULL){
					EM_error(a->pos, "too less params in funtion %s", S_name(a->u.call.func));
				}
				
				// pass type checking
				Tr_exp call_tr = Tr_Call(entry->u.fun.level, entry->u.fun.label, params_tr, level); 
				return expTy(call_tr, entry->u.fun.result);
			}
			else{
				EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
				return expTy(Tr_Nop(), Ty_Int());
			}
		}
		

		// exp1 op exp2
		// for +-*/: exp1's and exp2's types must be Ty_int
		// for equal operation: exp1's type must be same with exp2's
		// for < > >= <=, they can only used on both int or both string
		case A_opExp:{
			A_oper oper = a->u.op.oper;
			struct expty left = transExp(venv, tenv, a->u.op.left, level, label);
			struct expty right = transExp(venv, tenv, a->u.op.right, level, label);
			int is_str = (left.ty->kind == Ty_string);
			
			if(oper == A_plusOp || oper == A_minusOp || oper == A_timesOp || oper == A_divideOp){
				if(left.ty->kind != Ty_int)
				  EM_error(a->u.op.left->pos, "integer required");
				if(right.ty->kind != Ty_int)
				  EM_error(a->u.op.right->pos, "integer required");
				return expTy(Tr_OpArithm(oper, left.exp, right.exp), Ty_Int());
			}
			// eq / neq
			else if(oper == A_eqOp || oper == A_neqOp){
				if(!typeEq(left.ty, right.ty) || left.ty->kind == Ty_void) 
				  EM_error(a->pos, "same type required");
				
				return expTy(Tr_OpCmp(oper, left.exp, right.exp, is_str), Ty_Int());
			}
			else if(oper == A_ltOp || oper == A_leOp || oper == A_gtOp || oper == A_geOp){// lt,le,gt,ge only on int and string
				if(!( (left.ty->kind == Ty_int && right.ty->kind == Ty_int) || (left.ty->kind == Ty_string && right.ty->kind == Ty_string) ))
				  EM_error(a->pos, "same type required");
				return expTy(Tr_OpCmp(oper, left.exp, right.exp, is_str), Ty_Int());
			}
			
			// the code should not reach here
			else
			  assert(0);
		}
	    // record patern: ID(ID=exp, ID=exp, ...)
		case A_recordExp:{
			Ty_ty init_ty = checked_S_look_type(tenv, a->u.record.typ, a->pos);
			Ty_ty ty = actual_ty(init_ty);
			A_efieldList efs;
			Ty_fieldList fs;
			int fields_cnt = 0;
			Tr_expList fields = NULL;

			struct expty efield_exp_expTy;
			// actual type of ID before ( should be Ty_record
			if(ty->kind != Ty_record){
				EM_error(a->pos, "record type required");
				return expTy(Tr_Nop(), Ty_Int());
			}

			// check each field
			// efield patern: ID = exp
			for(efs = a->u.record.fields, fs = ty->u.record; efs && fs; efs = efs->tail, fs = fs->tail){
				efield_exp_expTy = transExp(venv, tenv, efs->head->exp, level, label);
				// name not equal or Ty_ty not equal
				if(!(efs->head->name == fs->head->name)  || !(typeEq(efield_exp_expTy.ty, fs->head->ty))){
					EM_error(a->pos, "type name or expression return  type mismatch");
				}

				fields_cnt ++;
				fields = Tr_ExpList(efield_exp_expTy.exp, fields);
				// here the field is reversed, so it's ok to reverse fields again when malloc space for record
			}
			if(fs || efs){
				EM_error(a->pos, "type count mismatch");
			}

			// pass type checking
			Tr_exp record_tr = Tr_Record(fields_cnt, fields);
			return expTy(record_tr, ty);
		
		}

		case A_seqExp:{
			// patern: exp ; exp ; ...
			A_expList curExp;
			struct expty exp = expTy(NULL, Ty_Void());
			Tr_exp exp_tr = Tr_Nop();
			for(curExp = a->u.seq; curExp; curExp = curExp->tail){
				exp = transExp(venv, tenv, curExp->head, level, label);
				exp_tr = Tr_Seq(exp_tr, exp.exp);
			}
			struct expty result = expTy(exp_tr, exp.ty);
			return result;
		}
		// lvalue := exp
		// lvalue could be ID, lvalue.ID, lvalue{ID}
		//				simpleVar, fieldVar, subscriptVar
		case A_assignExp:{
			struct expty var = transVar(venv, tenv, a->u.assign.var, level, label);
			struct expty exp = transExp(venv, tenv, a->u.assign.exp, level, label);
			
			// loop variable can not be assigned
			if(var.ty->kind == Ty_int && (var.ty->u.intt.loop_var == 1)){
				EM_error(a->pos, "loop variable can't be assigned");	
			}

			if(!typeEq(var.ty, exp.ty))
			  EM_error(a->pos, "unmatched assign exp");

			Tr_exp assign_tr = Tr_Assign(var.exp, exp.exp);
			return expTy(assign_tr, Ty_Void());
		}
		case A_ifExp:{
			// the type of if exp must be int
			// the type of elsee and then must be the same
			// if-then must produce no value
			struct expty if_ty = transExp(venv, tenv, a->u.iff.test, level, label);
			if(if_ty.ty->kind != Ty_int){
				EM_error(a->u.iff.test->pos, "integer required");
			}
			struct expty then_ty = transExp(venv, tenv, a->u.iff.then, level, label);
			if(a->u.iff.elsee){
				struct expty else_ty = transExp(venv, tenv, a->u.iff.elsee, level, label);
				if(!typeEq(else_ty.ty, then_ty.ty)){
					EM_error(a->pos, "then exp and else exp type mismatch");
				}
				Tr_exp if_then_else_tr = Tr_IfThenElse(if_ty.exp, then_ty.exp, else_ty.exp);
				return expTy(if_then_else_tr, then_ty.ty);
			}else{
				if(then_ty.ty->kind != Ty_void){
					EM_error(a->pos, "if-then exp's body must produce no value");
				}
				
				Tr_exp if_then_tr = Tr_IfThen(if_ty.exp, then_ty.exp);
				return expTy(if_then_tr, Ty_Void());
			}
		}
		case A_whileExp:{
			Temp_label done = Temp_newlabel();
			struct expty test_ty = transExp(venv, tenv, a->u.whilee.test, level, label);
			struct expty body_ty = transExp(venv, tenv, a->u.whilee.body, level, done);
			/* TODO */
			// why ? what is the meaning of Temp_label when trans...

			if(test_ty.ty->kind != Ty_int){
				EM_error(a->u.whilee.test->pos, "integer required");
			}
			if(body_ty.ty->kind != Ty_void){
				EM_error(a->u.whilee.body->pos, "while body must produce no value");
			}


			Tr_exp while_tr = Tr_While(test_ty.exp, body_ty.exp, done);
			return expTy(while_tr, Ty_Void());
		}
		case A_forExp:{
			// begin a new scope and enter the the counter variable 	
			// declare a normal int variable at first and after trans low part
			// change this variable to unchangable
			S_beginScope(venv);
			Tr_access access_tr = Tr_allocLocal(level, a->u.forr.escape);
			S_enter(venv, a->u.forr.var, E_VarEntry(access_tr, Ty_Int()));
			
			struct expty lo_ty = transExp(venv, tenv, a->u.forr.lo, level, label);
			struct expty hi_ty = transExp(venv, tenv, a->u.forr.hi, level, label);
			struct expty body_ty;

			if(lo_ty.ty->kind != Ty_int){
				EM_error(a->u.forr.lo->pos, "for exp's range type is not integer");
			}
			if(hi_ty.ty->kind != Ty_int){
				EM_error(a->u.forr.hi->pos, "for exp's range type is not integer");
			}
			
			// change the variable to unchangable after declarition
			E_enventry entry = S_look(venv, a->u.forr.var);
			Ty_ty for_ty = entry->u.var.ty;
			for_ty->u.intt.loop_var = 1;

			Temp_label done = Temp_newlabel();
			body_ty = transExp(venv, tenv, a->u.forr.body, level, done);
			if(body_ty.ty->kind != Ty_void){
				EM_error(a->u.forr.body->pos, "for body must produce no value");
			}
			S_endScope(venv);

			Tr_exp for_tr = Tr_For(access_tr, level, lo_ty.exp, hi_ty.exp, body_ty.exp, done);	
			return expTy(for_tr, Ty_Void());
		}
		case A_breakExp:
			{
				// if label is not NULL, then break is inside a while or for loop
				// and the label is at the exit point of that loop
				if(label)
					return expTy(Tr_Jump(label), Ty_Void());
				else{
					EM_error(a->pos, "break is not inside a loop");
					return expTy(Tr_Nop(), Ty_Void());
				}
			}		
		case A_letExp:{
			struct expty body_expTy;
			A_decList decs;
			S_beginScope(venv);
			S_beginScope(tenv);
			Tr_exp e = Tr_Nop();

			for(decs = a->u.let.decs; decs; decs = decs->tail){
				Tr_exp d = transDec(venv, tenv, decs->head, level, label);
				e = Tr_Seq(e, d);
			}
			body_expTy = transExp(venv, tenv, a->u.let.body, level, label);
			body_expTy.exp = Tr_Seq(e, body_expTy.exp);
			S_endScope(tenv);
			S_endScope(venv);

			return body_expTy;
		}
		// ID [exp1] of exp2
		// exp1 must be Ty_int, ID's type should be Ty_array
		case A_arrayExp:{
			// check ID's type
			Ty_ty ty = actual_ty(checked_S_look_type(tenv, a->u.array.typ, a->pos));
			struct expty size = transExp(venv, tenv, a->u.array.size, level, label);
			struct expty init = transExp(venv, tenv, a->u.array.init, level, label);
			// check size type
			if(size.ty->kind != Ty_int){
				EM_error(a->u.array.size->pos, "integer required");
			}
			if(!typeEq(ty->u.array, init.ty)){
				EM_error(a->u.array.init->pos, "array type mismatch");
			}

			Tr_exp array_tr = Tr_Array(size.exp, init.exp);	
			return expTy(array_tr, ty);
		}
	}

}



/* ensure in a NametyList, two type will not have same name */
int find_same_name(A_nametyList follow_list, S_symbol name){
	// the end of nametyList
	if(!follow_list){
		return 0;
	}

	// not the end
	if(follow_list->head->name == name){
		return 1;
	}else{
		return find_same_name(follow_list->tail, name);
	}

}

/* ensure in a fundecList, two function can not have same name */
int find_same_funcName(A_fundecList follow_list, S_symbol name){
	// the end of nametyList
	if(!follow_list){
		return 0;
	}

	// not the end
	if(follow_list->head->name == name){
		return 1;
	}else{
		return find_same_funcName(follow_list->tail, name);
	}

}


// go through fomal list, return a Ty_TyList include all formal's Ty_ty (in order)from name
Ty_tyList makeFormalTyList(S_table tenv, A_fieldList fields){
	if(fields){
		Ty_ty ty = checked_S_look_type(tenv, fields->head->typ, fields->head->pos);
		return Ty_TyList(actual_ty(ty), makeFormalTyList(tenv, fields->tail));
	}else{
		return NULL;   // the end
	}
}

// just like the below one , but this time the funciton return a Ty_fieldList from name
// which have a binding of name and Ty_ty
// used by A_recordTy
Ty_fieldList makeFieldTyList(S_table tenv, A_fieldList fields){
	if(fields){
		Ty_ty ty = checked_S_look_type(tenv, fields->head->typ, fields->head->pos);
		return Ty_FieldList(Ty_Field(fields->head->name, ty) , makeFieldTyList(tenv, fields->tail));
	}else{
		return NULL;  // the end
	}
}

U_boolList makeFormalBoolList(A_fieldList fields){
    if (fields) {
        U_boolList result  = U_BoolList(fields->head->escape, makeFormalBoolList(fields->tail));
		return result;
    } else {
        return NULL;
    }
}


/* transDec */
Tr_exp transDec(S_table venv, S_table tenv, A_dec d, Tr_level level, Temp_label label){
	switch (d->kind){
		// function ID (typefields formals) = exp
		case A_functionDec:{
			// first iteration, put the head of all function into venv
			// also ,check if two function have same name

			A_fundecList funcs;
			Ty_ty result_ty;
			for(funcs = d->u.function; funcs; funcs = funcs->tail){
				A_fundec cur_func = funcs->head;
				// decide result type
				if(strcmp(S_name(cur_func->result), "")){
					result_ty = actual_ty(checked_S_look_type(tenv, cur_func->result, cur_func->pos));
				}else{
					result_ty = Ty_Void();		
				}
				// check name
				if(find_same_funcName(funcs->tail, funcs->head->name)){
					EM_error(d->pos, "two functions have the same name");
				}
				// put FunEntry into venv
				Ty_tyList formalTyList = makeFormalTyList(tenv, cur_func->params);
				
				U_boolList formals_escape = makeFormalBoolList(cur_func->params);
				/* TODO */
				//Temp_label new_label = Temp_newlabel();
				Temp_label new_label = Temp_namedlabel(S_name(cur_func->name));
				Tr_level new_level = Tr_newLevel(level, new_label, formals_escape);	// here the function's level is its new level? is this correct
				S_enter(venv, cur_func->name, E_FunEntry(new_level, new_label, formalTyList, result_ty));
			}
		
			Tr_accessList params_tr;  //
			// check into every funtion
			for(funcs = d->u.function; funcs; funcs = funcs->tail){
				// put into venv all parameters
				S_beginScope(venv);
				A_fundec cur_func = funcs->head;
				E_enventry funEntry = S_look(venv, cur_func->name);
				A_fieldList params;
				Ty_tyList param_tys = funEntry->u.fun.formals;

				params_tr = Tr_formals(funEntry->u.fun.level);

				for(params = cur_func->params ; params && param_tys; params = params->tail, params_tr = params_tr->tail, param_tys = param_tys->tail){
					S_enter(venv, params->head->name, E_VarEntry(params_tr->head, param_tys->head));
				}

				// verify result type
				result_ty = funEntry->u.fun.result;
				struct expty exp = transExp(venv, tenv, cur_func->body, funEntry->u.fun.level, NULL);
				if(!typeEq(exp.ty, result_ty)){
					if(result_ty->kind == Ty_void){
						EM_error(cur_func->body->pos, "procedure returns value");
					}else{
						EM_error(cur_func->body->pos, "type mismatch");
					}
				}
				S_endScope(venv);

				Tr_Func(exp.exp, funEntry->u.fun.level, TRUE);
			}
			
			return Tr_Nop();
	    }

		case A_varDec:{
			// var id := exp
			// var id : type-id := exp
			struct expty exp = transExp(venv, tenv, d->u.var.init, level, label);
			Tr_access access_tr = Tr_allocLocal(level, d->u.var.escape);	// allocate a pointer
			// if has type-id, check type of exp and type-id
			if(strcmp(S_name(d->u.var.typ), "")){
				Ty_ty init_given_ty = checked_S_look_type(tenv, d->u.var.typ, d->pos);
				Ty_ty actual_given_ty = actual_ty(init_given_ty);
				// check if type of exp and type-id are equal
				if(!typeEq(exp.ty, actual_given_ty)){
					EM_error(d->pos, "type mismatch");
				}
				S_enter(venv, d->u.var.var, E_VarEntry(access_tr, actual_given_ty));

			}else{
				// do not have type-id
				// then exp type can not be Ty_nil
				if(exp.ty->kind == Ty_nil){
					EM_error(d->u.var.init->pos, "init should not be nil without type specified");
				}
				else
					S_enter(venv, d->u.var.var, E_VarEntry(access_tr, exp.ty));
			}

			return Tr_Assign(Tr_simpleVar(access_tr, level), exp.exp);
		}
		
		case A_typeDec:{
			// type id = ty			
			// ty:	ID				A_nameTy
			//		(ID:ID, ...)	A_recordTy
			//		array of ID		A_arrayTy

			// notice namety is different from nameTy
			A_nametyList namety_list;
			for(namety_list = d->u.type; namety_list; namety_list = namety_list->tail){
				A_namety cur_namety = namety_list->head;
				if(find_same_name(namety_list->tail, cur_namety->name)){
					EM_error(cur_namety->ty->pos, "two types have the same name");			
				}
				// at first just store the name(head) and do not fill the Ty_ty(body)
				S_enter(tenv, cur_namety->name, Ty_Name(cur_namety->name, NULL));
			}

			// fill the Ty_ty(body)
			// used to record the count of A_nameTy in nametyList
			int nameTy_cnt = 0;

			for(namety_list = d->u.type; namety_list; namety_list = namety_list->tail){
				A_namety cur_namety = namety_list->head;
				switch (cur_namety->ty->kind)
				{
					case A_nameTy:{
						// this part is confuessing
						// note: please check again before compile

						// type type-id = type-id
						nameTy_cnt ++;
						Ty_ty name_ty = checked_S_look_type(tenv, cur_namety->name, d->pos);
						name_ty->kind = Ty_name;  // this is default write in first iteration, but I write it again to make code easier to understand

						name_ty->u.name.sym = cur_namety->ty->u.name;
						break;
					}
					case A_recordTy:{
						Ty_ty record_ty = checked_S_look_type(tenv, cur_namety->name, d->pos);
						record_ty->kind = Ty_record;
						record_ty->u.record = makeFieldTyList(tenv, cur_namety->ty->u.record);
						// just as below, makeFieldTyList return types that are not actual types
						break;
					}
					case A_arrayTy:{
						Ty_ty array_ty = checked_S_look_type(tenv, cur_namety->name, d->pos);
						array_ty->kind = Ty_array;
						array_ty->u.array = checked_S_look_type(tenv, cur_namety->ty->u.array, d->pos);
						// you can't return actual type here because some of the type declaration has 
						// not been go through, so at the last of the function I iterate them again to 
						// guarentee each type is actual type
						break;
					}
				}
			}
			
			// detect recursive cycle name type
			int recursive_depth;  // depth of recursive nameTy
			while(nameTy_cnt != 0){
				int nameTy2NULL_cnt = 0;
				for(namety_list = d->u.type; namety_list; namety_list = namety_list->tail){
					A_namety cur_namety = namety_list->head;
					if(cur_namety->ty->kind == A_nameTy){
						Ty_ty name_ty = checked_S_look_type(tenv, cur_namety->name, d->pos);
						if(!name_ty->u.name.ty){ // it is NULL
							Ty_ty name_ty_to = checked_S_look_type(tenv, name_ty->u.name.sym, d->pos);
							if(name_ty_to->kind == Ty_name){
								// point to another
								if(name_ty_to->u.name.ty){
									name_ty->u.name.ty = name_ty_to->u.name.ty;
								}else{
									// Ty_ty of name_ty_to is NULL
									nameTy2NULL_cnt ++;
								}
							}else{
								name_ty->u.name.ty = name_ty_to;
							}
						}
					}
				}
				
				if(nameTy2NULL_cnt == nameTy_cnt){
					// all nameTy Ty_ty are NULL
					EM_error(d->pos, "illegal type cycle");
					break;
				}
				nameTy_cnt = nameTy2NULL_cnt;
			}


			// iteration 3, to get actual types
			for (namety_list = d->u.type; namety_list; namety_list = namety_list->tail){
				A_namety cur_namety = namety_list->head;
				switch(cur_namety->ty->kind){
					case A_nameTy:
						break;
					case A_recordTy:{
						Ty_ty ty = checked_S_look_type(tenv, cur_namety->name, d->pos);
						ty->u.record = actual_tys(ty->u.record);
						break;				
					}

					case A_arrayTy:{
						Ty_ty ty = checked_S_look_type(tenv, cur_namety->name, d->pos);
						ty->u.array = actual_ty(ty->u.array);
						break;			   
					}

				}
			}

			return Tr_Nop();
		}
	}
}

struct expty transVar(S_table venv, S_table tenv, A_var v, Tr_level level, Temp_label label){
	switch (v->kind)
	{
		case A_simpleVar:{
			E_enventry entry = S_look(venv, v->u.simple);
			if(entry && entry->kind == E_varEntry){
				return expTy(Tr_simpleVar(entry->u.var.access, level), entry->u.var.ty);
			}else{
				EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
				return expTy(Tr_Nop(), Ty_Int());
			}
		}
		case A_fieldVar:{
			// a dot b
			struct expty var_ty = transVar(venv, tenv, v->u.field.var, level, label);
			if(actual_ty(var_ty.ty)->kind != Ty_record){
				EM_error(v->u.field.var->pos, "not a record type");
				return expTy(Tr_Nop(), Ty_Int());
			}
			else{
				int index = 0;
				Ty_fieldList record_fields;
				for(record_fields = var_ty.ty->u.record; record_fields; record_fields = record_fields->tail){
					Ty_field a_field = record_fields->head;
					if(a_field->name == v->u.field.sym){
						Tr_exp fieldVar_tr = Tr_fieldVar(var_ty.exp, index);
					  	return expTy(fieldVar_tr, a_field->ty);
					}
					index ++;
				}
				// if the record do not have that field and exit the loop
				EM_error(v->u.field.var->pos, "field %s doesn't exist", S_name(v->u.field.sym));
				return expTy(Tr_Nop(), Ty_Int());
			
			}
		}

		case A_subscriptVar:{
			// a[i], a must be array and i must be an integer
			struct expty var_ty = transVar(venv, tenv, v->u.subscript.var, level, label);
			if(actual_ty(var_ty.ty)->kind != Ty_array){
				EM_error(v->u.subscript.var->pos, "array type required");
				return expTy(Tr_Nop(), Ty_Int());
			}
			else{
				struct expty exp = transExp(venv, tenv, v->u.subscript.exp, level, label);
				if(exp.ty->kind != Ty_int){
					EM_error(v->u.subscript.exp->pos, "integer required");
				}
                
				Tr_exp subscriptVar_tr = Tr_subscriptVar(var_ty.exp, exp.exp);
				return expTy(subscriptVar_tr, var_ty.ty->u.array);
			}
		}
	}
}
