#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_binary="$(mktemp /tmp/scrib-test-sticks.XXXXXX)"
trap 'rm -f "$test_binary"' EXIT
"${HOST_CC:-cc}" -std=c11 -Wall -Wextra -Werror -Isource \
    scripts/test_stick_touch.c source/stick_touch.c -lm -o "$test_binary"
"$test_binary"
