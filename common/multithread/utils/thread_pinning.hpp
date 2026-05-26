#pragma once

#include <pthread.h>
#include <sched.h>
#include <thread>

namespace demiplane::multithread {
    inline bool pin_current_thread_to_core(const int core) noexcept {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(core, &set);
        return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
    }
}  // namespace demiplane::multithread
