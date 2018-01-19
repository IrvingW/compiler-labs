#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "errormsg.h"
#include "tree.h"
#include "assem.h"
#include "frame.h"
#include "codegen.h"
#include "table.h"

//Lab 6: put your code here
# define MAXINLINE 40

static Temp_temp munchExp(T_exp e);

static void munchStm(T_stm s);

static Temp_tempList munchArgs(int index, T_expList list);

Temp_tempList L(Temp_temp h, Temp_tempList t) {
    return Temp_TempList(h,t);
}

static AS_instrList iList = NULL;
static AS_instrList last = NULL;

static void emit(AS_instr inst){
    if(last != NULL)
        last = last->tail = AS_InstrList(inst, NULL);
    else
        last = iList = AS_InstrList(inst, NULL);
}

AS_instrList F_codegen(F_frame f, T_stmList stmList) {
    AS_instrList list;
    T_stmList sl;

    for (sl = stmList; sl; sl = sl->tail)
        munchStm(sl->head);

    list=iList;
    //list = F_procEntryExit2(iList);
    iList = last = NULL;
    return list;
}


static Temp_temp munchExp(T_exp e){
    // T_Binop
    if(e->kind == T_BINOP){
        Temp_temp left = munchExp(e->u.BINOP.left);
        Temp_temp right = munchExp(e->u.BINOP.right);
        Temp_temp r = Temp_newtemp();
        
        switch (e->u.BINOP.op){
            case T_plus:{
                // in x86 instruction, addl can only take two regiter
                emit(AS_Move("movl `s0, `d0\n", L(r, NULL), L(left, NULL)));
                emit(AS_Oper("addl `s0, `d0\n", L(r, NULL), L(right, NULL), NULL));
                return r;
            }
            case T_minus:{
                emit(AS_Move("movl `s0, `d0\n", L(r, NULL), L(left, NULL)));
                emit(AS_Oper("subl `s0, `d0\n", L(r, NULL), L(right, NULL), NULL));
                return r;
            }
            case T_mul:{
                emit(AS_Move("movl `s0, `d0\n", L(r, NULL), L(left, NULL)));
                // signed times
                emit(AS_Oper("imul `s0, `d0\n", L(r, NULL), L(right, L(r, NULL)), NULL));
                return r;
            }
            /* divide is a little triky */
            case T_div:{
                Temp_temp edx = F_edx();
                Temp_temp eax = F_eax();
                emit(AS_Move("movl `s0, `d0\n", L(eax, NULL), L(left, NULL)));
                // signed extend eax to edx
                emit(AS_Oper("cltd\n", L(edx, NULL), L(eax, NULL), NULL));
                // signed divide, store x/y into eax, store x%y into edx; use edx as high 32 bit, eax as low 32 bit
                emit(AS_Oper("idivl `s0\n", L(eax, L(edx, NULL)), L(right, L(eax, L(edx, NULL))), NULL));
                emit(AS_Move("movl `s0, `d0\n", L(r, NULL), L(eax, NULL)));
                return r;
            }
            default:
                assert(0);
        }
    }

    // T_Mem
    if(e->kind == T_MEM){
        char *inst = checked_malloc(sizeof(char) * MAXINLINE);
        Temp_temp r = Temp_newtemp();

        T_exp mem = e->u.MEM;
        // MEM(BINOP(PLUS, CONST, e))
        if(mem->kind == T_BINOP 
            && mem->u.BINOP.op == T_plus 
                && mem->u.BINOP.left->kind == T_CONST){
            Temp_temp mem_right = munchExp(mem->u.BINOP.right);
            // immediate number
            int i = mem->u.BINOP.left->u.CONST;
            sprintf(inst, "movl %d(`s0), `d0\n", i);

            emit(AS_Oper(inst, L(r, NULL), L(mem_right, NULL), NULL));
            return r;
        }
        
        // MEM(BINOP(PLUS, e, CONST))
        if(mem->kind == T_BINOP
            && mem->u.BINOP.op == T_plus
                && mem->u.BINOP.right->kind == T_CONST){
            Temp_temp mem_left = munchExp(mem->u.BINOP.left);
            // immediate number
            int i = mem->u.BINOP.right->u.CONST;
            sprintf(inst, "movl %d(`s0), `d0\n", i);

            emit(AS_Oper(inst, L(r, NULL), L(mem_left, NULL), NULL));
            return r;
        }

        // MEM(CONST)
        if(mem->kind == T_CONST){
            // immediate
            int i = mem->u.CONST;
            sprintf(inst, "movl %d, `d0\n", i);

            emit(AS_Oper(inst, L(r, NULL), NULL, NULL));
            return r;
        }

        // MEM(e)
        else{
            Temp_temp s = munchExp(mem);
            emit(AS_Oper("movl (`s0), `d0\n", L(r, NULL), L(s, NULL), NULL));
            return r;
        }
    }

    // T_Temp
    if(e->kind == T_TEMP){
        return e->u.TEMP;
    }

    // T_Eseq
    if(e->kind == T_ESEQ){
        munchStm(e->u.ESEQ.stm);
        Temp_temp r = Temp_newtemp();
        r = munchExp(e->u.ESEQ.exp);
        return r;
    }

    // T_Name
    if(e->kind == T_NAME){
        char *inst = checked_malloc(sizeof(char) * MAXINLINE);
        Temp_temp r = Temp_newtemp();
        /////////////////////////////////////
        sprintf(inst, "movl $%s, `d0\n", Temp_labelstring(e->u.NAME));
        emit(AS_Oper(inst, L(r, NULL), NULL, NULL));
        return r;
    }

    // T_Const
    if(e->kind == T_CONST){
        char *inst = checked_malloc(sizeof(char) * MAXINLINE);
        Temp_temp r = Temp_newtemp();
        sprintf(inst, "movl $%d, `d0\n", e->u.CONST);
        emit(AS_Oper(inst, L(r, NULL), NULL, NULL));
        return r;       
    }

    // T_Call
    if(e->kind == T_CALL){
        Temp_temp r = Temp_newtemp();
        Temp_temp rv = F_RV();      // return value
        //Temp_temp rv = F_RV();
        // CALL(fun, args)
        // I've introduced spill, so I do not need to push every caller save 
        // register into stack, instead, I move them to new temps
        if(e->u.CALL.fun->kind == T_NAME){
            Temp_label fun = e->u.CALL.fun->u.NAME;
            char *inst = checked_malloc(sizeof(char) * MAXINLINE);
            sprintf(inst, "call %s\n", Temp_labelstring(fun));
            // push arguments
            Temp_tempList args = munchArgs(0, e->u.CALL.args);
            Temp_tempList calldefs = F_callersaves();    /*TODO*/
            emit(AS_Oper(inst, L(F_RV(), calldefs), NULL, NULL));
            emit(AS_Move("movl `s0, `d0\n", L(r, NULL), L(rv, NULL)));  // move eax, temp
            return r;
        }
        // CALL(e, args)
        else{
            Temp_temp s = munchExp(e->u.CALL.fun);
            Temp_tempList args = munchArgs(0, e->u.CALL.args);
            Temp_tempList calldefs = F_callersaves(); /*TODO*/
            emit(AS_Oper("call *`s0\n", L(F_RV(), calldefs), L(s, NULL), NULL));
            emit(AS_Move("movl `s0, `d0\n", L(r, NULL), L(rv, NULL)));
            return r;
        }
    }

    else
        assert(0);
}

static void munchStm(T_stm s){
    // T_seq
    if(s->kind == T_SEQ){
        munchStm(s->u.SEQ.left);
        munchStm(s->u.SEQ.right);
        return;
    }

    // T_Label
    if(s->kind == T_LABEL){
        char *inst = checked_malloc(sizeof(char) * MAXINLINE);
        sprintf(inst, "%s:\n", Temp_labelstring(s->u.LABEL));
        emit(AS_Label(inst, s->u.LABEL));
        return;
    }

    // T_Jump
    if(s->kind == T_JUMP){
        switch(s->u.JUMP.exp->kind){
            case T_NAME:{
                Temp_label label = s->u.JUMP.exp->u.NAME;
                char *inst = checked_malloc(sizeof(char) * MAXINLINE);
                sprintf(inst, "jmp %s\n", Temp_labelstring(label));
                emit(AS_Oper(inst, NULL, NULL, AS_Targets(s->u.JUMP.jumps)));
                return;
            }
            default:{
                Temp_temp t = munchExp(s->u.JUMP.exp);
                emit(AS_Oper("jmp *`s0\n", NULL, NULL, AS_Targets(s->u.JUMP.jumps)));
            }
        }
    }

    // T_Cjump
    if(s->kind == T_CJUMP){
        Temp_temp left = munchExp(s->u.CJUMP.left);
        Temp_temp right = munchExp(s->u.CJUMP.right);
        char *inst = checked_malloc(sizeof(char) * MAXINLINE);
        emit(AS_Oper("cmp `s0, `s1\n", NULL, L(right, L(left, NULL)), NULL));
        
        switch(s->u.CJUMP.op){
            // caution: cmp e1, e2 is based on e2-e1
            case T_eq:
                sprintf(inst, "je %s\n", Temp_labelstring(s->u.CJUMP.true));
                break;
            case T_ne:
                sprintf(inst, "jne %s\n", Temp_labelstring(s->u.CJUMP.true));
                break;
            case T_lt:
                sprintf(inst, "jl %s\n", Temp_labelstring(s->u.CJUMP.true));
                break;
            case T_le:
                sprintf(inst, "jle %s\n", Temp_labelstring(s->u.CJUMP.true));
                break;
            case T_gt:
                sprintf(inst, "jg %s\n", Temp_labelstring(s->u.CJUMP.true));
                break;
            case T_ge:
                sprintf(inst, "jge %s\n", Temp_labelstring(s->u.CJUMP.true));
                break;
            default:
                assert(0);
        }
        emit(AS_Oper(inst, NULL, NULL, AS_Targets(Temp_LabelList(s->u.CJUMP.true, NULL))));                
        return;
    }

    // T_Move
    
    if (s->kind == T_MOVE && s->u.MOVE.dst->kind == T_TEMP &&
            s->u.MOVE.src->kind == T_MEM &&
            s->u.MOVE.src->u.MEM->kind == T_BINOP &&
            s->u.MOVE.src->u.MEM->u.BINOP.op == T_plus &&
            s->u.MOVE.src->u.MEM->u.BINOP.right->kind == T_CONST) {
        Temp_temp dst = s->u.MOVE.dst->u.TEMP;
        Temp_temp r = munchExp(s->u.MOVE.src->u.MEM->u.BINOP.left);
        int off = s->u.MOVE.src->u.MEM->u.BINOP.right->u.CONST;
        char *a = checked_malloc(MAXINLINE * sizeof(char));
        sprintf(a, "movl %d(`s0), `d0\n", off);
        emit(AS_Oper(a, L(dst, NULL), L(r, NULL), AS_Targets(NULL)));
        return;
    }
    if (s->kind == T_MOVE && s->u.MOVE.dst->kind == T_TEMP &&
            s->u.MOVE.src->kind == T_MEM &&
            s->u.MOVE.src->u.MEM->kind == T_BINOP &&
            s->u.MOVE.src->u.MEM->u.BINOP.op == T_plus &&
            s->u.MOVE.src->u.MEM->u.BINOP.left->kind == T_CONST) {
        Temp_temp dst = s->u.MOVE.dst->u.TEMP;
        Temp_temp r = munchExp(s->u.MOVE.src->u.MEM->u.BINOP.right);
        int off = s->u.MOVE.src->u.MEM->u.BINOP.left->u.CONST;
        char *a = checked_malloc(MAXINLINE * sizeof(char));
        sprintf(a, "movl %d(`s0), `d0\n", off);
        emit(AS_Oper(a, L(dst, NULL), L(r, NULL), AS_Targets(NULL)));
        return;
    }
    if (s->kind == T_MOVE && s->u.MOVE.dst->kind == T_MEM &&
            s->u.MOVE.dst->u.MEM->kind == T_BINOP &&
            s->u.MOVE.dst->u.MEM->u.BINOP.op == T_plus &&
            s->u.MOVE.dst->u.MEM->u.BINOP.right->kind == T_CONST) {
        Temp_temp src = munchExp(s->u.MOVE.src);
        Temp_temp r = munchExp(s->u.MOVE.dst->u.MEM->u.BINOP.left);
        int off = s->u.MOVE.dst->u.MEM->u.BINOP.right->u.CONST;
        char *a = checked_malloc(MAXINLINE * sizeof(char));
        sprintf(a, "movl `s0, %d(`s1)\n", off);
        emit(AS_Oper(a, NULL, L(src, L(r, NULL)), AS_Targets(NULL)));
        return;
    }
    if (s->kind == T_MOVE && s->u.MOVE.dst->kind == T_MEM &&
            s->u.MOVE.dst->u.MEM->kind == T_BINOP &&
            s->u.MOVE.dst->u.MEM->u.BINOP.op == T_plus &&
            s->u.MOVE.dst->u.MEM->u.BINOP.left->kind == T_CONST) {
        Temp_temp src = munchExp(s->u.MOVE.src);
        Temp_temp r = munchExp(s->u.MOVE.dst->u.MEM->u.BINOP.right);
        int off = s->u.MOVE.dst->u.MEM->u.BINOP.left->u.CONST;
        char *a = checked_malloc(MAXINLINE * sizeof(char));
        sprintf(a, "movl `s0, %d(`s1)\n", off);
        emit(AS_Oper(a, NULL, L(src, L(r, NULL)), AS_Targets(NULL)));
        return;
    }   
     if (s->kind == T_MOVE && s->u.MOVE.dst->kind == T_MEM) {
        Temp_temp src = munchExp(s->u.MOVE.src);
        Temp_temp dst = munchExp(s->u.MOVE.dst->u.MEM);
        emit(AS_Oper("movl `s0, (`s1)\n", NULL, L(src, L(dst, NULL)), AS_Targets(NULL)));
        return;
    }
    if (s->kind == T_MOVE && s->u.MOVE.dst->kind == T_TEMP && s->u.MOVE.src->kind == T_TEMP) {
        Temp_temp src = s->u.MOVE.src->u.TEMP;
        Temp_temp dst = s->u.MOVE.dst->u.TEMP;
        emit(AS_Move("movl `s0, `d0\n", L(dst, NULL), L(src, NULL)));
        return;
    }
    if (s->kind == T_MOVE && s->u.MOVE.dst->kind == T_TEMP) {
        Temp_temp src = munchExp(s->u.MOVE.src);
        Temp_temp dst = s->u.MOVE.dst->u.TEMP;
        emit(AS_Move("movl `s0, `d0\n", L(dst, NULL), L(src, NULL)));
        return;
    }


    // T_Exp
    if(s->kind == T_EXP){
        munchExp(s->u.EXP);
        return;
    }
    
    assert(0);  // the code should not reach here 

}


static Temp_tempList munchArgs(int index, T_expList args){
    if(args == NULL){
        return NULL;
    }
    
    Temp_tempList tail_list = munchArgs(index + 1, args->tail);
    
    Temp_temp head = munchExp(args->head);
    //Temp_temp sp = F_SP();
    //Temp_temp sp = F_SP();
    // push args into stack
    //emit(AS_Oper("pushl `s0\n", L(sp, NULL), L(head, NULL), NULL));
    emit(AS_Oper("pushl `s0\n", NULL, L(head, NULL), NULL));
    return Temp_TempList(head, tail_list);   /* TODO */ // do not need return any more
}
