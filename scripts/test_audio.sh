#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_binary="$(mktemp /tmp/scrib-test-audio.XXXXXX)"
trap 'rm -f "$test_binary"' EXIT
"${HOST_CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -Isource \
    scripts/test_audio.c source/audio_core.c -lvorbisfile -lvorbis -logg -o "$test_binary"
"$test_binary" "${1:-../res/raw}"
