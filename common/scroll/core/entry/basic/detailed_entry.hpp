#pragma once

#include <sstream>
#include <string>

#include "../entry_interface.hpp"

namespace demiplane::scroll {
    class DetailedEntry final : public detail::EntryBase<detail::MetaSource, detail::MetaThread, detail::MetaProcess> {
        using Base = EntryBase;

    public:
        DetailedEntry(LogLevel lvl, const std::string_view& msg, const MetaSource& meta_source,
            const MetaThread& meta_thread, const MetaProcess& meta_process)
            : EntryBase(lvl, msg, meta_source, meta_thread, meta_process) {}

        [[nodiscard]] std::string to_string() const {
            std::ostringstream os;
            os << "[" << scroll::to_string(level_) << "] "
               << "[" << loc.file_name() << ':' << loc.line() << " " << loc.function_name() << "] "
               << "[tid " << tid << ", pid " << pid << "] " << message_ << '\n';
            return os.str();
        }
    };
} // namespace demiplane::scroll
