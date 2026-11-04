#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
export LC_ALL=C
mkdir -p build
flags=(-std=c11 -Wall -Wextra -Werror -pedantic -g -I. -Ibuild)
if [[ ${SANITIZE:-0} == 1 ]]; then
    flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
fi
bison -Wall -Werror -d --report=state -o build/parser.tab.c parser.y
flex -o build/lex.yy.c scanner.l
"${CC:-gcc}" "${flags[@]}" -D_POSIX_C_SOURCE=200809L \
    ast.c ast_dot.c semantic.c semantic_dot.c main.c build/parser.tab.c build/lex.yy.c -o build/quectoc-ast
"${CC:-gcc}" "${flags[@]}" ast.c ast_dot.c tests/test_ast.c -o build/test-ast

"${CC:-gcc}" "${flags[@]}" ast.c semantic.c tests/test_semantic.c -o build/test-semantic

"${CC:-gcc}" "${flags[@]}" -D_POSIX_C_SOURCE=200809L \
    ast.c semantic.c tests/test_parser_actions.c build/parser.tab.c build/lex.yy.c -o build/test-parser-actions
