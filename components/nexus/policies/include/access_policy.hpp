#pragma once

namespace demiplane::nexus {

    /// @brief Способ выдачи объекта из Nexus
    enum class Access {
        Default, ///< shared_ptr alias (дефолт)
        View, ///< const view (без счётчика)
        Unique, ///< эксклюзивная ручка (единственный pylon)
        Copy, ///< глубокая/поверхностная копия
        Safe ///< weak_ptr с run‑time проверкой
    };

} // namespace demiplane::nexus
