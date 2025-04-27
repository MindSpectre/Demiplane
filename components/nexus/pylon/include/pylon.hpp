#pragma once

namespace demiplane::nexus::detail {
    inline std::mutex unique_guard_mtx;
    inline std::unordered_map<void*, int> unique_counts;
} // namespace detail

#include "../basic_pylon.hpp"
#include "../safe_pylon.hpp"
#include "../unique_pylon.hpp"
#include "../view_pylon.hpp"
