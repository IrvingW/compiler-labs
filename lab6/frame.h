
/*Lab5: This header file is not complete. Please finish it with more definition.*/

#ifndef FRAME_H
#define FRAME_H

#include "tree.h"
#include "temp.h"
#include "util.h"
#include "assem.h"



typedef struct F_access_ *F_access;
struct F_access_ {
	enum {inFrame, inReg} kind;
	union {
		int offset; //inFrame
		Temp_temp reg; //inReg
	} u;
};
typedef struct F_accessList_ *F_accessList;

struct F_accessList_ {F_access head; F_accessList tail;};

typedef struct F_frame_ *F_frame;
struct F_frame_{
	Temp_label name;
	F_accessList formals;
	int inFrame_cnt;
};


/* declaration for fragments */
typedef struct F_frag_ *F_frag;
struct F_frag_ {enum {F_stringFrag, F_procFrag} kind;
			union {
				struct {Temp_label label; string str;} stringg;
				struct {T_stm body; F_frame frame;} proc;
			} u;
};

F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);

typedef struct F_fragList_ *F_fragList;
struct F_fragList_ 
{
	F_frag head; 
	F_fragList tail;
};

F_fragList F_FragList(F_frag head, F_fragList tail);


// Here is where my work begins

Temp_map F_tempMap;

extern const int F_wordSize;
Temp_temp F_FP(void);
Temp_temp F_SP(void);
Temp_temp F_RV(void);
Temp_temp F_eax(void);
Temp_temp F_ebx(void);
Temp_temp F_ecx(void);
Temp_temp F_edx(void);
Temp_temp F_esi(void);
Temp_temp F_edi(void);

int F_size(F_frame f);


/* constructor of F_accessList */
F_accessList F_AccessList(F_access head, F_accessList tail);

/* make a new frame */
F_frame F_newFrame(Temp_label name, U_boolList formals);

/* get the name of a frame */
Temp_label F_name(F_frame f);

/* get the formals of a frame */
F_accessList F_formals(F_frame f);

/* alloc a F_access to store a variable */
F_access F_allocLocal(F_frame f, bool escape);

/* get the value through F_access */
T_exp F_Exp(F_access access, T_exp fp);

/* call a C function with arguments */
T_exp F_externalCall(string s, T_expList args);

/* spill a temp, increase frame size */
int F_Spill(F_frame f);

/* insert prologue and epilogue */
AS_proc F_procEntryExit3(F_frame f, AS_instrList body);

/* deal with callesaves */
T_stm F_procEntryExit1(F_frame f, T_stm body);

/* return caller save registers of x86 */
Temp_tempList F_callersaves(void);

/* return a list of backup temp for callersaves with same length*/
Temp_tempList F_backup_callersaves(void);

/* return sink */
AS_instrList F_procEntryExit2(AS_instrList body);

#endif
