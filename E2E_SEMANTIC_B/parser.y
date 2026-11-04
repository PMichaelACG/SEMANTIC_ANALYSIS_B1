/* v2.0: build and check nodes during reductions. No program execution. */
%require "3.8"
%code requires {
#include "ast.h"
#include "semantic.h"
}
%{
#include "parse_context.h"
#include <stdio.h>
int yylex(void);
void yyerror(const char *message);

static AstNode *one(const char *kind, const char *text,
                    const char *role, AstNode *child) {
    AstNode *node = ast_new(&parse_context.tree, kind, text);
    if (!node || !ast_add_child(node, role, child)) return NULL;
    return node;
}
static AstNode *two(const char *kind, const char *text,
                    const char *r1, AstNode *a, const char *r2, AstNode *b) {
    AstNode *node = one(kind, text, r1, a);
    if (!node || !ast_add_child(node, r2, b)) return NULL;
    return node;
}
%}
%union { AstNode *node; const char *op; SemanticScope *scope; }
%token <node> IDENTIFIER INT_LITERAL
%token KW_LET KW_INT KW_PRINT
%token ASSIGN PLUS MINUS SEMICOLON LPAREN RPAREN LEX_ERROR
%start program
/* The Ast allocation list owns every node, including discarded token values.
 * No %destructor frees nodes: main.c destroys the entire Ast on every exit. */
%locations
%token KW_IF KW_ELSE KW_WHILE KW_FOR
%token STRING_BEGIN STRING_END
%token <node> STRING_TEXT
%token EQ NE LT LE GT GE STAR SLASH PERCENT LBRACE RBRACE
%type <node> program_items block_items statement block assignment
%type <node> expression term unary primary print_item quoted_text text_opt condition
%type <op> relop
%type <scope> block_open
/* A discarded block opener must restore the enclosing scope. */
%destructor { semantic_leave_scope(&parse_context.semantic, $$); } <scope>
%%
program: program_items { parse_context.root = $1; } ;
/* Separate lists keep the program nonempty and the block possibly empty. */
program_items:
    statement {
        $$ = one("Program", NULL, "statement", $1);
        if (!$$) YYNOMEM;
    }
  | program_items statement {
        if (!ast_add_child($1, "statement", $2)) YYNOMEM;
        $$ = $1;
    }
;
block_items:
    %empty {
        $$ = ast_new(&parse_context.tree, "Block", NULL);
        if (!$$) YYNOMEM;
    }
  | block_items statement {
        if (!ast_add_child($1, "statement", $2)) YYNOMEM;
        $$ = $1;
    }
;
statement:
    KW_LET IDENTIFIER ASSIGN expression SEMICOLON {
        $$ = two("Declaration", "let", "name", $2, "initializer", $4);
        if (!$$) YYNOMEM;
        semantic_statement(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | assignment SEMICOLON { $$ = $1; }
  | KW_PRINT LPAREN print_item RPAREN SEMICOLON {
        $$ = one("Print", NULL, "value", $3);
        if (!$$) YYNOMEM;
        semantic_statement(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | block { $$ = $1; }
  | KW_IF LPAREN condition RPAREN block {
        $$ = two("If", NULL, "condition", $3, "then", $5);
        if (!$$) YYNOMEM;
        semantic_statement(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | KW_IF LPAREN condition RPAREN block KW_ELSE block {
        $$ = two("If", NULL, "condition", $3, "then", $5);
        if (!$$ || !ast_add_child($$, "else", $7)) YYNOMEM;
        semantic_statement(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | KW_WHILE LPAREN condition RPAREN block {
        $$ = two("While", NULL, "condition", $3, "body", $5);
        if (!$$) YYNOMEM;
        semantic_statement(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }

;
/* Named early action: reduce immediately after the opening brace.
 * Scanner lookahead may be fetched early; it does no binding lookup. */
block_open:
    LBRACE {
        $$ = semantic_enter_scope(&parse_context.semantic);
        if (parse_context.semantic.enabled && !$$) YYNOMEM;
    }
;
block:
    block_open block_items RBRACE {
        $$ = $2;
        semantic_close_block(&parse_context.semantic, $1, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
;
assignment:
    IDENTIFIER ASSIGN expression {
        $$ = two("Assign", NULL, "target", $1, "value", $3);
        if (!$$) YYNOMEM;
        semantic_statement(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
;
print_item: expression { $$ = $1; } | quoted_text { $$ = $1; } ;
quoted_text:
    STRING_BEGIN text_opt STRING_END {
        $$ = $2;
        semantic_expression(&parse_context.semantic, $$);
    }
;
text_opt:
    %empty {
        $$ = ast_new(&parse_context.tree, "String", "");
        if (!$$) YYNOMEM;
    }
  | STRING_TEXT { $$ = $1; }
;
condition:
    expression relop expression {
        $$ = two("Binary", $2, "left", $1, "right", $3);
        if (!$$) YYNOMEM;
        semantic_expression(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
;
relop:
    EQ { $$ = "=="; } | NE { $$ = "!="; }
  | LT { $$ = "<"; }  | LE { $$ = "<="; }
  | GT { $$ = ">"; }  | GE { $$ = ">="; }
;
expression:
    expression PLUS term {
        $$ = two("Binary", "+", "left", $1, "right", $3);
        if (!$$) YYNOMEM;
        semantic_expression(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | expression MINUS term {
        $$ = two("Binary", "-", "left", $1, "right", $3);
        if (!$$) YYNOMEM;
        semantic_expression(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | term { $$ = $1; }
;
term:
    term STAR unary {
        $$ = two("Binary", "*", "left", $1, "right", $3);
        if (!$$) YYNOMEM;
        semantic_expression(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | term SLASH unary {
        $$ = two("Binary", "/", "left", $1, "right", $3);
        if (!$$) YYNOMEM;
        semantic_expression(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | term PERCENT unary {
        $$ = two("Binary", "%", "left", $1, "right", $3);
        if (!$$) YYNOMEM;
        semantic_expression(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | unary { $$ = $1; }
;
unary:
    PLUS unary {
        $$ = one("Unary", "+", "operand", $2); if (!$$) YYNOMEM;
        semantic_expression(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | MINUS unary {
        $$ = one("Unary", "-", "operand", $2); if (!$$) YYNOMEM;
        semantic_expression(&parse_context.semantic, $$);
        if (parse_context.semantic.internal_error) YYNOMEM;
    }
  | primary { $$ = $1; }
;
primary:
    INT_LITERAL {
        $$ = $1;
        semantic_expression(&parse_context.semantic, $$);
    }
  | IDENTIFIER {
        $$ = $1;
        semantic_expression(&parse_context.semantic, $$);
    }
  | LPAREN expression RPAREN { $$ = $2; }
;
%%
void yyerror(const char *message) {
    if (!parse_context.had_error && !parse_context.out_of_memory)
        fprintf(stderr, "PARSER_ERROR %d:%d %s\n",
                yylloc.first_line, yylloc.first_column, message);
    parse_context.had_error = 1;
}
