#include "parse_context.h"
#include "ast_dot.h"
#include <stdio.h>
#include <string.h>

ParseContext parse_context = {0};
extern FILE *yyin;
int yyparse(void);
int yylex_destroy(void);

int main(int argc, char **argv)
{
    int semantic_mode = 0, trace_mode = 0, file_arg = 1;
    if (argc > 1 && strcmp(argv[1], "--semantic") == 0) {
        semantic_mode = 1; file_arg = 2;
    } else if (argc > 1 && strcmp(argv[1], "--trace-semantic") == 0) {
        semantic_mode = trace_mode = 1; file_arg = 2;
    }
    if (argc > file_arg + 1 ||
        (argc > file_arg && strncmp(argv[file_arg], "--", 2) == 0)) {
        fprintf(stderr, "usage: %s [--semantic|--trace-semantic] [source.qc|-]\n", argv[0]);
        return 2;
    }
    yyin = stdin;
    if (argc == file_arg + 1 && strcmp(argv[file_arg], "-") != 0) {
        yyin = fopen(argv[file_arg], "rb");
        if (!yyin) { perror(argv[file_arg]); return 2; }
    }

    /* Set up S0 BEFORE any reduction action runs. */
    int initialized = semantic_begin(&parse_context.semantic, semantic_mode,
                                     stderr, trace_mode ? stderr : NULL);
    int parsed = initialized ? yyparse() : 2;
    int status = parsed == 2 || parse_context.out_of_memory ? 2 :
                 parsed != 0 || parse_context.had_error ? 1 : 0;
    int checked = semantic_status(&parse_context.semantic, parsed == 0);
    if (checked == 2) status = 2;
    else if (status == 0) status = checked;
    if (ferror(yyin)) { fprintf(stderr, "input read failed\n"); status = 2; }
    if (yyin != stdin && fclose(yyin) != 0) { perror("closing input"); status = 2; }

    /* No semantic AST traversal here: reductions have already annotated it. */
    if (status == 0 &&
        ((semantic_mode ? semantic_write_dot(stdout, parse_context.root) :
                          ast_write_dot(stdout, parse_context.root)) != 0 ||
         fflush(stdout) != 0)) {
        fprintf(stderr, "DOT output failed\n"); status = 2;
    }
    if (parsed == 2 || parse_context.out_of_memory || checked == 2)
        fprintf(stderr, "compiler resource or internal error\n");
    yylex_destroy();
    semantic_destroy(&parse_context.semantic);
    ast_destroy(&parse_context.tree);
    return status;
}
