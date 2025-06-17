#pragma once

#include "gears_macros.hpp"
// There are main macros

#ifndef DMP_LOG_LEVEL
#  define DMP_LOG_LEVEL DBG                // 0=OFF 1=ERR 2=WRN 3=INF 4=DBG 5=TRC
#endif

#define SCROLL_LOG()                 CAT(SCROLL_LOG_,             DMP_LOG_LEVEL)()
#define SCROLL_LOG_MESSAGE(msg)      CAT(SCROLL_LOG_MESSAGE_,     DMP_LOG_LEVEL)(msg)
#define SCROLL_LOG_DBG() SCROLL_LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::DBG)
#define SCROLL_LOG_INF() SCROLL_LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::INF)
#define SCROLL_LOG_WRN() SCROLL_LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::WRN)
#define SCROLL_LOG_ERR() SCROLL_LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::ERR)
#define SCROLL_LOG_FAT() SCROLL_LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::FAT)



#define SCROLL_LOG_MESSAGE_DBG(message) SCROLL_LOG_ENTRY(this->get_logger(), demiplane::scroll::DBG, message)
#define SCROLL_LOG_MESSAGE_INF(message) SCROLL_LOG_ENTRY(this->get_logger(), demiplane::scroll::INF, message)
#define SCROLL_LOG_MESSAGE_WRN(message) SCROLL_LOG_ENTRY(this->get_logger(), demiplane::scroll::WRN, message)
#define SCROLL_LOG_MESSAGE_ERR(message) SCROLL_LOG_ENTRY(this->get_logger(), demiplane::scroll::ERR, message)
#define SCROLL_LOG_MESSAGE_FAT(message) SCROLL_LOG_ENTRY(this->get_logger(), demiplane::scroll::FAT, message)





// Addons
