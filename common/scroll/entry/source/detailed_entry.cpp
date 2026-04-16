#include "detailed_entry.hpp"

#include <cstring>
#include <demiplane/chrono>

namespace {
    void append_number(std::string& buf, const uint32_t value) {
        char temp[16];
        snprintf(temp, sizeof(temp), "%u", value);
        buf.append(temp);
    }
}  // namespace

namespace demiplane::scroll {
    void DetailedEntry::format_into(std::string& out) const {
        out.clear();
        out.reserve(128 + message_.size());

        chrono::UTCClock::format_time_iso_ms(time_point, out);

        out.push_back(' ');
        out.append(level_cstr());
        out.append(" [tid ");
        out.append(tid_str);
#ifndef __linux__
        out.append(", pid ");
        out.append(pid_str);
#endif
        out.append("] [");

        const char* fname = location.file_name();
        if (const char* last_slash = std::strrchr(fname, '/')) {
            fname = last_slash + 1;
        }
        out.append(fname);

        out.push_back(':');
        append_number(out, location.line());

        out.append("] ");
        out.append(message_);
        out.push_back('\n');
    }
}  // namespace demiplane::scroll
