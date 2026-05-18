#pragma once

#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
#endif

namespace demiplane::multithread {
    inline void pause_arc_agnostic() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("yield" ::: "memory");
#else
    #error unsupported architecture
#endif
    }
}  // namespace demiplane::multithread
