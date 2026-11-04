#ifndef QUECTOC_SEMANTIC_H
#define QUECTOC_SEMANTIC_H
#include "ast.h"
#include <stdio.h>

typedef struct {
    const char *name;
    Type type;
    AstNode *declaration;
    size_t scope_id;
} Symbol;
typedef struct {
    Symbol *symbols; /* Includes declarations from closed scopes. */
    size_t count, capacity, errors;
} SemanticResult;

typedef struct SemanticScope SemanticScope;
typedef struct {
    SemanticResult result;
    SemanticScope *scope;
    size_t next_scope;
    FILE *diagnostics, *trace;
    int enabled, internal_error;
} SemanticState;

/* Zero-initialize state. One input per process, as in v1.0. */
bool semantic_begin(SemanticState *s, int enabled, FILE *diagnostics, FILE *trace);
SemanticScope *semantic_enter_scope(SemanticState *s);
void semantic_leave_scope(SemanticState *s, SemanticScope *guard);
void semantic_close_block(SemanticState *s, SemanticScope *guard, AstNode *block);

/* Local actions only: children have already been checked by earlier reductions.
 * Neither function recursively visits the AST. */
void semantic_expression(SemanticState *s, AstNode *node);
void semantic_statement(SemanticState *s, AstNode *node);

/* 0 success, 1 semantic error, 2 internal/resource/I/O error.
 * completed is true only when yyparse accepted the input. */
int semantic_status(SemanticState *s, int completed);
void semantic_destroy(SemanticState *s);
const char *semantic_type_name(Type type);
int semantic_write_dot(FILE *out, const AstNode *root);
#endif
