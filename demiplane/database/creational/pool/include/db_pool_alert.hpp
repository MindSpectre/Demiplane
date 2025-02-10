#pragma once

namespace common::database::pool {
    enum class AlertType { SHARED_EXHAUSTION, SHARED_OVERFLOW, INVALID_CONNECTION };
}