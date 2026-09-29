#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/host artifacts
if [[ -z "${TEST_LDFLAGS:-}" && -f build/host/sanitizers/usr/lib64/libasan.so ]]; then
    TEST_LDFLAGS="-L$PWD/build/host/sanitizers/usr/lib64 -Wl,-rpath,$PWD/build/host/sanitizers/usr/lib64"
fi
${CC:-gcc} -std=c11 -g -O1 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    ${TEST_LDFLAGS:-} \
    -I main tests/test_pet.c main/pet.c main/paint.c main/input.c -lm -o build/host/test_pet
ASAN_OPTIONS=detect_leaks=1 build/host/test_pet | tee artifacts/host-tests.log
