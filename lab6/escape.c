#include <stdio.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "escape.h"
#include "table.h"

escapeEntry EscapeEntry(int depth, bool *escape){
	escapeEntry e = checked_malloc(sizeof(*e));
	e->depth = depth;
	e->escape = escape;
	return e;
}
// This part is similar to semant part
static void traverseExp(S_table env, int depth, A_exp e);
static void traverseDec(S_table env, int depth, A_dec d);
static void traverseVar(S_table env, int depth, A_var v);


void Esc_findEscape(A_exp exp) {
	//your code here
	traverseExp(S_empty(), 0, exp);
}

static void traverseExp(S_table env, int depth, A_exp e)
{
    switch(e->kind) {
        case A_varExp:
            traverseVar(env, depth, e->u.var);
            break;
        case A_nilExp:
			break;
        case A_intExp:
			break;
        case A_stringExp:
			break;
        case A_breakExp:
            break;
        case A_callExp:
            // a var may be used in call's args
            for (A_expList arg = e->u.call.args; arg; arg = arg->tail) {
                traverseExp(env, depth, arg->head);
            }
            break;
        case A_opExp:
            traverseExp(env, depth, e->u.op.left);
            traverseExp(env, depth, e->u.op.right);
            break;
        case A_recordExp:{
			A_efieldList ef = e->u.record.fields;
            for (; ef; ef = ef->tail) {
                traverseExp(env, depth, ef->head->exp);
            }
            break;
		}
        case A_seqExp:{
			A_expList cur = e->u.seq;
            for (; cur; cur = cur->tail) {
                traverseExp(env, depth, cur->head);
            }
            break;
		}
        case A_assignExp:
            traverseVar(env, depth, e->u.assign.var);
            traverseExp(env, depth, e->u.assign.exp);
            break;
        case A_ifExp:
            traverseExp(env, depth, e->u.iff.test);
            traverseExp(env, depth, e->u.iff.then);
            if (e->u.iff.elsee) {
                traverseExp(env, depth, e->u.iff.elsee);
            }
            break;
        case A_whileExp:
            traverseExp(env, depth, e->u.whilee.test);
            traverseExp(env, depth, e->u.whilee.body);
            break;
        case A_forExp:
            traverseExp(env, depth, e->u.forr.lo);
            traverseExp(env, depth, e->u.forr.hi);
            S_beginScope(env);	// enter a loop, begin a new environment
            e->u.forr.escape = FALSE;
            S_enter(env, e->u.forr.var, EscapeEntry(depth, &e->u.forr.escape));
            traverseExp(env, depth, e->u.forr.body);
            S_endScope(env);	// shut the env
            break;
        case A_letExp:
            S_beginScope(env); // start a new env
            for (A_decList d = e->u.let.decs; d; d = d->tail) {
                traverseDec(env, depth, d->head);
            }
            traverseExp(env, depth, e->u.let.body);
            S_endScope(env);	// shut the env
            break;
        case A_arrayExp:
            traverseExp(env, depth, e->u.array.size);
            traverseExp(env, depth, e->u.array.init);
            break;
    }
}
/* where a using of var may happen */
static void traverseVar(S_table env, int depth, A_var v)
{
    switch(v->kind) {
        case A_simpleVar:
            {
                escapeEntry e = S_look(env, v->u.simple);
				bool e_depth = e->depth;
                if (e_depth < depth) {
					bool * escape = e->escape;
                    *(escape) = TRUE;
                }
                break;
            }
        case A_fieldVar:
            traverseVar(env, depth, v->u.field.var);
            break;
        case A_subscriptVar:
            traverseVar(env, depth, v->u.subscript.var);
            traverseExp(env, depth, v->u.subscript.exp);
            break;
        default:
            assert(0); // the code should not reach here
    }
}

static void traverseDec(S_table env, int depth, A_dec d)
{
    switch (d->kind) {
        case A_varDec:
            traverseExp(env, depth, d->u.var.init);
            d->u.var.escape = FALSE;
			escapeEntry entry = EscapeEntry(depth, &d->u.var.escape);
            S_enter(env, d->u.var.var, entry);
            break;
        case A_functionDec:
            for (A_fundecList fun = d->u.function; fun; fun = fun->tail) {
				// entry a new function, start a new env
				S_beginScope(env);
				A_fieldList l = fun->head->params;
                for (; l; l = l->tail) {
                    l->head->escape = FALSE;
					escapeEntry entry = EscapeEntry(depth + 1, &l->head->escape);
                    S_enter(env, l->head->name, entry);
                }
                traverseExp(env, depth + 1, fun->head->body);
                S_endScope(env);	// shut it
            }
            break;
        case A_typeDec:
            break;
    }
}


