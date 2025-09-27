#include "postgres_params.hpp"

#include <netinet/in.h>

namespace demiplane::db {

    void PostgresParamSink::bind_one(std::monostate) const {
        params_->values.push_back(nullptr);
        params_->lengths.push_back(0);
        params_->formats.push_back(0);
        params_->oids.push_back(0);  // Let PG infer
    }

    void PostgresParamSink::bind_one(const bool b) const {
        // Use binary format for bool
        const auto offset = params_->binary_data.size();
        params_->binary_data.push_back(b ? std::byte{1} : std::byte{0});

        params_->values.push_back(reinterpret_cast<const char*>(params_->binary_data.data() + offset));
        params_->lengths.push_back(1);
        params_->formats.push_back(1);  // Binary
        params_->oids.push_back(reg_.oid_bool);
    }

    void PostgresParamSink::bind_one(const std::int32_t i) const {
        // Binary format - network byte order
        const auto offset = params_->binary_data.size();
        params_->binary_data.resize(offset + 4);
        auto* ptr = params_->binary_data.data() + offset;

        // Convert to network byte order (big-endian)
        const uint32_t net = htonl(static_cast<uint32_t>(i));
        std::memcpy(ptr, &net, 4);

        params_->values.push_back(reinterpret_cast<const char*>(ptr));
        params_->lengths.push_back(4);
        params_->formats.push_back(1);  // Binary
        params_->oids.push_back(reg_.oid_int4);
    }

    void PostgresParamSink::bind_one(const std::int64_t i) const {
        const auto offset = params_->binary_data.size();
        params_->binary_data.resize(offset + 8);
        auto* ptr = params_->binary_data.data() + offset;

        // Convert to network byte order
        const uint64_t net = htobe64(static_cast<uint64_t>(i));
        std::memcpy(ptr, &net, 8);

        params_->values.push_back(reinterpret_cast<const char*>(ptr));
        params_->lengths.push_back(8);
        params_->formats.push_back(1);  // Binary
        params_->oids.push_back(reg_.oid_int8);
    }

    void PostgresParamSink::bind_one(const double d) const {
        const auto offset = params_->binary_data.size();
        params_->binary_data.resize(offset + 8);
        auto* ptr = params_->binary_data.data() + offset;

        // PostgreSQL uses network byte order for floats
        uint64_t bits;
        std::memcpy(&bits, &d, 8);
        bits = htobe64(bits);
        std::memcpy(ptr, &bits, 8);

        params_->values.push_back(reinterpret_cast<const char*>(ptr));
        params_->lengths.push_back(8);
        params_->formats.push_back(1);  // Binary
        params_->oids.push_back(reg_.oid_float8);
    }

    void PostgresParamSink::bind_one(const std::string& s) const {
        // Text format for strings (no conversion needed)
        std::pmr::string pmr_str{s, mr_};  // copy
        params_->keeparams_str.emplace_back(std::move(pmr_str));
        params_->values.push_back(params_->keeparams_str.back().c_str());
        params_->lengths.push_back(static_cast<int>(params_->keeparams_str.back().size()));
        params_->formats.push_back(0);  // Text
        params_->oids.push_back(reg_.oid_text);
    }

    void PostgresParamSink::bind_one(const std::span<const uint8_t> bytes) const {
        // Binary data as BYTEA
        const auto offset = params_->binary_data.size();
        params_->binary_data.resize(offset + bytes.size());
        std::memcpy(params_->binary_data.data() + offset, bytes.data(), bytes.size());

        params_->values.push_back(reinterpret_cast<const char*>(params_->binary_data.data() + offset));
        params_->lengths.push_back(static_cast<int>(bytes.size()));
        params_->formats.push_back(1);  // Binary
        params_->oids.push_back(reg_.oid_bytea);
    }
    void PostgresParamSink::bind_one(std::string_view sview) const {
        std::pmr::string pmr_str{sview, mr_};  // copy
        params_->keeparams_str.emplace_back(std::move(pmr_str));
        params_->values.push_back(params_->keeparams_str.back().c_str());
        params_->lengths.push_back(static_cast<int>(params_->keeparams_str.back().size()));
        params_->formats.push_back(0);  // Text
        params_->oids.push_back(reg_.oid_text);
    }
}  // namespace demiplane::db
