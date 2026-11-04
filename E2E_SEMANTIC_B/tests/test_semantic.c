#include "semantic.h"
#include <assert.h>
#include <stdio.h>
static AstNode *node(Ast *t, const char *k, const char *v) {
    AstNode *n=ast_new(t,k,v); assert(n); return n;
}
static void edge(AstNode *p, const char *r, AstNode *c) { assert(ast_add_child(p,r,c)); }
static AstNode *decl(Ast *t, SemanticState *s, const char *name, AstNode *init) {
    AstNode *d=node(t,"Declaration","let");
    edge(d,"name",node(t,"Identifier",name)); edge(d,"initializer",init);
    semantic_statement(s,d); return d;
}
int main(void) {
    /* Direct action API covers type combinations excluded by the source grammar. */
    Ast t={0}; SemanticState s={0}; FILE *log=tmpfile(); assert(log);
    assert(semantic_begin(&s,1,log,NULL));
    AstNode *plus=node(&t,"Binary","+");
    AstNode *a=node(&t,"String","hi"), *b=node(&t,"Integer","1");
    semantic_expression(&s,a); semantic_expression(&s,b);
    edge(plus,"left",a); edge(plus,"right",b); semantic_expression(&s,plus);
    assert(semantic_status(&s,1)==1 && s.result.errors==1);
    assert(plus->inferred_type==TYPE_ERROR);
    semantic_destroy(&s); ast_destroy(&t); fclose(log);

    /* Initializer-before-insertion; binding identity survives scope closure. */
    t=(Ast){0}; s=(SemanticState){0}; log=tmpfile(); assert(log);
    assert(semantic_begin(&s,1,log,NULL));
    a=node(&t,"Integer","1"); semantic_expression(&s,a);
    AstNode *outer=decl(&t,&s,"x",a);
    SemanticScope *guard=semantic_enter_scope(&s); assert(guard);
    AstNode *init=node(&t,"Identifier","x"); semantic_expression(&s,init);
    AstNode *inner=decl(&t,&s,"x",init);
    AstNode *use=node(&t,"Identifier","x"); semantic_expression(&s,use);
    semantic_leave_scope(&s,guard);
    AstNode *after=node(&t,"Identifier","x"); semantic_expression(&s,after);
    assert(init->binding==outer && use->binding==inner && after->binding==outer);
    assert(outer->scope_id==0 && inner->scope_id==1 && s.result.count==2);
    assert(semantic_status(&s,1)==0);
    semantic_destroy(&s); ast_destroy(&t); fclose(log);

    /* A parent action must not silently recurse to fill unprocessed children. */
    t=(Ast){0}; s=(SemanticState){0}; log=tmpfile(); assert(log);
    assert(semantic_begin(&s,1,log,NULL));
    plus=node(&t,"Binary","+");
    a=node(&t,"Integer","2"); b=node(&t,"Integer","3");
    edge(plus,"left",a); edge(plus,"right",b); semantic_expression(&s,plus);
    assert(a->inferred_type==TYPE_UNKNOWN && b->inferred_type==TYPE_UNKNOWN);
    assert(semantic_status(&s,1)==2);
    semantic_destroy(&s); ast_destroy(&t); fclose(log);
    puts("PASS local action API: types, bindings, scheduling");
}
