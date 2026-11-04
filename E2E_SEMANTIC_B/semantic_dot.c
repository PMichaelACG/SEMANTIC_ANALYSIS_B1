#include "semantic.h"
#include <string.h>

/* DOT quoted labels have their own escapes. Escape all user text so quotes,
 * backslashes, and Graphviz substitutions such as \N remain literal text.
 * Render control characters visibly (e.g. a newline becomes the text \n).
 * IDs use allocation numbers, never label contents or memory addresses.
 */
static void escaped(FILE *out, const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        switch (*p) {
        case '"': fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\\\n", out); break;
        case '\r': fputs("\\\\r", out); break;
        case '\t': fputs("\\\\t", out); break;
        default:
            if (*p < 32 || *p == 127)
                fprintf(out, "\\\\x%02X", (unsigned)*p);
            else
                fputc(*p, out);
        }
    }
}

int semantic_write_dot(FILE *out, const AstNode *root)
{
    if (out == NULL || root == NULL)
        return -1;
    fputs("digraph AST {\n  graph [ordering=out];\n"
          "  node [shape=box, fontname=\"sans-serif\"];\n", out);

    /* Walk through parent/child links without recursion or an extra stack.
     * This exports only root's subtree, even when root itself has a parent.
     */
    const AstNode *node = root;
    for (;;) {
        fprintf(out, "  n%zu [label=\"", node->id);
        escaped(out, node->kind);
        if (node->text != NULL) {
            fputs(": ", out);
            escaped(out, node->text);
        }
        if (node->inferred_type != TYPE_UNKNOWN) {
            fputs("\\nType: ", out);
            escaped(out, semantic_type_name(node->inferred_type));
        }
        if (strcmp(node->kind, "Block") == 0 || strcmp(node->kind, "Declaration") == 0)
            fprintf(out, "\\nScope: S%zu", node->scope_id);
        if (node->binding)
            fprintf(out, "\\nBinds to n%zu", node->binding->id);
        if (node->inferred_type == TYPE_ERROR)
            fputs("\", color=\"#C52D38\", penwidth=2];\n", out);
        else
            fputs("\"];\n", out);
        if (node->binding)
            fprintf(out, "  n%zu -> n%zu [style=dashed, color=\"#C52D38\", constraint=false, label=\"binds\"];\n", node->id, node->binding->id);
        for (const AstNode *child = node->first_child; child != NULL;
             child = child->next_sibling) {
            fprintf(out, "  n%zu -> n%zu [label=\"", node->id, child->id);
            escaped(out, child->role);
            fputs("\"];\n", out);
        }
        if (node->first_child != NULL) {
            node = node->first_child;
            continue;
        }
        while (node != root && node->next_sibling == NULL)
            node = node->parent;
        if (node == root)
            break;
        node = node->next_sibling;
    }
    fputs("}\n", out);
    return ferror(out) ? -1 : 0;
}
