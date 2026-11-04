#ifndef QUECTOC_AST_H
#define QUECTOC_AST_H

#include <stdbool.h>
#include <stddef.h>

/* Generic, ordered AST. Use the functions below to create/attach nodes.
 * The field definitions are visible to keep this teaching example small.
 * Do not modify links, IDs, ownership, or allocated strings directly.
 */
typedef enum { TYPE_UNKNOWN = 0, TYPE_INT, TYPE_BOOL, TYPE_STRING, TYPE_ERROR } Type;

typedef struct AstNode AstNode;
typedef struct {
    AstNode *allocations;
    size_t next_id;
} Ast;

struct AstNode {
    size_t id;
    Type inferred_type;
    AstNode *binding; /* borrowed declaration pointer */
    size_t scope_id;
    char *kind;                  /* e.g. "Binary", "While", "Integer" */
    char *text;                  /* optional payload: "+", "12", ... */
    char *role;                  /* label on the edge from its parent */
    AstNode *parent;
    AstNode *first_child;
    AstNode *last_child;
    AstNode *next_sibling;
    AstNode *allocation_next;
    Ast *owner;
};

/* Start with: Ast tree = {0}; Do not copy/move it while its nodes are alive.
 * Strings are copied; callers may reuse their source buffers immediately.
 * Returns NULL on allocation failure or an invalid kind/tree argument.
 */
AstNode *ast_new(Ast *tree, const char *kind, const char *text);

/* Appends an ordered edge. Role must be nonempty and is copied.
 * A child may have exactly one parent. Cross-tree links and cycles are rejected.
 * Returns false without changing the tree if invalid or allocation fails.
 */
bool ast_add_child(AstNode *parent, const char *role, AstNode *child);

/* Frees ALL nodes owned by this tree, including unattached nodes from a failed
 * parse. Call once after using/exporting the AST, on success AND on failure.
 * Never free an individual node or use any node after this call.
 */
void ast_destroy(Ast *tree);

#endif
