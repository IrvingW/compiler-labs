#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"

/*Lab5: Your implementation here.*/ const int F_wordSize = 4;

Temp_temp F_FP(void){
	// ebp
	return Temp_newtemp();
}

struct F_frame_{
	Temp_label name;
	F_accessList formals;
	int inFrame_cnt;
};


struct F_access_ {
	enum {inFrame, inReg} kind;
	union {
		int offset; //inFrame
		Temp_temp reg; //inReg
	} u;
};

F_frag F_StringFrag(Temp_label label, string str) {   
	F_frag frag = (F_frag) checked_malloc(sizeof(*frag));
	frag->kind = F_stringFrag;
	frag->u.stringg.label = label;
	frag->u.stringg.str = str;
	return frag;
}                                                     
                                                      
F_frag F_ProcFrag(T_stm body, F_frame frame) {        
	F_frag frag = (F_frag) checked_malloc(sizeof(*frag));
	frag->kind = F_procFrag;
	frag->u.proc.body = body;
	frag->u.proc.frame = frame;
	return frag;
}                                                     
                                                      
F_fragList F_FragList(F_frag head, F_fragList tail) { 
	F_fragList frags = (F_fragList) checked_malloc(sizeof(* frags));
	frags->head = head;
	frags->tail = tail;
	return frags;
}                

/* construct a F_access, whose kind is inFrame*/
static F_access InFrame(int offset){
	F_access a = (F_access) checked_malloc(sizeof(*a));
	a->kind = inFrame;
	a->u.offset = offset;
	return a;
}

/* construct a F_access, whose kind is inReg */
static F_access InReg(Temp_temp t){
	F_access a = (F_access) checked_malloc(sizeof(*a));
	a->kind = inReg;
	a->u.reg = t;
	return a;
}

/* constructor of F_accessList */
F_accessList F_AccessList(F_access head, F_accessList tail){
	F_accessList list = (F_accessList)checked_malloc(sizeof(* list));
	list->head = head;
	list->tail = tail;
	return list;

}

/* used by F_newFrame, used to construct a formal list(F_accessList) 
	through a U_boolList formals */
static F_accessList Formal_accessList(int offset, U_boolList formals){
	if(formals != NULL){
		return F_AccessList(InFrame(offset), Formal_accessList(offset + 4, formals->tail));
	}else{
		return NULL;
	}
} 


/* make a new frame */
F_frame F_newFrame(Temp_label name, U_boolList formals){
	F_frame f = (F_frame) checked_malloc(sizeof(*f));	
	f->name = name;
	f->formals = Formal_accessList(4, formals);
	f->inFrame_cnt = 0;
	return f;
}

/* get the name of a frame */
Temp_label F_name(F_frame f){
	return f->name;
}

/* get the formals of a frame */
F_accessList F_formals(F_frame f){
	return f->formals;
}

/* alloc a F_access to store a variable */
F_access F_allocLocal(F_frame f, bool escape){
	if(escape){
		f->inFrame_cnt ++;
		return InFrame(-(F_wordSize * f->inFrame_cnt)); // attention, x86's frame is from high address to low address
	}else{
		return InReg(Temp_newtemp());
	}
}
/* get the value through F_access, turn a F_access into a tree structure */
T_exp F_Exp(F_access access, T_exp fp){
	if (access->kind == inFrame)
	{
		return T_Mem(T_Binop(T_plus, fp, T_Const(access->u.offset)));
	} else
	{
		return T_Temp(access->u.reg);
	}
}

/* call a C function with arguments */
T_exp F_externalCall(string s, T_expList args){
	return T_Call(T_Name(Temp_namedlabel(s)), args);
}
