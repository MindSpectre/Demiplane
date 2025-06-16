#pragma once

#include <sstream>
#include <string>

#include "../entry_interface.hpp"

namespace demiplane::scroll {
    template <class Service>
    class ServiceEntry final : public detail::EntryBase<detail::MetaSource, detail::MetaThread, detail::MetaProcess> {
        using Base = EntryBase;

    public:
        using Base::Base;

        [[nodiscard]] std::string to_string() const {
            std::ostringstream os;
            os << "[" << scroll::to_string(level_) << "] "
               << "[" << Service::name() << "] "
               << "[" << loc.file_name() << ':' << loc.line() << " " << loc.function_name() << "] "
               << "[tid " << tid << ", pid " << pid << "] " << message_ << '\n';
            return os.str();
        }
    };
} // namespace demiplane::scroll
