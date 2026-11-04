#ifndef QUECTOC_AST_DOT_H
#define QUECTOC_AST_DOT_H

#include "ast.h"
#include <stdio.h>

/* User-written AST exporter; this is NOT a Bison built-in.
 * Writes Graphviz DOT for the subtree rooted at root, without closing/flushing
 * out. Returns 0 on success, -1 for invalid arguments or a write error.
 * The caller must also check fflush/fclose for delayed output errors.
 * Text uses NUL-terminated strings (UTF-8 may be used); no embedded NUL bytes.
 */
int ast_write_dot(FILE *out, const AstNode *root);

#endif
