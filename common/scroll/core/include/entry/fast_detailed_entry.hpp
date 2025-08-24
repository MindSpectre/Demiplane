#pragma once
#include <string>

#include <cstring>
#include <gears_templates.hpp>
#include "../entry_interface.hpp"

namespace demiplane::scroll {
    class FastDetailedEntry final
        : public detail::EntryBase<detail::MetaTimePoint, detail::MetaSource, detail::MetaThread,
                                   detail::MetaProcess> {
    public:
        using EntryBase::EntryBase;

        [[nodiscard]] std::string to_string() const override {
            // Reuse thread-local buffer
            auto& tl_string_buffer = get_tl_buffer();
            tl_string_buffer.clear();
            tl_string_buffer.clear();
            tl_string_buffer.reserve(256 + message_.size());

            // Fast timestamp
            append_timestamp_fast(tl_string_buffer, time_point);

            tl_string_buffer.append(" [");
            tl_string_buffer.append(level_cstr());
            tl_string_buffer.append("] [");

            // Already have just filename in MetaSourceFast
            if (source_file) {
                const char* fname = source_file;
                if (const char* last_slash = std::strrchr(fname, '/')) fname = last_slash + 1;
                tl_string_buffer.append(fname);
            }
            tl_string_buffer.push_back(':');
            append_number(tl_string_buffer, source_line);
            tl_string_buffer.push_back(' ');

            if (source_func) {
                tl_string_buffer.append(source_func);
            }

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

        static bool comp(const FastDetailedEntry& lhs, const FastDetailedEntry& rhs) {
            // Optimized comparison using single 64-bit compare when possible
            if (lhs.time_point != rhs.time_point) {
                return lhs.time_point < rhs.time_point;
            }
            return lhs.level() < rhs.level();
        }

    private:
        static void append_number(std::string& buf, uint32_t value) {
            char temp[16];
            snprintf(temp, sizeof(temp), "%u", value);
            buf.append(temp);
        }

        static void append_timestamp_fast(std::string& buf,
                                          const std::chrono::system_clock::time_point& tp) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                tp.time_since_epoch()) % 1000;

            const auto tt = std::chrono::system_clock::to_time_t(tp);
            tm tm_buf{};
            gmtime_r(&tt, &tm_buf);

            char timestamp[32];
            const std::int32_t len = snprintf(timestamp, sizeof(timestamp),
                                              "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
                                              tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                                              tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                                              ms.count());

            buf.append(timestamp, static_cast<std::size_t>(len));
        }
    };

    // Traits for the new fast entry
    template <>
    struct detail::entry_traits<FastDetailedEntry> {
        using wants = gears::type_list<MetaTimePoint, MetaSource, MetaThread, MetaProcess>;
    };
}
