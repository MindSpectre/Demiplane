#pragma once
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <pg_oid_type_registry.hpp>
#include <sql_params.hpp>

namespace demiplane::db::postgres {

    struct Params {
        std::pmr::vector<const char*> values;
        std::pmr::vector<int> lengths;
        std::pmr::vector<int> formats;  // 0 text, 1 binary
        std::pmr::vector<unsigned> oids;
        std::pmr::deque<std::pmr::string> str_data;                  // deque for pointer stability
        std::pmr::deque<std::pmr::vector<std::byte>> binary_chunks;  // each binary param in its own vector
    };

    class ParamSink final : public db::ParamSink {
    public:
        explicit ParamSink(std::pmr::memory_resource* mr)
            : mr_(mr),
              params_(std::make_shared<Params>(
                  Params{.values{mr}, .lengths{mr}, .formats{mr}, .oids{mr}, .str_data{mr}, .binary_chunks{mr}})) {
        }

        std::size_t push(const FieldValue& v) override {
            std::visit([this](const auto& x) { bind_one(x); }, v);
            return params_->values.size();  // 1-based index for PostgreSQL
        }

        /// @warning No true move. Must be copied
        std::size_t push(FieldValue&& v) override {
            return push(v);
        }

        /// @brief Expose packet via shared_ptr<void>
        [[nodiscard]] std::shared_ptr<void> packet() const noexcept override {
            return params_;
        }
        /// @brief Expose packet in native format
        [[nodiscard]] std::shared_ptr<Params> native_packet() const noexcept {
            return params_;
        }

        // TODO: think about adding binary format flag(indicate dont use binary format at all)
    private:
        // Specializations for each type
        void bind_one(std::monostate) const;

        void bind_one(bool b) const;

        void bind_one(char c) const;

        void bind_one(std::int16_t i) const;

        void bind_one(std::int32_t i) const;

        void bind_one(std::int64_t i) const;

        void bind_one(std::uint16_t i) const;

        void bind_one(std::uint32_t i) const;

        void bind_one(std::uint64_t i) const;

        void bind_one(float f) const;

        void bind_one(double d) const;

        void bind_one(const std::string& s) const;

        void bind_one(std::string_view s_view) const;

        void bind_one(const std::vector<std::uint8_t>& bytes) const;

        void bind_one(std::span<const std::uint8_t> bytes) const;

        std::pmr::memory_resource* mr_;
        std::shared_ptr<Params> params_;
    };

}  // namespace demiplane::db::postgres
