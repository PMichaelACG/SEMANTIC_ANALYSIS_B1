#include "ast.h"
#include "ast_dot.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dot_text(const AstNode *root)
{
    FILE *out = tmpfile();
    assert(out != NULL && ast_write_dot(out, root) == 0);
    assert(fflush(out) == 0);
    long length = ftell(out);
    assert(length >= 0);
    rewind(out);
    char *text = calloc((size_t)length + 1, 1);
    assert(text != NULL);
    assert(fread(text, 1, (size_t)length, out) == (size_t)length);
    assert(fclose(out) == 0);
    return text;
}

int main(void)
{
    Ast tree = {0}, other = {0};
    char kind[] = "Identifier", value[] = "x", role[] = "left";
    AstNode *root = ast_new(&tree, "Program", NULL);
    AstNode *a = ast_new(&tree, kind, value);
    AstNode *b = ast_new(&tree, "Identifier", "x");
    AstNode *foreign = ast_new(&other, "Foreign", NULL);
    assert(root && a && b && foreign);
    kind[0] = 'Z'; value[0] = 'y';
    assert(strcmp(a->kind, "Identifier") == 0 && strcmp(a->text, "x") == 0);
    assert(a->id != b->id);
    assert(ast_add_child(root, role, a));
    role[0] = 'z';
    assert(strcmp(a->role, "left") == 0);
    assert(ast_add_child(root, "right", b));
    assert(root->first_child == a && a->next_sibling == b && root->last_child == b);
    assert(!ast_add_child(root, "again", a));      /* two parents/duplicate edge */
    assert(!ast_add_child(a, "cycle", root));      /* ancestor cycle */
    assert(!ast_add_child(foreign, "self", foreign));
    assert(!ast_add_child(root, "foreign", foreign));
    assert(!ast_add_child(NULL, "null", b));
    assert(!ast_add_child(root, "null", NULL));
    assert(!ast_add_child(root, "", foreign));
    assert(ast_new(NULL, "Bad", NULL) == NULL);
    assert(ast_new(&tree, NULL, NULL) == NULL);
    assert(ast_write_dot(NULL, root) == -1);

    char *subtree = dot_text(a);
    assert(strstr(subtree, "Identifier: x") != NULL);
    assert(strstr(subtree, "Program") == NULL);
    assert(strstr(subtree, " -> ") == NULL);
    free(subtree);
    /* An unattached node is intentionally left here to check arena cleanup. */
    assert(ast_new(&tree, "Unattached", "discarded after parse failure"));
    ast_destroy(&tree);
    ast_destroy(&other);
    assert(tree.allocations == NULL && tree.next_id == 0);
    ast_destroy(&tree);                          /* safe to clean up twice */
    ast_destroy(NULL);

    /* Traversal and cleanup must not exhaust the C stack on a deep tree. */
    root = ast_new(&tree, "Root", NULL);
    AstNode *tail = root;
    for (int i = 0; i < 5000; ++i) {
        AstNode *child = ast_new(&tree, "Unary", "-");
        assert(ast_add_child(tail, "operand", child));
        tail = child;
    }
    char *deep = dot_text(root);
    assert(strstr(deep, "n5000 [label=\"Unary: -\"]") != NULL);
    free(deep);
    ast_destroy(&tree);

    /* The runner renders this graph and checks its actual SVG text. */
    root = ast_new(&tree, "Program", NULL);
    a = ast_new(&tree, "String", "quote \"; slash \\; newline \n; tab \t; CR \r; control \001");
    b = ast_new(&tree, "String", "\"]; evil [label=\"injected\"]; // \\N");
    assert(ast_add_child(root, "first", a));
    assert(ast_add_child(root, "second", b));
    char *escaped = dot_text(root);
    assert(strstr(escaped, "quote \\\"") != NULL);
    assert(strstr(escaped, "slash \\\\") != NULL);
    assert(strstr(escaped, "newline \\\\n") != NULL);
    assert(strstr(escaped, "\\\\N") != NULL);
    free(escaped);
    assert(ast_write_dot(stdout, root) == 0 && fflush(stdout) == 0);
    ast_destroy(&tree);
    fprintf(stderr, "PASS AST ownership, order, cleanup, deep traversal, and escaping\n");
    return 0;
}
