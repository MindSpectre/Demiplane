#pragma once
#include <cstring>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <pg_type_registry.hpp>
#include <sql_params.hpp>

namespace demiplane::db {

    struct PostgresParams {
        std::pmr::vector<const char*> values;
        std::pmr::vector<int> lengths;
        std::pmr::vector<int> formats;  // 0 text, 1 binary
        std::pmr::vector<unsigned> oids;
        std::pmr::vector<std::pmr::string> keeparams_str;  // adopt/move strings
        std::pmr::vector<std::byte> binary_data;
    };

    class PostgresParamSink final : public ParamSink {
    public:
        PostgresParamSink(std::pmr::memory_resource* mr, const PgTypeRegistry& reg)
            : mr_(mr),
              params_(std::make_shared<PostgresParams>(PostgresParams{
                  .values{mr}, .lengths{mr}, .formats{mr}, .oids{mr}, .keeparams_str{mr}, .binary_data{mr}})),
              reg_(reg) {
        }

        std::size_t push(const FieldValue& v) override {
            std::visit([this](const auto& x) { bind_one(x); }, v);
            return params_->values.size();  // 1-based index for PostgreSQL
        }

        /// @warning No true move. Must be copied
        std::size_t push(FieldValue&& v) override {
            return push(v);
        }
        std::shared_ptr<void> packet() override {
            return params_;
        }

        // expose packet via shared_ptr<void>

    private:
        // Specializations for each type
        void bind_one(std::monostate) const;

        void bind_one(bool b) const;

        void bind_one(std::int32_t i) const;

        void bind_one(std::int64_t i) const;

        void bind_one(double d) const;

        void bind_one(const std::string& s) const;

        void bind_one(std::span<const uint8_t> bytes) const;

        void bind_one(std::string_view sview) const;
        // helpers …
        const char* clone(const void* src, const std::size_t n) const {
            auto* buf = static_cast<char*>(mr_->allocate(n, alignof(char)));
            std::memcpy(buf, src, n);
            return buf;
        }

        std::pmr::memory_resource* mr_;
        std::shared_ptr<PostgresParams> params_;
        PgTypeRegistry reg_;
    };

}  // namespace demiplane::db
