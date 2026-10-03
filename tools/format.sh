#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
mode=${1:-write}
case "$mode" in
    write) flags=(-i) ;;
    --check) flags=(--dry-run --Werror) ;;
    *) echo "Usage: $0 [--check]" >&2; exit 2 ;;
esac
cd "$root_dir"
mapfile -d '' -t sources < <(python3 - <<'PY'
from pathlib import Path
import sys
for directory in ['common', 'main', 'firmwares', 'tools']:
    for path in sorted(Path(directory).rglob('*')):
        if path.suffix not in ('.c', '.h') or path.name.startswith('assets_generated.'):
            continue
        if any(part in ('build', 'managed_components', 'artifacts', '.source-git-metadata')
               or part.startswith('build-') for part in path.parts):
            continue
        sys.stdout.write(str(path) + '\0')
PY
)
clang-format "${flags[@]}" "${sources[@]}"
