// Test-only stub: provides the OIDN namespace macros and the minimum surface
// that common/adaptive_denoise.h relies on, without pulling in the ISPC/CMake
// generated build of the full library.
#pragma once

#define OIDN_NAMESPACE_BEGIN namespace oidn {
#define OIDN_NAMESPACE_END }
#define OIDN_NAMESPACE_USING using namespace oidn;
