%option noyywrap noinput nounput never-interactive nodefault
%x COMMENT STRING
%{
#include <stdio.h>
#include "parser.tab.h"
#include "parse_context.h"
static int leaf(const char *kind, int token);
static int line = 1, column = 1;
static int opener_line, opener_column;
static void locate(void);
static int fail_at(int l, int c, const char *reason);
#define YY_USER_ACTION locate();
%}
DIGIT  [0-9]
IDENT  [A-Za-z_][A-Za-z0-9_]*
HEX    0[xX][0-9A-Fa-f]+
BADNUM ({DIGIT}[A-Za-z0-9_.]*|"."{DIGIT}[A-Za-z0-9_.]*)
%%
"let"       { return KW_LET; }
"int"       { return KW_INT; }
"print"     { return KW_PRINT; }
"if"        { return KW_IF; }
"else"      { return KW_ELSE; }
"while"     { return KW_WHILE; }
"for"       { return KW_FOR; }
{IDENT}     { return leaf("Identifier", IDENTIFIER); }
{HEX}       { return leaf("Integer", INT_LITERAL); }
{DIGIT}+    { return leaf("Integer", INT_LITERAL); }
{BADNUM}    { return fail_at(yylloc.first_line, yylloc.first_column,
                            "invalid integer spelling"); }
"=="        { return EQ; }
"!="        { return NE; }
"<="        { return LE; }
">="        { return GE; }
"<"         { return LT; }
">"         { return GT; }
"="         { return ASSIGN; }
"+"         { return PLUS; }
"-"         { return MINUS; }
"*"         { return STAR; }
"/"         { return SLASH; }
"%"         { return PERCENT; }
";"         { return SEMICOLON; }
"("         { return LPAREN; }
")"         { return RPAREN; }
"{"         { return LBRACE; }
"}"         { return RBRACE; }
"//"[^\n]*  { /* Skip a line comment. */ }
"/*"        {
    opener_line = yylloc.first_line;
    opener_column = yylloc.first_column;
    BEGIN(COMMENT);
}
<COMMENT>"*/"    { BEGIN(INITIAL); }
<COMMENT>[^*\n]+ { /* Skip comment text. */ }
<COMMENT>"*"     { /* A star without a closing slash. */ }
<COMMENT>\n      { /* locate() counts this newline. */ }
<COMMENT><<EOF>> {
    BEGIN(INITIAL);
    return fail_at(opener_line, opener_column, "unterminated block comment");
}
\" {
    opener_line = yylloc.first_line;
    opener_column = yylloc.first_column;
    BEGIN(STRING);
    return STRING_BEGIN;
}
<STRING>[^"\r\n\x00]+ { return leaf("String", STRING_TEXT); }
<STRING>\"         { BEGIN(INITIAL); return STRING_END; }
<STRING>(\r\n|\r|\n) {
    BEGIN(INITIAL);
    return fail_at(opener_line, opener_column, "unterminated string");
}
<STRING>\x00 { return fail_at(yylloc.first_line, yylloc.first_column, "NUL in string"); }
<STRING><<EOF>> {
    BEGIN(INITIAL);
    return fail_at(opener_line, opener_column, "unterminated string");
}
[ \t\r\n]+ { /* Skip whitespace but update its location. */ }
. { return fail_at(yylloc.first_line, yylloc.first_column,
                  "unexpected character"); }
<<EOF>> {
    yylloc.first_line = yylloc.last_line = line;
    yylloc.first_column = yylloc.last_column = column;
    return 0;
}
%%
static void locate(void) {
    yylloc.first_line = line;
    yylloc.first_column = column;
    for (int i = 0; i < yyleng; ++i) {
        yylloc.last_line = line;
        yylloc.last_column = column;
        if (yytext[i] == '\n') { ++line; column = 1; }
        else { ++column; }
    }
}
static int fail_at(int l, int c, const char *reason) {
    parse_context.had_error = 1;
    fprintf(stderr, "LEX_ERROR %d:%d %s\n", l, c, reason);
    return LEX_ERROR;
}

static int leaf(const char *kind, int token) {
    yylval.node = ast_new(&parse_context.tree, kind, yytext);
    if (!yylval.node) {
        parse_context.out_of_memory = 1;
        return LEX_ERROR;
    }
    return token;
}
