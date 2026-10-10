#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
flags=(-std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
g++ "${flags[@]}" -I"$root" "$here/logic.cpp" -o "$build/logic"
"$build/logic"
g++ "${flags[@]}" -I"$root" "$here/live_nodes.cpp" -o "$build/live_nodes"
"$build/live_nodes"
g++ "${flags[@]}" -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -I"$here/stubs" -I"$root" "$here/app_test.cpp" -o "$build/app_test"
ASAN_OPTIONS=detect_leaks=0 CRT_TEST_OUTPUT="$build/" "$build/app_test"
g++ -x c++ -std=c++11 -Wall -Wextra -Werror -I"$here/stubs" -I"$root" -fsyntax-only "$root/CRT_House_Console_v0_3_3.ino"
echo "Host checks complete. These are NOT an ESP32 target build."
