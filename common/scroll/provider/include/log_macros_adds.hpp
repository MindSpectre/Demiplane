#pragma once

#include "gears_macros.hpp"
// There are main macros



#define SCROLL_ENTER_FUNCTION() SLOG_INF() << "Entering function " << __func__

// Addons
#define COUNT_ARGS(...) COUNT_ARGS_IMPL(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define COUNT_ARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

// Individual parameter formatters
#define FMT_1(p1) #p1 "=" << p1
#define FMT_2(p1, p2) FMT_1(p1) << ", " << #p2 "=" << p2
#define FMT_3(p1, p2, p3) FMT_2(p1, p2) << ", " << #p3 "=" << p3
#define FMT_4(p1, p2, p3, p4) FMT_3(p1, p2, p3) << ", " << #p4 "=" << p4
#define FMT_5(p1, p2, p3, p4, p5) FMT_4(p1, p2, p3, p4) << ", " << #p5 "=" << p5
#define FMT_6(p1, p2, p3, p4, p5, p6) FMT_5(p1, p2, p3, p4, p5) << ", " << #p6 "=" << p6
#define FMT_7(p1, p2, p3, p4, p5, p6, p7) FMT_6(p1, p2, p3, p4, p5, p6) << ", " << #p7 "=" << p7
#define FMT_8(p1, p2, p3, p4, p5, p6, p7, p8) FMT_7(p1, p2, p3, p4, p5, p6, p7) << ", " << #p8 "=" << p8

// Dispatcher macro
#define DISPATCH_FMT(N) FMT_##N
#define CALL_FMT(N, ...) DISPATCH_FMT(N)(__VA_ARGS__)

// Main macro
#define SCROLL_PARAMS(...)                                                                                             \
    ([&]() {                                                                                                           \
        std::ostringstream oss;                                                                                        \
        oss << " " << CALL_FMT(COUNT_ARGS(__VA_ARGS__), __VA_ARGS__);                                                  \
        return oss.str();                                                                                              \
    })()
