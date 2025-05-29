#pragma once

namespace demiplane::nexus {
    class Nexus;
    /// @brief Глобальный процесс‑wide инстанс Nexus
    Nexus& process_nexus() noexcept;
}