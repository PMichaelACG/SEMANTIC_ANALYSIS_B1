#include "ast.h"

#include <stdlib.h>
#include <string.h>

static char *copy_text(const char *text)
{
    size_t size = strlen(text) + 1;
    char *copy = malloc(size);
    if (copy != NULL)
        memcpy(copy, text, size);
    return copy;
}

AstNode *ast_new(Ast *tree, const char *kind, const char *text)
{
    if (tree == NULL || kind == NULL || kind[0] == '\0')
        return NULL;
    AstNode *node = calloc(1, sizeof *node);
    if (node == NULL)
        return NULL;
    node->kind = copy_text(kind);
    node->text = text == NULL ? NULL : copy_text(text);
    if (node->kind == NULL || (text != NULL && node->text == NULL)) {
        free(node->kind);
        free(node->text);
        free(node);
        return NULL;
    }
    node->id = tree->next_id++;
    node->owner = tree;
    node->allocation_next = tree->allocations;
    tree->allocations = node;
    return node;
}

bool ast_add_child(AstNode *parent, const char *role, AstNode *child)
{
    if (parent == NULL || child == NULL || role == NULL || role[0] == '\0' ||
        parent->owner != child->owner || child->parent != NULL)
        return false;
    /* Reject a cycle before changing any links. */
    for (AstNode *ancestor = parent; ancestor != NULL; ancestor = ancestor->parent)
        if (ancestor == child)
            return false;
    char *role_copy = copy_text(role);
    if (role_copy == NULL)
        return false;
    child->role = role_copy;
    child->parent = parent;
    if (parent->last_child != NULL)
        parent->last_child->next_sibling = child;
    else
        parent->first_child = child;
    parent->last_child = child;
    return true;
}

void ast_destroy(Ast *tree)
{
    if (tree == NULL)
        return;
    AstNode *node = tree->allocations;
    while (node != NULL) {
        AstNode *next = node->allocation_next;
        free(node->kind);
        free(node->text);
        free(node->role);
        free(node);
        node = next;
    }
    *tree = (Ast){0};
}
