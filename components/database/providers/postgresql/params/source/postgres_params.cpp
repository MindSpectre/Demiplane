#include "postgres_params.hpp"

#include <netinet/in.h>

#include "pg_format_registry.hpp"

namespace demiplane::db::postgres {

    void ParamSink::bind_one(std::monostate) const {
        params_->values.push_back(nullptr);
        params_->lengths.push_back(0);
        params_->formats.push_back(0);
        params_->oids.push_back(0);  // Let PG infer
    }

    void ParamSink::bind_one(const bool b) const {
        // Use binary format for bool
        params_->binary_chunks.emplace_back();
        params_->binary_chunks.back().push_back(b ? std::byte{1} : std::byte{0});

        params_->values.push_back(reinterpret_cast<const char*>(params_->binary_chunks.back().data()));
        params_->lengths.push_back(1);
        params_->formats.push_back(FormatRegistry::binary);
        params_->oids.push_back(TypeRegistry::oid_bool);
    }

    void ParamSink::bind_one(const std::int32_t i) const {
        // Binary format - network byte order
        params_->binary_chunks.emplace_back(4);
        auto* ptr = params_->binary_chunks.back().data();

        // Convert to network byte order (big-endian)
        const uint32_t net = htonl(static_cast<uint32_t>(i));
        std::memcpy(ptr, &net, 4);

        params_->values.push_back(reinterpret_cast<const char*>(ptr));
        params_->lengths.push_back(4);
        params_->formats.push_back(FormatRegistry::binary);
        params_->oids.push_back(TypeRegistry::oid_int4);
    }

    void ParamSink::bind_one(const std::int64_t i) const {
        params_->binary_chunks.emplace_back(8);
        auto* ptr = params_->binary_chunks.back().data();

        // Convert to network byte order
        const uint64_t net = htobe64(static_cast<uint64_t>(i));
        std::memcpy(ptr, &net, 8);

        params_->values.push_back(reinterpret_cast<const char*>(ptr));
        params_->lengths.push_back(8);
        params_->formats.push_back(FormatRegistry::binary);
        params_->oids.push_back(TypeRegistry::oid_int8);
    }

    void ParamSink::bind_one(const float f) const {
        params_->binary_chunks.emplace_back(4);
        auto* ptr = params_->binary_chunks.back().data();

        // PostgreSQL uses network byte order for floats
        uint32_t bits;
        std::memcpy(&bits, &f, 4);
        bits = htobe32(bits);
        std::memcpy(ptr, &bits, 4);

        params_->values.push_back(reinterpret_cast<const char*>(ptr));
        params_->lengths.push_back(4);
        params_->formats.push_back(FormatRegistry::binary);
        params_->oids.push_back(TypeRegistry::oid_float4);
    }

    void ParamSink::bind_one(const double d) const {
        params_->binary_chunks.emplace_back(8);
        auto* ptr = params_->binary_chunks.back().data();

        // PostgreSQL uses network byte order for floats
        uint64_t bits;
        std::memcpy(&bits, &d, 8);
        bits = htobe64(bits);
        std::memcpy(ptr, &bits, 8);

        params_->values.push_back(reinterpret_cast<const char*>(ptr));
        params_->lengths.push_back(8);
        params_->formats.push_back(FormatRegistry::binary);  // Binary
        params_->oids.push_back(TypeRegistry::oid_float8);
    }

    void ParamSink::bind_one(const std::string& s) const {
        // Text format for strings (no conversion needed)
        std::pmr::string pmr_str{s, mr_};  // copy
        params_->str_data.emplace_back(std::move(pmr_str));
        params_->values.push_back(params_->str_data.back().c_str());
        params_->lengths.push_back(static_cast<int>(params_->str_data.back().size()));
        params_->formats.push_back(FormatRegistry::text);  // Text
        params_->oids.push_back(TypeRegistry::oid_text);
    }
    void ParamSink::bind_one(const std::string_view s_view) const {
        std::pmr::string pmr_str{s_view, mr_};  // copy
        params_->str_data.emplace_back(std::move(pmr_str));
        params_->values.push_back(params_->str_data.back().c_str());
        params_->lengths.push_back(static_cast<int>(params_->str_data.back().size()));
        params_->formats.push_back(FormatRegistry::text);
        params_->oids.push_back(TypeRegistry::oid_text);
    }
    void ParamSink::bind_one(const std::span<const std::uint8_t> bytes) const {
        // Binary data as BYTEA
        params_->binary_chunks.emplace_back(bytes.size());
        std::memcpy(params_->binary_chunks.back().data(), bytes.data(), bytes.size());

        params_->values.push_back(reinterpret_cast<const char*>(params_->binary_chunks.back().data()));
        params_->lengths.push_back(static_cast<int>(bytes.size()));
        params_->formats.push_back(FormatRegistry::binary);  // Binary
        params_->oids.push_back(TypeRegistry::oid_bytea);
    }
    void ParamSink::bind_one(const std::vector<std::uint8_t>& bytes) const {
        params_->binary_chunks.emplace_back(bytes.size());
        std::memcpy(params_->binary_chunks.back().data(), bytes.data(), bytes.size());

        params_->values.push_back(reinterpret_cast<const char*>(params_->binary_chunks.back().data()));
        params_->lengths.push_back(static_cast<int>(bytes.size()));
        params_->formats.push_back(FormatRegistry::binary);  // Binary
        params_->oids.push_back(TypeRegistry::oid_bytea);
    }


}  // namespace demiplane::db::postgres
