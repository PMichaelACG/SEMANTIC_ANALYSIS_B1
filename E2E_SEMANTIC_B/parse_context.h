#ifndef QUECTOC_PARSE_CONTEXT_H
#define QUECTOC_PARSE_CONTEXT_H
#include "ast.h"
#include "semantic.h"
/* One input per process. The tree owns every allocated node. */
typedef struct {
    Ast tree;
    AstNode *root;
    SemanticState semantic;
    int had_error;
    int out_of_memory;
} ParseContext;
extern ParseContext parse_context;
#endif
