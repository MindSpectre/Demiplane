#pragma once
#include <cstdint>
#include <demiplane/chrono>
#include <filesystem>

#include <gears_class_traits.hpp>
#include <gears_types.hpp>
#include <json/value.h>
#include <sink_interface.hpp>

namespace demiplane::scroll {
    /**
     * @brief Configuration for file output with rotation
     */
    class FileSinkConfig final : public gears::ConfigInterface<FileSinkConfig, Json::Value> {
    public:
        constexpr FileSinkConfig(const LogLevel threshold,
                                 std::filesystem::path file,
                                 const bool add_time_to_filename,
                                 std::string time_format_in_file_name,
                                 const bool rotate_file,
                                 const std::uint64_t max_file_size,
                                 const bool flush_each_entry) noexcept
            : threshold_{threshold},
              file_{std::move(file)},
              add_time_to_filename_{add_time_to_filename},
              time_format_in_file_name_{std::move(time_format_in_file_name)},
              rotate_file_{rotate_file},
              max_file_size_{max_file_size},
              flush_each_entry_{flush_each_entry} {
        }
        constexpr FileSinkConfig() = default;

        constexpr void validate() override {
            if (rotate_file_ && max_file_size_ == 0) {
                throw std::invalid_argument("max_file_size must be greater than 0");
            }
            if (file_.empty()) {
                throw std::invalid_argument("file path must be specified");
            }
            if (add_time_to_filename_ && time_format_in_file_name_.empty()) {
                throw std::invalid_argument("time format must be specified");
            }
            if (rotate_file_ && !add_time_to_filename_) {
                throw std::invalid_argument("rotation is enabled, but the dynamic filename is disabled");
            }
        }
        [[nodiscard]] Json::Value serialize() const override {
            Json::Value result;
            // TODO: Add serialization
            return result;
        }
        static FileSinkConfig deserialize(const Json::Value& config) {
            gears::unused_value(config);
            // TODO: Add deserialization
            std::unreachable();
        }

        template <typename Self>
        constexpr auto&& threshold(this Self&& self, const LogLevel threshold) noexcept {
            self.threshold_ = threshold;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& file(this Self&& self, std::filesystem::path file) noexcept {
            self.file_ = std::move(file);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& add_time_to_filename(this Self&& self, const bool add_time_to_filename) noexcept {
            self.add_time_to_filename_ = add_time_to_filename;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& time_format_in_file_name(this Self&& self, std::string time_format) noexcept {
            self.time_format_in_file_name_ = std::move(time_format);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& max_file_size(this Self&& self, const std::uint64_t max_file_size) noexcept {
            self.max_file_size_ = max_file_size;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& flush_each_entry(this Self&& self, const bool flush_each_entry) noexcept {
            self.flush_each_entry_ = flush_each_entry;
            return std::forward<Self>(self);
        }
        template <typename Self>
        constexpr auto&& rotation(this Self&& self, const bool enabling_rotation) noexcept {
            self.rotate_file_ = enabling_rotation;
            return std::forward<Self>(self);
        }
        [[nodiscard]] constexpr LogLevel get_threshold() const noexcept {
            return threshold_;
        }
        [[nodiscard]] constexpr const std::filesystem::path& get_file() const noexcept {
            return file_;
        }
        [[nodiscard]] constexpr bool is_add_time_to_filename() const noexcept {
            return add_time_to_filename_;
        }
        [[nodiscard]] constexpr const std::string& get_time_format_in_file_name() const noexcept {
            return time_format_in_file_name_;
        }
        [[nodiscard]] constexpr std::uint64_t get_max_file_size() const noexcept {
            return max_file_size_;
        }
        [[nodiscard]] constexpr bool is_flush_each_entry() const noexcept {
            return flush_each_entry_;
        }
        [[nodiscard]] constexpr bool do_rotate() const noexcept {
            return rotate_file_;
        }

    private:
        LogLevel threshold_ = LogLevel::Debug;
        std::filesystem::path file_;
        bool add_time_to_filename_            = true;
        std::string time_format_in_file_name_ = chrono::clock_formats::iso8601;

        bool rotate_file_            = true;
        std::uint64_t max_file_size_ = gears::literals::operator""_mb(100);
        bool flush_each_entry_       = false;
    };
}  // namespace demiplane::scroll
