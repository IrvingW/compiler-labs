%{
/* Lab2 Attention: You are only allowed to add code in this file and start at Line 26.*/
#include <string.h>
#include "util.h"
#include "tokens.h"
#include "errormsg.h"

int charPos=1;

int yywrap(void)
{
 charPos=1;
 return 1;
}

void adjust(void)
{
 EM_tokPos=charPos;
 charPos+=yyleng;
}

/*
* Please don't modify the lines above.
* You can add C declarations of your own below.
*/

#define MAX_STR_LEN 4096
static int position;
static char strbuf[MAX_STR_LEN];
static int comment_depth;

%}
  /* You can add lex definitions here. */
letter [a-zA-Z]
digits [0-9]

%Start NORMAL COMMENT STR

%%
  /* 
  * Below is an example, which you can wipe out
  * and write reguler expressions and actions of your own.
  */ 


<NORMAL>"while" {adjust(); return WHILE;}
<NORMAL>"for" {adjust(); return FOR;}
<NORMAL>"to" {adjust(); return TO;}
<NORMAL>"break" {adjust(); return BREAK;}
<NORMAL>"let" {adjust(); return LET;}
<NORMAL>"in" {adjust(); return IN;}
<NORMAL>"end" {adjust(); return END;}
<NORMAL>"function" {adjust(); return FUNCTION;}
<NORMAL>"var" {adjust(); return VAR;}
<NORMAL>"type" {adjust(); return TYPE;}
<NORMAL>"array" {adjust(); return ARRAY;}
<NORMAL>"if" {adjust(); return IF;}
<NORMAL>"then" {adjust(); return THEN;}
<NORMAL>"else" {adjust(); return ELSE;}
<NORMAL>"do" {adjust(); return DO;}
<NORMAL>"of" {adjust(); return OF;}
<NORMAL>"nil" {adjust(); return NIL;}


<NORMAL>{letter}({letter}|{digits}|_)* {adjust(); yylval.sval=String(yytext); return ID;}
<NORMAL>{digits}+ {adjust(); yylval.ival=atoi(yytext); return INT;}

<NORMAL>"," {adjust(); return COMMA;}
<NORMAL>":" {adjust(); return COLON;}
<NORMAL>";" {adjust(); return SEMICOLON;}
<NORMAL>"(" {adjust(); return LPAREN;}
<NORMAL>")" {adjust(); return RPAREN;}
<NORMAL>"[" {adjust(); return LBRACK;}
<NORMAL>"]" {adjust(); return RBRACK;}
<NORMAL>"{" {adjust(); return LBRACE;}
<NORMAL>"}" {adjust(); return RBRACE;}
<NORMAL>"." {adjust(); return DOT;}
<NORMAL>"+" {adjust(); return PLUS;}
<NORMAL>"-" {adjust(); return MINUS;}
<NORMAL>"*" {adjust(); return TIMES;}
<NORMAL>"/" {adjust(); return DIVIDE;}
<NORMAL>"=" {adjust(); return EQ;}
<NORMAL>"<>" {adjust(); return NEQ;}
<NORMAL>"<" {adjust(); return LT;}
<NORMAL>"<=" {adjust(); return LE;}
<NORMAL>">" {adjust(); return GT;}
<NORMAL>">=" {adjust(); return GE;}
<NORMAL>"&" {adjust(); return AND;}
<NORMAL>"|" {adjust(); return OR;}
<NORMAL>":=" {adjust(); return ASSIGN;}

<NORMAL>" "|\t {adjust(); continue;}
<NORMAL>\n {adjust(); EM_newline(); continue;}

<NORMAL>"/*" {adjust(); comment_depth=1; BEGIN COMMENT;}
<NORMAL>\" {adjust(); position=0; BEGIN STR;}
<NORMAL>\"\" {adjust(); yylval.sval = NULL; return STRING;}

<NORMAL>. {adjust(); EM_error(EM_tokPos, "illegal token");}


<COMMENT>"/*" {adjust(); comment_depth++;}
<COMMENT>"*/" {adjust(); comment_depth--; if(comment_depth==0) BEGIN NORMAL;}
<COMMENT>\n {adjust(); EM_newline(); continue;}
<COMMENT>. {adjust();}

<STR>\" {charPos += yyleng; strbuf[position]='\0'; yylval.sval = String(strbuf); BEGIN NORMAL; return STRING;}
<STR>\\n {charPos += yyleng; strbuf[position]='\n'; position++;}
<STR>\\t {charPos += yyleng; strbuf[position]='\t'; position++;}

<STR>\\{digits}{3} {charPos += yyleng; strbuf[position] = atoi(yytext+1); position++;}
<STR>\\\" {charPos += yyleng; strbuf[position]='"'; position++;}
<STR>"\\\\" {charPos += yyleng; strbuf[position]='\\'; position++;}
<STR>\\[ \t\n\f]+\\ {charPos += yyleng;}
<STR>. {charPos += yyleng; strcpy(strbuf+position, yytext); position += yyleng;}

. {BEGIN NORMAL; yyless(0);}
