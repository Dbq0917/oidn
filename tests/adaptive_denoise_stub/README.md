# Adaptive denoise unit tests

Host-only unit tests for `common/adaptive_denoise.cpp`. They deliberately do
not use the OIDN CMake/ISPC build: the `stub/` directory provides minimal
`common.h` and `half.h` replacements so the adaptive-strength math and the
`float32`/`float16` blending paths can be checked with a plain C++17 compiler.

Run:

    ./build_and_run.sh

The equivalent compile command is:

    g++ -std=c++17 -O2 -Wall -Wextra -DOIDN_ADAPTIVE_TEST_STUB \
        -Istub -I../../common \
        ../../common/adaptive_denoise.cpp test_adaptive_denoise.cpp \
        -o /tmp/test_adaptive_denoise
