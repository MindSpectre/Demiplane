#pragma once

#include "gears_macros.hpp"
// There are main macros

#ifndef DMP_LOG_LEVEL
#define DMP_LOG_LEVEL DBG // 0=OFF 1=ERR 2=WRN 3=INF 4=DBG 5=TRC
#endif

#define SCROLL_LOG()            CAT(SCROLL_LOG_, DMP_LOG_LEVEL)()
#define SCROLL_LOG_MESSAGE(msg) CAT(SCROLL_LOG_MESSAGE_, DMP_LOG_LEVEL)(msg)
#define SCROLL_LOG_DBG()        SCROLL_LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::DBG)
#define SCROLL_LOG_INF()        SCROLL_LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::INF)
#define SCROLL_LOG_WRN()        SCROLL_LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::WRN)
#define SCROLL_LOG_ERR()        SCROLL_LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::ERR)
#define SCROLL_LOG_FAT()        SCROLL_LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::FAT)


#define SCROLL_LOG_MESSAGE_DBG(message) SCROLL_LOG_ENTRY(this->get_logger(), demiplane::scroll::DBG, message)
#define SCROLL_LOG_MESSAGE_INF(message) SCROLL_LOG_ENTRY(this->get_logger(), demiplane::scroll::INF, message)
#define SCROLL_LOG_MESSAGE_WRN(message) SCROLL_LOG_ENTRY(this->get_logger(), demiplane::scroll::WRN, message)
#define SCROLL_LOG_MESSAGE_ERR(message) SCROLL_LOG_ENTRY(this->get_logger(), demiplane::scroll::ERR, message)
#define SCROLL_LOG_MESSAGE_FAT(message) SCROLL_LOG_ENTRY(this->get_logger(), demiplane::scroll::FAT, message)


// Addons

#define SCROLL_PARAMS(...) \
    ([&]() { \
    std::ostringstream oss; \
    SCROLL_PARAMS_PROCESS(oss, __VA_ARGS__); \
    return oss.str(); \
})()

#define SCROLL_PARAMS_PROCESS(stream, ...) \
    SCROLL_PARAMS_HELPER(stream, __VA_ARGS__)

#define SCROLL_PARAMS_HELPER(stream, param, ...) \
    do { \
    stream << #param "={" << param << "}"; \
    SCROLL_PARAMS_CONTINUE(stream, __VA_ARGS__); \
    } while(0)

#define SCROLL_PARAMS_CONTINUE(stream, ...) \
    SCROLL_PARAMS_IF_NOT_EMPTY(__VA_ARGS__)(SCROLL_PARAMS_NEXT(stream, __VA_ARGS__))

#define SCROLL_PARAMS_IF_NOT_EMPTY(...) \
    SCROLL_PARAMS_IF_NOT_EMPTY_IMPL(SCROLL_PARAMS_HAS_COMMA(__VA_ARGS__,))

#define SCROLL_PARAMS_IF_NOT_EMPTY_IMPL(has_comma) \
    CAT(SCROLL_PARAMS_IF_NOT_EMPTY_, has_comma)

#define SCROLL_PARAMS_IF_NOT_EMPTY_0(...)
#define SCROLL_PARAMS_IF_NOT_EMPTY_1(...) __VA_ARGS__

#define SCROLL_PARAMS_NEXT(stream, param, ...) \
    stream << " "; \
    SCROLL_PARAMS_HELPER(stream, __VA_ARGS__)

#define SCROLL_PARAMS_HAS_COMMA(...) \
    SCROLL_PARAMS_HAS_COMMA_IMPL(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)

#define SCROLL_PARAMS_HAS_COMMA_IMPL(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10, ...) a10
