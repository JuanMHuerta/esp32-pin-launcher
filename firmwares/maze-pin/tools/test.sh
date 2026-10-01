#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
cd "$here/../.."
cc -std=c11 -Wall -Wextra -Werror -O1 \
   -I"$here/main" "$here/tests/test_maze.c" "$here/main/maze.c" "$here/main/paint.c" \
   -lm -o "$here/tests/test_maze"
"$here/tests/test_maze" "$here/preview.ppm"
