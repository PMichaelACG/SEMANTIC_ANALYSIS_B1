#include "parse_context.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
ParseContext parse_context={0};
extern FILE *yyin;
int yyparse(void);
int yylex_destroy(void);
static AstNode *child(AstNode *n,const char *role) {
    for(AstNode *c=n->first_child;c;c=c->next_sibling)
        if(strcmp(c->role,role)==0)return c;
    assert(0); return NULL;
}
int main(int argc,char **argv) {
    (void)argv;
    FILE *source=tmpfile(), *log=tmpfile(); assert(source && log);
    fputs(argc==1 ? "let x=1; {let x=x+1; print(x);} print(x);" :
                   "let x=1; {{ let y=2; print( ;",source);
    rewind(source); yyin=source;
    assert(semantic_begin(&parse_context.semantic,1,log,NULL));
    int parsed=yyparse();
    if(argc==1) {
        assert(parsed==0);
        /* Inspect immediately after yyparse: no analysis call intervenes. */
        AstNode *root=parse_context.root, *outer=root->first_child;
        AstNode *block=outer->next_sibling, *inner=block->first_child;
        AstNode *initializer=child(inner,"initializer");
        assert(child(initializer,"left")->binding==outer);
        assert(initializer->inferred_type==TYPE_INT);
        assert(child(inner->next_sibling,"value")->binding==inner);
        assert(child(block->next_sibling,"value")->binding==outer);
        assert(parse_context.semantic.result.count==2 && block->scope_id==1);
    } else {
        assert(parsed==1);
        /* %destructor must restore S0 when two open blocks are discarded. */
    }
    assert(semantic_status(&parse_context.semantic,1)==0);
    yylex_destroy(); semantic_destroy(&parse_context.semantic);
    ast_destroy(&parse_context.tree); fclose(source); fclose(log);
    puts(argc==1 ? "PASS parser annotations exist at return" : "PASS scope guards on parse abort");
}
