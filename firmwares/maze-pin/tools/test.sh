#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
exec "$root_dir/tools/test.sh" --app maze-pin "$@"
