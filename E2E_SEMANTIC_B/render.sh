#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
bash build.sh
input=${1:-examples/semantic_sample.qc}
./build/quectoc-ast "$input" > build/raw.dot
./build/quectoc-ast --semantic "$input" > build/typed.dot
dot -Tsvg build/raw.dot -o build/raw.svg
dot -Tsvg build/typed.dot -o build/typed.svg
printf '%s\n' 'Created raw and typed DOT/SVG in build/'
