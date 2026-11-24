#ifndef GRAUPEL_DSP_MSVC_COMPAT_H_
#define GRAUPEL_DSP_MSVC_COMPAT_H_

// Upstream stmlib uses GCC/Clang's __attribute__((always_inline)) in
// stmlib/utils/dsp.h with no platform guard. eurorack/ is read-only (ADR 0001),
// so this shim is force-included ahead of the upstream headers on MSVC only.
#if defined(_MSC_VER) && !defined(__clang__)
// Upstream stmlib/dsp/filter.h uses M_PI through <cmath>; MSVC hides it unless
// this is defined before the first math header is included.
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#define __attribute__(x)
#endif

#endif  // GRAUPEL_DSP_MSVC_COMPAT_H_
