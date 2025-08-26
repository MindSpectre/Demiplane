#include "detailed_entry.hpp"

#include <cstring>
#include <demiplane/chrono>

namespace {
    void append_number(std::string& buf, uint32_t value) {
        char temp[16];
        snprintf(temp, sizeof(temp), "%u", value);
        buf.append(temp);
    }
}

namespace demiplane::scroll {
    std::string DetailedEntry::to_string() const {
        // Reuse thread-local buffer
        std::string& tl_string_buffer = get_tl_buffer();
        tl_string_buffer.clear();
        tl_string_buffer.clear();
        tl_string_buffer.reserve(128 + message_.size());

        // Fast timestamp
        chrono::UTCClock::format_time_iso_ms(time_point, tl_string_buffer);

        tl_string_buffer.append(" [");
        tl_string_buffer.append(level_cstr());
        tl_string_buffer.append("] [");

        // Already have just filename in MetaSourceF
        const char* fname = location.file_name();
        if (const char* last_slash = std::strrchr(fname, '/')) {
            fname = last_slash + 1;
        }
        tl_string_buffer.append(fname);

        tl_string_buffer.push_back(':');
        append_number(tl_string_buffer, location.line());

        // Use pre-formatted strings
        tl_string_buffer.append("] [tid ");
        tl_string_buffer.append(tid_str); // Already formatted!
        tl_string_buffer.append(", pid ");
        tl_string_buffer.append(pid_str); // Already formatted!
        tl_string_buffer.append("] ");
        tl_string_buffer.append(message_);
        tl_string_buffer.push_back('\n');

        return std::move(tl_string_buffer);
    }
}
