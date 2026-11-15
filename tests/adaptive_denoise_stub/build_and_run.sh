#!/usr/bin/env bash
# Builds and runs the host-only adaptive denoise unit tests without requiring
# ISPC or the generated CMake config.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

g++ -std=c++17 -O2 -Wall -Wextra \
  -DOIDN_ADAPTIVE_TEST_STUB \
  -I"$ROOT/tests/adaptive_denoise_stub/stub" \
  -I"$ROOT/common" \
  "$ROOT/common/adaptive_denoise.cpp" \
  "$ROOT/tests/adaptive_denoise_stub/test_adaptive_denoise.cpp" \
  -o /tmp/test_adaptive_denoise

/tmp/test_adaptive_denoise
