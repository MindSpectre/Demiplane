#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "aliases.hpp"

namespace demiplane::http {

    class RequestContext {
    public:
        explicit RequestContext(Request req);

        // Path parameter access with type safety
        template <typename T>
        std::optional<T> path(std::string_view name) const;

        template <typename T>
        T path_or(std::string_view name, T default_value) const;

        // Query parameter access
        template <typename T>
        std::optional<T> query(std::string_view name) const;

        template <typename T>
        T query_or(std::string_view name, T default_value) const;

        // Header access
        std::optional<std::string> header(std::string_view name) const;
        std::string header_or(std::string_view name, std::string_view default_value) const;

        // Request data
        std::string body() const {
            return request_.body();
        }
        std::string method() const {
            return std::string(request_.method_string());
        }
        std::string target() const {
            return std::string(request_.target());
        }
        std::string path_only() const;
        std::string query_string() const;

        // Content type helpers
        bool is_json() const;
        bool is_form_data() const;
        bool accepts_json() const;
        bool accepts_html() const;
        std::string preferred_content_type() const;

        // Internal methods for framework use
        void set_path_params(std::unordered_map<std::string, std::string> params);
        void set_query_params(std::unordered_map<std::string, std::string> params);

    private:
        Request request_;

        // Private maps - hidden from public interface
        std::unordered_map<std::string, std::string> path_params_;
        std::unordered_map<std::string, std::string> query_params_;
        std::unordered_map<std::string, std::string> headers_;

        void parse_headers();

        template <typename T>
        std::optional<T> convert_string(const std::string& value) const;
    };

    // Template specializations
    template <>
    std::optional<int> RequestContext::convert_string<int>(const std::string& value) const;

    template <>
    std::optional<long> RequestContext::convert_string<long>(const std::string& value) const;

    template <>
    std::optional<double> RequestContext::convert_string<double>(const std::string& value) const;

    template <>
    std::optional<std::string> RequestContext::convert_string<std::string>(const std::string& value) const;

} // namespace demiplane::http
