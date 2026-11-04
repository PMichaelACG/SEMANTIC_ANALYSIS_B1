#include "semantic.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct SemanticScope {
    SemanticScope *parent;
    size_t id;
};

static bool is(const AstNode *n, const char *kind)
{ return n && strcmp(n->kind, kind) == 0; }

static AstNode *child(AstNode *n, const char *role)
{
    for (AstNode *c = n->first_child; c; c = c->next_sibling)
        if (strcmp(c->role, role) == 0) return c;
    return NULL;
}

static void report(SemanticState *s, AstNode *n, const char *category, const char *name)
{
    ++s->result.errors;
    n->inferred_type = TYPE_ERROR;
    fprintf(s->diagnostics, "SEMANTIC_%s %s\n", category, name ? name : "");
}

SemanticScope *semantic_enter_scope(SemanticState *s)
{
    if (!s->enabled) return NULL;
    if (s->next_scope == SIZE_MAX) { s->internal_error = 1; return NULL; }
    SemanticScope *scope = malloc(sizeof *scope);
    if (!scope) { s->internal_error = 1; return NULL; }
    *scope = (SemanticScope){s->scope, s->next_scope++};
    s->scope = scope;
    if (s->trace) fprintf(s->trace, "SEM_TRACE enter S%zu\n", scope->id);
    return scope;
}

bool semantic_begin(SemanticState *s, int enabled, FILE *diagnostics, FILE *trace)
{
    s->enabled = enabled;
    s->diagnostics = diagnostics;
    s->trace = trace;
    if (!enabled) return true;
    if (!diagnostics) { s->internal_error = 1; return false; }
    return semantic_enter_scope(s) != NULL; /* Global S0. */
}

void semantic_leave_scope(SemanticState *s, SemanticScope *guard)
{
    if (!s->enabled || !guard) return;
    if (s->scope != guard) { s->internal_error = 1; return; }
    if (s->trace) fprintf(s->trace, "SEM_TRACE leave S%zu\n", guard->id);
    s->scope = guard->parent;
    free(guard);
}

void semantic_close_block(SemanticState *s, SemanticScope *guard, AstNode *block)
{
    if (!s->enabled) return;
    if (!guard || !block) { s->internal_error = 1; return; }
    block->scope_id = guard->id;
    semantic_leave_scope(s, guard);
}

static const Symbol *lookup_current(const SemanticState *s,
                                    const SemanticScope *scope, const char *name)
{
    for (size_t i = s->result.count; i > 0; --i) {
        const Symbol *item = &s->result.symbols[i - 1];
        if (item->scope_id == scope->id && strcmp(item->name, name) == 0)
            return item;
    }
    return NULL;
}

static const Symbol *lookup(const SemanticState *s, const char *name)
{
    for (const SemanticScope *scope = s->scope; scope; scope = scope->parent) {
        const Symbol *found = lookup_current(s, scope, name);
        if (found) return found;
    }
    return NULL;
}

static bool insert(SemanticState *s, AstNode *decl, AstNode *name, Type t)
{
    SemanticResult *r = &s->result;
    if (r->count == r->capacity) {
        if (r->capacity > SIZE_MAX / 2 / sizeof(Symbol)) return false;
        size_t cap = r->capacity ? r->capacity * 2 : 8;
        Symbol *items = realloc(r->symbols, cap * sizeof *items);
        if (!items) return false;
        r->symbols = items;
        r->capacity = cap;
    }
    r->symbols[r->count++] = (Symbol){name->text, t, decl, s->scope->id};
    decl->scope_id = s->scope->id;
    name->binding = decl;
    name->inferred_type = decl->inferred_type = t;
    if (s->trace)
        fprintf(s->trace, "SEM_TRACE declare %s -> n%zu in S%zu\n",
                name->text, decl->id, decl->scope_id);
    return true;
}

static bool comparison(const char *op)
{
    return strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
           strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 ||
           strcmp(op, ">") == 0 || strcmp(op, ">=") == 0;
}

static Type ready(SemanticState *s, AstNode *n)
{
    if (!n || n->inferred_type == TYPE_UNKNOWN) {
        s->internal_error = 1; /* A missing or incorrectly scheduled action. */
        return TYPE_ERROR;
    }
    return n->inferred_type;
}

void semantic_expression(SemanticState *s, AstNode *n)
{
    if (!s->enabled) return;
    if (!n || !s->scope) { s->internal_error = 1; return; }
    Type t = TYPE_ERROR;
    if (is(n, "Integer")) t = TYPE_INT;
    else if (is(n, "String")) t = TYPE_STRING;
    else if (is(n, "Identifier")) {
        const Symbol *found = lookup(s, n->text);
        if (!found) report(s, n, "UNDECLARED", n->text);
        else {
            n->binding = found->declaration;
            t = found->type;
            if (s->trace)
                fprintf(s->trace, "SEM_TRACE lookup %s in S%zu -> n%zu (S%zu)\n",
                        n->text, s->scope->id, found->declaration->id, found->scope_id);
        }
    } else if (is(n, "Unary")) {
        Type operand = ready(s, child(n, "operand"));
        if (operand == TYPE_INT) t = TYPE_INT;
        else if (operand != TYPE_ERROR) report(s, n, "TYPE", n->text);
    } else if (is(n, "Binary")) {
        Type left = ready(s, child(n, "left"));
        Type right = ready(s, child(n, "right"));
        if (left == TYPE_INT && right == TYPE_INT)
            t = comparison(n->text) ? TYPE_BOOL : TYPE_INT;
        else if (left != TYPE_ERROR && right != TYPE_ERROR)
            report(s, n, "TYPE", n->text);
    } else s->internal_error = 1;
    n->inferred_type = t;
    if (s->trace)
        fprintf(s->trace, "SEM_TRACE %s n%zu -> %s (S%zu)\n",
                n->kind, n->id, semantic_type_name(t), s->scope->id);
}

void semantic_statement(SemanticState *s, AstNode *n)
{
    if (!s->enabled) return;
    if (!n || !s->scope) { s->internal_error = 1; return; }
    if (is(n, "Declaration")) {
        AstNode *name = child(n, "name");
        Type t = ready(s, child(n, "initializer"));
        if (!name) { s->internal_error = 1; return; }
        /* The initializer was checked before this declaration exists. */
        if (lookup_current(s, s->scope, name->text))
            report(s, n, "DUPLICATE", name->text);
        else if (t == TYPE_INT) {
            if (!insert(s, n, name, t)) s->internal_error = 1;
        } else if (t != TYPE_ERROR) report(s, n, "TYPE", name->text);
    } else if (is(n, "Assign")) {
        AstNode *target = child(n, "target");
        /* Target IDENTIFIER is not reduced as primary; resolve it here. */
        semantic_expression(s, target);
        Type a = ready(s, target), b = ready(s, child(n, "value"));
        if (a != TYPE_ERROR && b != TYPE_ERROR &&
            (a != TYPE_INT || b != TYPE_INT)) report(s, n, "TYPE", "assignment");
    } else if (is(n, "Print")) {
        Type t = ready(s, child(n, "value"));
        if (t != TYPE_ERROR && t != TYPE_INT && t != TYPE_STRING)
            report(s, n, "TYPE", "print");
    } else if (is(n, "If") || is(n, "While")) {
        Type t = ready(s, child(n, "condition"));
        if (t != TYPE_BOOL && t != TYPE_ERROR) report(s, n, "TYPE", "condition");
        /* Both branches / the loop body have already been checked once. */
    } else s->internal_error = 1;
    if (s->trace) fprintf(s->trace, "SEM_TRACE checked %s n%zu\n", n->kind, n->id);
}

int semantic_status(SemanticState *s, int completed)
{
    if (!s->enabled) return 0;
    if (completed && (!s->scope || s->scope->id != 0)) s->internal_error = 1;
    if (s->internal_error || (s->diagnostics && ferror(s->diagnostics)) ||
        (s->trace && ferror(s->trace))) return 2;
    return s->result.errors ? 1 : 0;
}

void semantic_destroy(SemanticState *s)
{
    if (!s) return;
    while (s->scope) semantic_leave_scope(s, s->scope);
    free(s->result.symbols);
    *s = (SemanticState){0};
}

const char *semantic_type_name(Type t)
{
    switch (t) {
    case TYPE_INT: return "Int";
    case TYPE_BOOL: return "Bool";
    case TYPE_STRING: return "String";
    case TYPE_ERROR: return "Error";
    default: return "Unknown";
    }
}
