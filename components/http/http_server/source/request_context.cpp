
#include "request_context.hpp"

#include <algorithm>
#include <demiplane/gears>
#include <json/json.h> // You'll need to add jsoncpp dependency
#include <sstream>
namespace demiplane::http {

    RequestContext::RequestContext(Request req) : request_(std::move(req)) {
        parse_headers();
    }

    void RequestContext::parse_headers() {
        for (const auto& field : request_) {
            auto name        = std::string(field.name_string());
            const auto value = std::string(field.value());
            std::ranges::transform(name, name.begin(), ::tolower);
            headers_[name] = value;
        }
    }

    std::string RequestContext::path_only() const {
        auto target_str      = target();
        const auto query_pos = target_str.find('?');
        return query_pos != std::string::npos ? target_str.substr(0, query_pos) : target_str;
    }

    std::string RequestContext::query_string() const {
        const auto target_str = target();
        const auto query_pos  = target_str.find('?');
        return query_pos != std::string::npos ? target_str.substr(query_pos + 1) : "";
    }

    bool RequestContext::is_json() const {
        const auto content_type = header("content-type");
        return content_type && content_type->find("application/json") != std::string::npos;
    }

    bool RequestContext::is_form_data() const {
        const auto content_type = header("content-type");
        return content_type && content_type->find("application/x-www-form-urlencoded") != std::string::npos;
    }

    bool RequestContext::is_multipart() const {
        const auto content_type = header("content-type");
        return content_type && content_type->find("multipart/form-data") != std::string::npos;
    }

    // JSON parsing
    std::optional<Json::Value> RequestContext::json() const {
        if (!is_json()) {
            return std::nullopt;
        }

        if (!cached_json_.has_value()) {
            try {
                cached_json_ = parse_json_body();
            } catch ([[maybe_unused]] const std::runtime_error& e) {
                cached_json_ = std::nullopt;
            }
        }

        return cached_json_;
    }

    Json::Value RequestContext::parse_json_body() const {
        Json::Value root;
        std::string errors;

        const auto body_str = body();
        std::istringstream stream(body_str);

        if (const Json::CharReaderBuilder builder; !Json::parseFromStream(builder, stream, &root, &errors)) {
            throw std::runtime_error("Failed to parse JSON: " + errors);
        }

        return root;
    }

    // Form data parsing
    std::optional<std::unordered_map<std::string, std::string>> RequestContext::form_data() const {
        if (!is_form_data()) {
            return std::nullopt;
        }

        if (!cached_form_data_.has_value()) {
            try {
                cached_form_data_ = parse_form_data_body();
            } catch (const std::exception&) {
                cached_form_data_ = std::nullopt;
            }
        }

        return cached_form_data_;
    }

    std::unordered_map<std::string, std::string> RequestContext::parse_form_data_body() const {
        std::unordered_map<std::string, std::string> data;
        const auto body_str = body();

        std::istringstream stream(body_str);
        std::string pair;

        while (std::getline(stream, pair, '&')) {
            if (const auto eq_pos = pair.find('='); eq_pos != std::string::npos) {
                auto key         = pair.substr(0, eq_pos);
                const auto value = pair.substr(eq_pos + 1);

                // URL decode key and value here if needed
                data[key] = value;
            }
        }

        return data;
    }
    

    // Multipart parsing (basic implementation)
    std::optional<std::vector<RequestContext::MultipartField>> RequestContext::multipart_data() const {
        if (!is_multipart()) {
            return std::nullopt;
        }

        if (!cached_multipart_data_.has_value()) {
            try {
                cached_multipart_data_ = parse_multipart_body();
            } catch (const std::exception&) {
                cached_multipart_data_ = std::nullopt;
            }
        }

        return cached_multipart_data_;
    }

    std::vector<RequestContext::MultipartField> RequestContext::parse_multipart_body() const {
        // This is a simplified implementation
        // In production, you'd want to use a proper multipart parser
        std::vector<MultipartField> fields;

        // Extract boundary from Content-Type header
        const auto content_type = header("content-type");
        if (!content_type) {
            return fields;
        }

        const auto boundary_pos = content_type->find("boundary=");
        if (boundary_pos == std::string::npos) {
            return fields;
        }

        std::string boundary = "--" + content_type->substr(boundary_pos + 9);

        // Parse multipart data (simplified)
        const auto body_str = body();
        // Implementation would split by boundary and parse each part
        // This is complex and would typically use a library like cpp-httplib's multipart parser

        return fields;
    }

    // Rest of the existing methods...
    bool RequestContext::accepts_json() const {
        const auto accept = header("accept");
        return accept
            && (accept->find("application/json") != std::string::npos || accept->find("*/*") != std::string::npos);
    }

    bool RequestContext::accepts_html() const {
        const auto accept = header("accept");
        return accept && (accept->find("text/html") != std::string::npos || accept->find("*/*") != std::string::npos);
    }

    std::string RequestContext::preferred_content_type() const {
        if (accepts_json()) {
            return "application/json";
        }
        if (accepts_html()) {
            return "text/html";
        }
        return "text/plain";
    }

    std::optional<std::string> RequestContext::header(const std::string_view name) const {
        std::string lower_name{name};
        std::ranges::transform(lower_name, lower_name.begin(), ::tolower);

        if (const auto it = headers_.find(lower_name); it != headers_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::string RequestContext::header_or(const std::string_view name, const std::string_view default_value) const {
        if (auto value = header(name)) {
            return *value;
        }
        return std::string{default_value};
    }

    void RequestContext::set_path_params(std::unordered_map<std::string, std::string> params) {
        path_params_ = std::move(params);
    }

    void RequestContext::set_query_params(std::unordered_map<std::string, std::string> params) {
        query_params_ = std::move(params);
    }

    // Template specializations
    template <>
    std::optional<int> RequestContext::convert_string<int>(const std::string& value) const {
        try {
            return std::stoi(value);
        } catch (...) {
            return std::nullopt;
        }
    }

    template <>
    std::optional<long> RequestContext::convert_string<long>(const std::string& value) const {
        try {
            return std::stol(value);
        } catch (...) {
            return std::nullopt;
        }
    }

    template <>
    std::optional<double> RequestContext::convert_string<double>(const std::string& value) const {
        try {
            return std::stod(value);
        } catch (...) {
            return std::nullopt;
        }
    }

    template <>
    std::optional<std::string> RequestContext::convert_string<std::string>(const std::string& value) const {
        return value;
    }

    // Template method implementations
    template <typename T>
    std::optional<T> RequestContext::path(const std::string_view name) const {
        if (const auto it = path_params_.find(std::string{name}); it != path_params_.end()) {
            return convert_string<T>(it->second);
        }
        return std::nullopt;
    }

    template <typename T>
    T RequestContext::path_or(const std::string_view name, T default_value) const {
        if (auto value = path<T>(name)) {
            return *value;
        }
        return default_value;
    }

    template <typename T>
    std::optional<T> RequestContext::query(const std::string_view name) const {
        if (const auto it = query_params_.find(std::string{name}); it != query_params_.end()) {
            return convert_string<T>(it->second);
        }
        return std::nullopt;
    }

    template <typename T>
    T RequestContext::query_or(const std::string_view name, T default_value) const {
        if (auto value = query<T>(name)) {
            return *value;
        }
        return default_value;
    }

    // Explicit template instantiations
    template std::optional<int> RequestContext::path<int>(std::string_view) const;
    template std::optional<long> RequestContext::path<long>(std::string_view) const;
    template std::optional<double> RequestContext::path<double>(std::string_view) const;
    template std::optional<std::string> RequestContext::path<std::string>(std::string_view) const;

    template int RequestContext::path_or<int>(std::string_view, int) const;
    template long RequestContext::path_or<long>(std::string_view, long) const;
    template double RequestContext::path_or<double>(std::string_view, double) const;
    template std::string RequestContext::path_or<std::string>(std::string_view, std::string) const;

    template std::optional<int> RequestContext::query<int>(std::string_view) const;
    template std::optional<long> RequestContext::query<long>(std::string_view) const;
    template std::optional<double> RequestContext::query<double>(std::string_view) const;
    template std::optional<std::string> RequestContext::query<std::string>(std::string_view) const;

    template int RequestContext::query_or<int>(std::string_view, int) const;
    template long RequestContext::query_or<long>(std::string_view, long) const;
    template double RequestContext::query_or<double>(std::string_view, double) const;
    template std::string RequestContext::query_or<std::string>(std::string_view, std::string) const;

} // namespace demiplane::http
