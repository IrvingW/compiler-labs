%{
#include <stdio.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"

int yylex(void); /* function prototype */

A_exp absyn_root;

void yyerror(char *s)
{
 EM_error(EM_tokPos, "%s", s);
}
%}


%union {
	int pos;
	int ival;
	string sval;
	A_exp exp;
	A_expList explist;
	A_var var;
	A_decList declist;
	A_dec dec;
	A_efieldList efieldlist;
	A_efield  efield;
	A_namety namety;
	A_nametyList nametylist;
	A_fieldList fieldlist;
	A_field field;
	A_fundecList fundeclist;
	A_fundec fundec;
	A_ty ty;
	}

%token <sval> ID STRING
%token <ival> INT

%token 
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK 
  LBRACE RBRACE DOT 
  PLUS MINUS TIMES DIVIDE EQ NEQ LT LE GT GE UMINUS
  AND OR ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF 
  BREAK NIL
  FUNCTION VAR TYPE 

%type <exp> exp
%type <explist> params param sequencing_exps sequencing expseq
%type <var>  lvalue
%type <declist> decs
%type <dec> dec vardec
%type <efieldlist> records record
/*%type <efield> */
%type <nametylist> typedecs
%type <namety>  typedec
%type <fieldlist> typefields typefield
%type <ty> ty
%type <fundeclist> funcdecs
%type <fundec> funcdec

%start program
/* define precedence */
%nonassoc DO
%nonassoc THEN
%nonassoc ELSE
%nonassoc OF
%nonassoc ASSIGN

%left OR
%left AND
%nonassoc EQ NEQ LT GT LE GE
%left PLUS MINUS
%left TIMES DIVIDE
%left UMINUS


%%

program:	exp {absyn_root = $1;};

exp:	lvalue {$$=A_VarExp(EM_tokPos, $1);}
	|	NIL {$$=A_NilExp(EM_tokPos);}
	/* fucking the testcase 20! */
	|   LPAREN RPAREN {$$=A_SeqExp(EM_tokPos, NULL);}

	|	INT {$$=A_IntExp(EM_tokPos, $1);}
	|	STRING {$$=A_StringExp(EM_tokPos, $1);}
	|	ID LPAREN params RPAREN {$$=A_CallExp(EM_tokPos, S_Symbol($1), $3);}
	
/* op expression*/
	|	exp PLUS exp {$$=A_OpExp(EM_tokPos, A_plusOp, $1, $3);}
	|	exp MINUS exp {$$=A_OpExp(EM_tokPos, A_minusOp, $1, $3);}
	|	exp TIMES exp {$$=A_OpExp(EM_tokPos, A_timesOp, $1, $3);}
	|	exp DIVIDE exp {$$=A_OpExp(EM_tokPos, A_divideOp, $1, $3);}
	|	exp EQ exp {$$=A_OpExp(EM_tokPos, A_eqOp, $1, $3);}
	|	exp NEQ exp {$$=A_OpExp(EM_tokPos, A_neqOp, $1, $3);}
	|	exp LT exp {$$=A_OpExp(EM_tokPos, A_ltOp, $1, $3);}
	|	exp LE exp {$$=A_OpExp(EM_tokPos, A_leOp, $1, $3);}
	|	exp GT exp {$$=A_OpExp(EM_tokPos, A_gtOp, $1, $3);}
	|	exp GE exp {$$=A_OpExp(EM_tokPos, A_geOp, $1, $3);}

/* integer less than 0 */
	|	MINUS exp %prec UMINUS {$$=A_OpExp(EM_tokPos, A_minusOp, A_IntExp(EM_tokPos, 0), $2);}

	|	ID LBRACE records RBRACE {$$=A_RecordExp(EM_tokPos, S_Symbol($1), $3);}
	|	LPAREN exp RPAREN {$$=$2;}
	|	LPAREN expseq RPAREN {$$=A_SeqExp(EM_tokPos, $2);}
	|	lvalue ASSIGN exp {$$=A_AssignExp(EM_tokPos, $1, $3);}
/* this order does matter */
	|	IF exp THEN exp ELSE exp {$$=A_IfExp(EM_tokPos, $2, $4, $6);}
	|	IF exp THEN exp {$$=A_IfExp(EM_tokPos, $2, $4, NULL);}
	
	|	WHILE exp DO exp {$$=A_WhileExp(EM_tokPos, $2, $4);}
	|	BREAK {$$=A_BreakExp(EM_tokPos);}
	|	FOR ID ASSIGN exp TO exp DO exp {$$=A_ForExp(EM_tokPos, S_Symbol($2), $4, $6, $8);}
	|	LET decs IN sequencing_exps END {$$=A_LetExp(EM_tokPos, $2, A_SeqExp(EM_tokPos, $4));}
	|	ID LBRACK exp RBRACK OF exp {$$=A_ArrayExp(EM_tokPos, S_Symbol($1), $3, $6);}

/* & | */
	|	exp AND exp {$$=A_IfExp(EM_tokPos, $1, $3, A_IntExp(EM_tokPos, 0));}
	|	exp OR exp {$$=A_IfExp(EM_tokPos, $1, A_IntExp(EM_tokPos, 1), $3);}
	;

lvalue:	ID {$$=A_SimpleVar(EM_tokPos, S_Symbol($1));}
	|	lvalue DOT ID {$$=A_FieldVar(EM_tokPos, $1, S_Symbol($3));}
	|	lvalue LBRACK exp RBRACK {$$=A_SubscriptVar(EM_tokPos, $1, $3);}
	|	ID LBRACK exp RBRACK {$$=A_SubscriptVar(EM_tokPos, A_SimpleVar(EM_tokPos, S_Symbol($1)),     $3);}

	;

params:	{$$=NULL;}
	|	param {$$=$1;}
	;

param:	exp {$$=A_ExpList($1, NULL);}
	|	exp COMMA param {$$=A_ExpList($1, $3);}
	;

records: {$$=NULL;}	
	|	record {$$=$1;}
	;

record: ID EQ exp {$$=A_EfieldList(A_Efield(S_Symbol($1), $3), NULL);}
	|	ID EQ exp COMMA record {$$=A_EfieldList(A_Efield(S_Symbol($1), $3), $5);}
	;

sequencing_exps: {$$=NULL;}
	|	sequencing {$$=$1;}
	;

sequencing:	exp {$$=A_ExpList($1, NULL);}
	|	exp SEMICOLON sequencing {$$=A_ExpList($1, $3);}
	;
expseq: exp SEMICOLON exp {$$=A_ExpList($1, A_ExpList($3, NULL));}
	|	exp SEMICOLON exp SEMICOLON sequencing {$$=A_ExpList($1, A_ExpList($3, $5));}

decs:	{$$=NULL;}
	|	dec decs {$$=A_DecList($1, $2);}
	;

dec:	funcdecs {$$=A_FunctionDec(EM_tokPos, $1);}
	|	vardec {$$=$1;}
	|	typedecs {$$=A_TypeDec(EM_tokPos, $1);}
	;

vardec: VAR ID ASSIGN exp  {$$=A_VarDec(EM_tokPos,S_Symbol($2),S_Symbol(""),$4);}
    |	VAR ID COLON ID ASSIGN exp  {$$=A_VarDec(EM_tokPos,S_Symbol($2),S_Symbol($4),$6);}
	;

funcdecs: {$$=NULL;}
	|	funcdec funcdecs {$$=A_FundecList($1, $2);}
	;

funcdec: FUNCTION ID LPAREN typefields RPAREN EQ exp {$$=A_Fundec(EM_tokPos, S_Symbol($2), $4, S_Symbol(""), $7);}
	|	FUNCTION ID LPAREN typefields RPAREN COLON ID EQ exp {$$=A_Fundec(EM_tokPos, S_Symbol($2), $4, S_Symbol($7), $9);}
	;

typedecs: typedec {$$=A_NametyList($1, NULL);}
	|	typedec typedecs {$$=A_NametyList($1, $2);}
	;

typedec: TYPE ID EQ ty {$$=A_Namety(S_Symbol($2), $4);}
	;

ty:		ID {$$=A_NameTy(EM_tokPos, S_Symbol($1));}
	|	LBRACE typefields RBRACE {$$=A_RecordTy(EM_tokPos, $2);}
	|	ARRAY OF ID {$$=A_ArrayTy(EM_tokPos, S_Symbol($3));}
	;

typefields: {$$=NULL;}
	|	typefield {$$=$1;}
	;

typefield:	ID COLON ID {$$=A_FieldList(A_Field(EM_tokPos, S_Symbol($1), S_Symbol($3)), NULL);}
	|	ID COLON ID COMMA typefield {$$=A_FieldList(A_Field(EM_tokPos, S_Symbol($1), S_Symbol($3)), $5);}
	;

