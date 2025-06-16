#pragma once

#define LOG2S_DBG() LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::DBG)
#define LOG2S_INF() LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::INF)
#define LOG2S_WRN() LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::WRN)
#define LOG2S_ERR() LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::ERR)
#define LOG2S_FAT() LOG_STREAM_ENTRY(this->get_logger(), demiplane::scroll::FAT)
#define LOG2_DBG(message) LOG_ENTRY(this->get_logger(), demiplane::scroll::DBG, message)
#define LOG2_INF(message) LOG_ENTRY(this->get_logger(), demiplane::scroll::INF, message)
#define LOG2_WRN(message) LOG_ENTRY(this->get_logger(), demiplane::scroll::WRN, message)
#define LOG2_ERR(message) LOG_ENTRY(this->get_logger(), demiplane::scroll::ERR, message)
#define LOG2_FAT(message) LOG_ENTRY(this->get_logger(), demiplane::scroll::FAT, message)
