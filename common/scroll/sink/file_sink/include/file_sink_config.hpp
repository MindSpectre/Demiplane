#pragma once

#include <demiplane/chrono>
#include <demiplane/gears>
#include <filesystem>

#include <config_interface.hpp>
#include <json/json.hpp>
#include <sink_interface.hpp>

namespace demiplane::scroll {

    class FileSinkConfig final : public serialization::ConfigInterface<FileSinkConfig, Json::Value> {
    public:
        // Full constructor (escape hatch)
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

        constexpr void validate() const override {
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

        [[nodiscard]] constexpr LogLevel threshold() const noexcept {
            return threshold_;
        }
        [[nodiscard]] constexpr const std::filesystem::path& file() const noexcept {
            return file_;
        }
        [[nodiscard]] constexpr bool add_time_to_filename() const noexcept {
            return add_time_to_filename_;
        }
        [[nodiscard]] constexpr const std::string& time_format_in_file_name() const noexcept {
            return time_format_in_file_name_;
        }
        [[nodiscard]] constexpr std::uint64_t max_file_size() const noexcept {
            return max_file_size_;
        }
        [[nodiscard]] constexpr bool flush_each_entry() const noexcept {
            return flush_each_entry_;
        }
        [[nodiscard]] constexpr bool rotate_file() const noexcept {
            return rotate_file_;
        }

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&FileSinkConfig::threshold_, "threshold">{},
                serialization::Field<&FileSinkConfig::file_, "file">{},
                serialization::Field<&FileSinkConfig::add_time_to_filename_, "add_time_to_filename">{},
                serialization::Field<&FileSinkConfig::time_format_in_file_name_, "time_format_in_file_name">{},
                serialization::Field<&FileSinkConfig::rotate_file_, "rotate_file">{},
                serialization::Field<&FileSinkConfig::max_file_size_, "max_file_size">{},
                serialization::Field<&FileSinkConfig::flush_each_entry_, "flush_each_entry">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr FileSinkConfig() = default;

        LogLevel threshold_ = LogLevel::Debug;
        std::filesystem::path file_;
        bool add_time_to_filename_            = true;
        std::string time_format_in_file_name_ = chrono::clock_formats::iso8601;

        bool rotate_file_            = true;
        std::uint64_t max_file_size_ = gears::literals::operator""_mb(100);
        bool flush_each_entry_       = false;
    };

    class FileSinkConfig::Builder {
    public:
        Builder() = default;
        explicit Builder(const FileSinkConfig& existing)
            : config_{existing} {
        }
        explicit Builder(FileSinkConfig&& existing)
            : config_{std::move(existing)} {
        }

        template <typename Self>
        constexpr auto&& threshold(this Self&& self, const LogLevel value) noexcept {
            self.config_.threshold_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& file(this Self&& self, std::filesystem::path value) noexcept {
            self.config_.file_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& add_time_to_filename(this Self&& self, const bool value) noexcept {
            self.config_.add_time_to_filename_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& time_format_in_file_name(this Self&& self, std::string value) noexcept {
            self.config_.time_format_in_file_name_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& max_file_size(this Self&& self, const std::uint64_t value) noexcept {
            self.config_.max_file_size_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& flush_each_entry(this Self&& self, const bool value) noexcept {
            self.config_.flush_each_entry_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& rotation(this Self&& self, const bool value) noexcept {
            self.config_.rotate_file_ = value;
            return std::forward<Self>(self);
        }

        [[nodiscard]] FileSinkConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class FileSinkConfig;
        friend class ConfigInterface;
        FileSinkConfig config_;
    };

}  // namespace demiplane::scroll
