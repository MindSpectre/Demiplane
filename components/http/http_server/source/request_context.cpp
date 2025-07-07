#include "request_context.hpp"

#include <algorithm>

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
