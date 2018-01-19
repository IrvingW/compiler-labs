#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"

/*Lab5: Your implementation here.*/ 

const int F_wordSize = 4;
static Temp_temp eax = NULL;
static Temp_temp ebx = NULL;
static Temp_temp ecx = NULL;
static Temp_temp edx = NULL;
static Temp_temp esi = NULL;
static Temp_temp edi = NULL;
static Temp_temp ebp = NULL;
static Temp_temp esp = NULL;

int F_size(F_frame f){
	return f->inFrame_cnt * F_wordSize;
}

Temp_temp F_FP(void){
	// ebp
	if(!ebp)
		ebp = Temp_newtemp();
	return ebp;
}

Temp_temp F_SP(void){
	if(!esp)
		esp = Temp_newtemp();
	return esp;
}

Temp_temp F_RV(void){
	if(!eax)
		eax = Temp_newtemp();
	return eax;
}

Temp_temp F_eax(void){
	if(!eax)
		eax = Temp_newtemp();
	return eax;
}

Temp_temp F_ebx(void){
	if(!ebx)
		ebx = Temp_newtemp();
	return ebx;
}

Temp_temp F_ecx(void){
	if(!ecx)
		ecx = Temp_newtemp();
	return ecx;
}

Temp_temp F_edx(void){
	if(!edx)
		edx = Temp_newtemp();
	return edx;
}

Temp_temp F_esi(void){
	if(!esi)
		esi = Temp_newtemp();
	return esi;
}

Temp_temp F_edi(void){
	if(!edi)
		edi = Temp_newtemp();
	return edi;
}



F_frag F_StringFrag(Temp_label label, string str) { 
	F_frag frag = (F_frag) checked_malloc(sizeof(*frag));
	frag->kind = F_stringFrag;
	frag->u.stringg.label = label;
	//int length = strlen(str);
	//string len_str = checked_malloc(sizeof(str));
	//(int *) len_str;
	//*len_str = length;
	//frag->u.stringg.str = strcat((char *)len_str, str);
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
	f->formals = Formal_accessList(8, formals);
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

int F_Spill(F_frame f){
	f->inFrame_cnt ++;
	return (- F_wordSize * f->inFrame_cnt);	
}

T_stm F_procEntryExit1(F_frame f, T_stm body){
	// if callesave registers do not used, then it's move
	// instruction will be coalesced
   	Temp_temp bak_ebx = Temp_newtemp();
    Temp_temp bak_esi = Temp_newtemp();
    Temp_temp bak_edi = Temp_newtemp();

	T_stm store_ebx = T_Move(T_Temp(bak_ebx), T_Temp(F_ebx()));
	T_stm store_esi = T_Move(T_Temp(bak_esi), T_Temp(F_esi()));
	T_stm store_edi = T_Move(T_Temp(bak_edi), T_Temp(F_edi()));
	T_stm store = T_Seq(store_ebx, T_Seq(store_esi, store_edi));
	
	T_stm load_ebx = T_Move(T_Temp(F_ebx()), T_Temp(bak_ebx));
	T_stm load_esi = T_Move(T_Temp(F_esi()), T_Temp(bak_esi));
	T_stm load_edi = T_Move(T_Temp(F_edi()), T_Temp(bak_edi));
	T_stm load = T_Seq(load_ebx, T_Seq(load_esi, load_edi));

	return T_Seq(store, T_Seq(body, load));	
}

AS_proc F_procEntryExit3(F_frame frame, AS_instrList body){
	// do not need declare use and def set for prolog and epilog
	// because these registers(ebp, esp) do not have a color
	// don't belong to precolored nodes
	AS_proc result;
	char prolog[100];
	char epilog[100];
	sprintf(prolog, "pushl %%ebp\nmovl %%esp, %%ebp\nsubl $%d, %%esp\n", F_size(frame));
	sprintf(epilog, "leave\nret\n");
	result = AS_Proc(String(prolog), body, String(epilog));
	return result;
}

/* return sink */
AS_instrList F_procEntryExit2(AS_instrList body){
	// do not need return sink because x86 do not use special registers (ebp, esp)
	// to store data
}


Temp_tempList F_callersaves(void) {
  return //Temp_TempList(eax,
            Temp_TempList(F_edx(),
              Temp_TempList(F_ecx(), NULL));//);
}

/* return a list of backup temp for callersaves with same length*/
Temp_tempList F_backup_callersaves(void){
	Temp_tempList callersaves = F_callersaves();
	Temp_tempList result = NULL;
	while(callersaves){
		result = Temp_TempList(Temp_newtemp(), result); 
		callersaves = callersaves->tail;
	}
	return result;
}