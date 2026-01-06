// PostgreSQL ParamSink Functional Tests
// Tests parameter binding and encoding with actual PostgreSQL

#include "postgres_params.hpp"

#include <cmath>
#include <memory_resource>

#include <gtest/gtest.h>
#include <netinet/in.h>

#include "pg_format_registry.hpp"
#include "pg_oid_type_registry.hpp"
#include "postgres_result.hpp"

using namespace demiplane::db;
using postgres::FormatRegistry;
using postgres::OidTypeRegistry;

// Test fixture for PostgreSQL params
class PostgresParamsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get connection parameters from environment or use defaults
        const char* host     = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
        const char* port     = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5433";
        const char* dbname   = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "test_db";
        const char* user     = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "test_user";
        const char* password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "test_password";

        // Create connection string
        const std::string conn_info = "host=" + std::string(host) + " port=" + std::string(port) +
                                      " dbname=" + std::string(dbname) + " user=" + std::string(user) +
                                      " password=" + std::string(password);

        // Connect to database
        conn_ = PQconnectdb(conn_info.c_str());

        // Check connection status
        if (PQstatus(conn_) != CONNECTION_OK) {
            const std::string error = PQerrorMessage(conn_);
            PQfinish(conn_);
            conn_ = nullptr;
            GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error
                         << "\nSet POSTGRES_HOST, POSTGRES_PORT, POSTGRES_DB, POSTGRES_USER, POSTGRES_PASSWORD";
        }
    }

    void TearDown() override {
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    PGconn* conn_{nullptr};
};

// ============== Integration Tests with PostgreSQL ==============

TEST_F(PostgresParamsTest, RoundTripNull) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::monostate{}});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);

    EXPECT_TRUE(PQgetisnull(result, 0, 0));

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripBool) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{true});
    sink.push(FieldValue{false});
    const auto params = sink.native_packet();

    // Test true
    PGresult* result = PQexecParams(
        conn_, "SELECT $1::bool", 1, &params->oids[0], &params->values[0], &params->lengths[0], &params->formats[0], 1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
    EXPECT_EQ(static_cast<unsigned char>(PQgetvalue(result, 0, 0)[0]), 1);
    PQclear(result);

    // Test false
    result = PQexecParams(
        conn_, "SELECT $1::bool", 1, &params->oids[1], &params->values[1], &params->lengths[1], &params->formats[1], 1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
    EXPECT_EQ(static_cast<unsigned char>(PQgetvalue(result, 0, 0)[0]), 0);
    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripChar) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    const std::vector<char> test_chars = {'A', 'Z', '0', '9', ' ', '\0'};

    for (auto c : test_chars) {
        sink.push(FieldValue{c});
    }

    const auto params = sink.native_packet();

    for (size_t i = 0; i < test_chars.size(); ++i) {
        // PostgreSQL "char" type (internal 1-byte type)
        PGresult* result = PQexecParams(conn_,
                                        "SELECT $1::\"char\"",
                                        1,
                                        &params->oids[i],
                                        &params->values[i],
                                        &params->lengths[i],
                                        &params->formats[i],
                                        1);
        ASSERT_NE(result, nullptr);
        ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
        EXPECT_EQ(PQgetvalue(result, 0, 0)[0], test_chars[i]);
        PQclear(result);
    }
}

TEST_F(PostgresParamsTest, RoundTripInt16) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::int16_t{12345}});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::int2",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);

    uint16_t net_value;
    std::memcpy(&net_value, PQgetvalue(result, 0, 0), 2);
    EXPECT_EQ(static_cast<int16_t>(ntohs(net_value)), 12345);

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripInt32) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::int32_t{12345}});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::int4",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
    EXPECT_EQ(PQntuples(result), 1);

    uint32_t net_value;
    std::memcpy(&net_value, PQgetvalue(result, 0, 0), 4);
    EXPECT_EQ(static_cast<int32_t>(ntohl(net_value)), 12345);

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripInt64) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::int64_t{9223372036854775807LL}});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::int8",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);

    uint64_t net_value;
    std::memcpy(&net_value, PQgetvalue(result, 0, 0), 8);
    EXPECT_EQ(static_cast<int64_t>(be64toh(net_value)), 9223372036854775807LL);

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripUInt16) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    // uint16 is promoted to int4 in PostgreSQL
    sink.push(FieldValue{std::uint16_t{65535}});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::int4",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);

    uint32_t net_value;
    std::memcpy(&net_value, PQgetvalue(result, 0, 0), 4);
    EXPECT_EQ(ntohl(net_value), 65535u);

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripUInt32) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    // uint32 is promoted to int8 in PostgreSQL
    sink.push(FieldValue{std::uint32_t{4294967295u}});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::int8",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);

    uint64_t net_value;
    std::memcpy(&net_value, PQgetvalue(result, 0, 0), 8);
    EXPECT_EQ(be64toh(net_value), 4294967295u);

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripUInt64) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    // uint64 uses NUMERIC in PostgreSQL (text format)
    sink.push(FieldValue{std::uint64_t{18446744073709551615ULL}});
    const auto params = sink.native_packet();

    // Request text format result for NUMERIC
    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::numeric",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    0);  // Text result format

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);

    const std::string result_str = PQgetvalue(result, 0, 0);
    EXPECT_EQ(result_str, "18446744073709551615");

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripFloat) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{3.14159f});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::float4",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);

    uint32_t net_bits;
    std::memcpy(&net_bits, PQgetvalue(result, 0, 0), 4);
    net_bits = be32toh(net_bits);
    float result_float;
    std::memcpy(&result_float, &net_bits, 4);
    EXPECT_FLOAT_EQ(result_float, 3.14159f);

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripDouble) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{2.718281828459045});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::float8",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);

    uint64_t net_bits;
    std::memcpy(&net_bits, PQgetvalue(result, 0, 0), 8);
    net_bits = be64toh(net_bits);
    double result_double;
    std::memcpy(&result_double, &net_bits, 8);
    EXPECT_DOUBLE_EQ(result_double, 2.718281828459045);

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripString) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::string{"Hello, PostgreSQL!"}});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::text",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    0);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);

    const std::string result_str = PQgetvalue(result, 0, 0);
    EXPECT_EQ(result_str, "Hello, PostgreSQL!");

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripByteArray) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    std::vector<uint8_t> bytes = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF};
    sink.push(FieldValue{bytes});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::bytea",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);

    const int result_len    = PQgetlength(result, 0, 0);
    const char* result_data = PQgetvalue(result, 0, 0);

    ASSERT_EQ(result_len, static_cast<int>(bytes.size()));
    for (size_t i = 0; i < bytes.size(); ++i) {
        EXPECT_EQ(static_cast<uint8_t>(result_data[i]), bytes[i]);
    }

    PQclear(result);
}

TEST_F(PostgresParamsTest, RoundTripMultipleTypes) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::int32_t{42}});
    sink.push(FieldValue{std::string{"test"}});
    sink.push(FieldValue{true});
    sink.push(FieldValue{3.14});

    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::int4, $2::text, $3::bool, $4::float8",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
    EXPECT_EQ(PQnfields(result), 4);

    // Verify int32
    uint32_t int_val;
    std::memcpy(&int_val, PQgetvalue(result, 0, 0), 4);
    EXPECT_EQ(static_cast<int32_t>(ntohl(int_val)), 42);

    // Verify text (binary format returns text as-is)
    const int text_len = PQgetlength(result, 0, 1);
    const std::string text_val(PQgetvalue(result, 0, 1), static_cast<size_t>(text_len));
    EXPECT_EQ(text_val, "test");

    // Verify bool
    EXPECT_EQ(static_cast<unsigned char>(PQgetvalue(result, 0, 2)[0]), 1);

    // Verify double
    uint64_t double_bits;
    std::memcpy(&double_bits, PQgetvalue(result, 0, 3), 8);
    double_bits = be64toh(double_bits);
    double double_val;
    std::memcpy(&double_val, &double_bits, 8);
    EXPECT_DOUBLE_EQ(double_val, 3.14);

    PQclear(result);
}

// ============== Edge Cases ==============

TEST_F(PostgresParamsTest, Int16EdgeCases) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    const std::vector<std::int16_t> test_values = {
        0,
        1,
        -1,
        std::numeric_limits<std::int16_t>::min(),
        std::numeric_limits<std::int16_t>::max(),
        32767,
        -32768,
    };

    for (auto val : test_values) {
        sink.push(FieldValue{val});
    }

    const auto params = sink.native_packet();

    for (size_t i = 0; i < test_values.size(); ++i) {
        PGresult* result = PQexecParams(conn_,
                                        "SELECT $1::int2",
                                        1,
                                        &params->oids[i],
                                        &params->values[i],
                                        &params->lengths[i],
                                        &params->formats[i],
                                        1);
        ASSERT_NE(result, nullptr);
        ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
        uint16_t net_value;
        std::memcpy(&net_value, PQgetvalue(result, 0, 0), 2);
        EXPECT_EQ(static_cast<int16_t>(ntohs(net_value)), test_values[i]);
        PQclear(result);
    }
}

TEST_F(PostgresParamsTest, Int32EdgeCases) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    const std::vector<std::int32_t> test_values = {
        0,
        1,
        -1,
        std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::min() + 1,
        std::numeric_limits<std::int32_t>::max() - 1,
        2147483647,
        -2147483648,
    };

    for (auto val : test_values) {
        sink.push(FieldValue{val});
    }

    const auto params = sink.native_packet();
    ASSERT_EQ(params->values.size(), test_values.size());

    for (size_t i = 0; i < test_values.size(); ++i) {
        PGresult* result = PQexecParams(conn_,
                                        "SELECT $1::int4",
                                        1,
                                        &params->oids[i],
                                        &params->values[i],
                                        &params->lengths[i],
                                        &params->formats[i],
                                        1);
        ASSERT_NE(result, nullptr);
        ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
        uint32_t net_value;
        std::memcpy(&net_value, PQgetvalue(result, 0, 0), 4);
        EXPECT_EQ(static_cast<int32_t>(ntohl(net_value)), test_values[i]);
        PQclear(result);
    }
}

TEST_F(PostgresParamsTest, Int64EdgeCases) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    const std::vector<std::int64_t> test_values = {
        0LL,
        1LL,
        -1LL,
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max(),
        9223372036854775807LL,
        -9223372036854775807LL - 1LL,
        2147483648LL,
        -2147483649LL,
    };

    for (auto val : test_values) {
        sink.push(FieldValue{val});
    }

    const auto params = sink.native_packet();

    for (size_t i = 0; i < test_values.size(); ++i) {
        PGresult* result = PQexecParams(conn_,
                                        "SELECT $1::int8",
                                        1,
                                        &params->oids[i],
                                        &params->values[i],
                                        &params->lengths[i],
                                        &params->formats[i],
                                        1);
        ASSERT_NE(result, nullptr);
        ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
        uint64_t net_value;
        std::memcpy(&net_value, PQgetvalue(result, 0, 0), 8);
        EXPECT_EQ(static_cast<int64_t>(be64toh(net_value)), test_values[i]);
        PQclear(result);
    }
}

TEST_F(PostgresParamsTest, UnsignedIntegerEdgeCases) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    // uint16 edge cases (stored as int4)
    sink.push(FieldValue{std::uint16_t{0}});
    sink.push(FieldValue{std::uint16_t{65535}});

    // uint32 edge cases (stored as int8)
    sink.push(FieldValue{std::uint32_t{0}});
    sink.push(FieldValue{std::uint32_t{4294967295u}});

    // uint64 edge cases (stored as NUMERIC)
    sink.push(FieldValue{std::uint64_t{0}});
    sink.push(FieldValue{std::uint64_t{18446744073709551615ULL}});

    const auto params = sink.native_packet();

    // Verify uint16 values
    for (size_t i = 0; i < 2; ++i) {
        PGresult* result = PQexecParams(conn_,
                                        "SELECT $1::int4",
                                        1,
                                        &params->oids[i],
                                        &params->values[i],
                                        &params->lengths[i],
                                        &params->formats[i],
                                        1);
        ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
        uint32_t net_value;
        std::memcpy(&net_value, PQgetvalue(result, 0, 0), 4);
        uint32_t expected = (i == 0) ? 0 : 65535;
        EXPECT_EQ(ntohl(net_value), expected);
        PQclear(result);
    }

    // Verify uint32 values
    for (size_t i = 2; i < 4; ++i) {
        PGresult* result = PQexecParams(conn_,
                                        "SELECT $1::int8",
                                        1,
                                        &params->oids[i],
                                        &params->values[i],
                                        &params->lengths[i],
                                        &params->formats[i],
                                        1);
        ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
        uint64_t net_value;
        std::memcpy(&net_value, PQgetvalue(result, 0, 0), 8);
        uint64_t expected = (i == 2) ? 0 : 4294967295u;
        EXPECT_EQ(be64toh(net_value), expected);
        PQclear(result);
    }

    // Verify uint64 values (text format for NUMERIC)
    const std::vector<std::string> expected_uint64 = {"0", "18446744073709551615"};
    for (size_t i = 4; i < 6; ++i) {
        PGresult* result = PQexecParams(conn_,
                                        "SELECT $1::numeric",
                                        1,
                                        &params->oids[i],
                                        &params->values[i],
                                        &params->lengths[i],
                                        &params->formats[i],
                                        0);
        ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
        EXPECT_EQ(std::string(PQgetvalue(result, 0, 0)), expected_uint64[i - 4]);
        PQclear(result);
    }
}

TEST_F(PostgresParamsTest, FloatSpecialValues) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::numeric_limits<float>::infinity()});
    sink.push(FieldValue{-std::numeric_limits<float>::infinity()});
    sink.push(FieldValue{std::numeric_limits<float>::quiet_NaN()});
    sink.push(FieldValue{0.0f});
    sink.push(FieldValue{-0.0f});
    sink.push(FieldValue{std::numeric_limits<float>::min()});
    sink.push(FieldValue{std::numeric_limits<float>::max()});

    const auto params = sink.native_packet();

    // Test infinity
    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::float4",
                                    1,
                                    &params->oids[0],
                                    &params->values[0],
                                    &params->lengths[0],
                                    &params->formats[0],
                                    1);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    uint32_t bits;
    std::memcpy(&bits, PQgetvalue(result, 0, 0), 4);
    bits = be32toh(bits);
    float val;
    std::memcpy(&val, &bits, 4);
    EXPECT_TRUE(std::isinf(val) && val > 0);
    PQclear(result);

    // Test -infinity
    result = PQexecParams(conn_,
                          "SELECT $1::float4",
                          1,
                          &params->oids[1],
                          &params->values[1],
                          &params->lengths[1],
                          &params->formats[1],
                          1);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    std::memcpy(&bits, PQgetvalue(result, 0, 0), 4);
    bits = be32toh(bits);
    std::memcpy(&val, &bits, 4);
    EXPECT_TRUE(std::isinf(val) && val < 0);
    PQclear(result);

    // Test NaN
    result = PQexecParams(conn_,
                          "SELECT $1::float4",
                          1,
                          &params->oids[2],
                          &params->values[2],
                          &params->lengths[2],
                          &params->formats[2],
                          1);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    std::memcpy(&bits, PQgetvalue(result, 0, 0), 4);
    bits = be32toh(bits);
    std::memcpy(&val, &bits, 4);
    EXPECT_TRUE(std::isnan(val));
    PQclear(result);
}

// ============== Comprehensive Tests ==============

TEST_F(PostgresParamsTest, VeryLargeString) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    std::string large_string(1024 * 1024, 'A');
    for (size_t i = 0; i < large_string.size(); i += 100) {
        large_string[i] = static_cast<char>('A' + (i % 26));
    }

    sink.push(FieldValue{large_string});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT length($1::text), $1::text",
                                    static_cast<int>(params->values.size()),
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    0);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), 1024 * 1024);
    EXPECT_EQ(std::string(PQgetvalue(result, 0, 1)), large_string);
    PQclear(result);
}

TEST_F(PostgresParamsTest, UnicodeStrings) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    const std::vector<std::string> unicode_strings = {
        "Hello, 世界",
        "Привет мир",
        "مرحبا بالعالم",
        "🎉🚀💻🌟",
        "Ñoño",
        "Café",
        "日本語テスト",
        "한글 테스트",
        "Ελληνικά",
    };

    for (const auto& str : unicode_strings) {
        sink.push(FieldValue{str});
    }

    const auto params = sink.native_packet();

    for (size_t i = 0; i < unicode_strings.size(); ++i) {
        PGresult* result = PQexecParams(conn_,
                                        "SELECT $1::text",
                                        1,
                                        &params->oids[i],
                                        &params->values[i],
                                        &params->lengths[i],
                                        &params->formats[i],
                                        0);
        ASSERT_NE(result, nullptr);
        ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
        EXPECT_EQ(std::string(PQgetvalue(result, 0, 0)), unicode_strings[i]);
        PQclear(result);
    }
}

TEST_F(PostgresParamsTest, BinaryAllByteValues) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    std::vector<uint8_t> all_bytes;
    all_bytes.reserve(256);
    for (int i = 0; i < 256; ++i) {
        all_bytes.push_back(static_cast<uint8_t>(i));
    }

    sink.push(FieldValue{all_bytes});
    const auto params = sink.native_packet();

    PGresult* result = PQexecParams(conn_,
                                    "SELECT $1::bytea",
                                    1,
                                    params->oids.data(),
                                    params->values.data(),
                                    params->lengths.data(),
                                    params->formats.data(),
                                    1);

    ASSERT_NE(result, nullptr);
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
    const int result_len = PQgetlength(result, 0, 0);
    ASSERT_EQ(result_len, 256);
    const char* result_data = PQgetvalue(result, 0, 0);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(static_cast<uint8_t>(result_data[i]), static_cast<uint8_t>(i));
    }
    PQclear(result);
}

TEST_F(PostgresParamsTest, ManyParameters) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    for (int i = 0; i < 100; ++i) {
        switch (i % 11) {
            case 0:
                sink.push(FieldValue{static_cast<std::int16_t>(i)});
                break;
            case 1:
                sink.push(FieldValue{std::int32_t{i}});
                break;
            case 2:
                sink.push(FieldValue{std::int64_t{i * 1000LL}});
                break;
            case 3:
                sink.push(FieldValue{static_cast<std::uint16_t>(i)});
                break;
            case 4:
                sink.push(FieldValue{static_cast<std::uint32_t>(i)});
                break;
            case 5:
                sink.push(FieldValue{static_cast<std::uint64_t>(i)});
                break;
            case 6:
                sink.push(FieldValue{static_cast<float>(i) * 0.5f});
                break;
            case 7:
                sink.push(FieldValue{static_cast<double>(i) * 0.25});
                break;
            case 8:
                sink.push(FieldValue{std::string{"str"} + std::to_string(i)});
                break;
            case 9:
                sink.push(FieldValue{i % 2 == 0});
                break;
            case 10:
                sink.push(FieldValue{std::monostate{}});
                break;
            default:
                std::unreachable();
        }
    }

    const auto params = sink.native_packet();
    EXPECT_EQ(params->values.size(), 100);

    for (size_t i = 0; i < params->values.size(); ++i) {
        if (i % 11 == 10) {
            EXPECT_EQ(params->values[i], nullptr);
        } else {
            EXPECT_NE(params->values[i], nullptr);
        }
    }
}

TEST_F(PostgresParamsTest, InsertAndSelect) {
    std::pmr::unsynchronized_pool_resource pool;

    PGresult* create_result = PQexec(conn_,
                                     "CREATE TEMP TABLE test_data (id SERIAL PRIMARY KEY, name TEXT, age INT, "
                                     "salary FLOAT8, active BOOL, data BYTEA)");
    ASSERT_EQ(PQresultStatus(create_result), PGRES_COMMAND_OK) << PQerrorMessage(conn_);
    PQclear(create_result);

    postgres::ParamSink insert_sink(&pool);
    std::string name = "John Doe";
    insert_sink.push(FieldValue{name});
    insert_sink.push(FieldValue{std::int32_t{30}});
    insert_sink.push(FieldValue{75000.50});
    insert_sink.push(FieldValue{true});
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
    insert_sink.push(FieldValue{data});

    const auto insert_params = insert_sink.native_packet();

    PGresult* insert_result =
        PQexecParams(conn_,
                     "INSERT INTO test_data (name, age, salary, active, data) VALUES ($1, $2, $3, $4, $5) RETURNING id",
                     static_cast<int>(insert_params->values.size()),
                     insert_params->oids.data(),
                     insert_params->values.data(),
                     insert_params->lengths.data(),
                     insert_params->formats.data(),
                     0);

    ASSERT_EQ(PQresultStatus(insert_result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
    const int id = std::atoi(PQgetvalue(insert_result, 0, 0));
    PQclear(insert_result);

    postgres::ParamSink select_sink(&pool);
    select_sink.push(FieldValue{std::int32_t{id}});
    const auto select_params = select_sink.native_packet();

    PGresult* select_result = PQexecParams(conn_,
                                           "SELECT name, age, salary, active, data FROM test_data WHERE id = $1",
                                           1,
                                           select_params->oids.data(),
                                           select_params->values.data(),
                                           select_params->lengths.data(),
                                           select_params->formats.data(),
                                           1);

    ASSERT_EQ(PQresultStatus(select_result), PGRES_TUPLES_OK) << PQerrorMessage(conn_);
    EXPECT_EQ(PQntuples(select_result), 1);
    EXPECT_EQ(std::string(PQgetvalue(select_result, 0, 0)), name);

    uint32_t age_net;
    std::memcpy(&age_net, PQgetvalue(select_result, 0, 1), 4);
    EXPECT_EQ(static_cast<int32_t>(ntohl(age_net)), 30);

    PQclear(select_result);
}

TEST_F(PostgresParamsTest, VerifyOIDs) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::monostate{}});     // 0: NULL
    sink.push(FieldValue{true});                 // 1: bool
    sink.push(FieldValue{char{'A'}});            // 2: char
    sink.push(FieldValue{std::int16_t{42}});     // 3: int2
    sink.push(FieldValue{std::int32_t{42}});     // 4: int4
    sink.push(FieldValue{std::int64_t{42}});     // 5: int8
    sink.push(FieldValue{std::uint16_t{42}});    // 6: int4 (promoted)
    sink.push(FieldValue{std::uint32_t{42}});    // 7: int8 (promoted)
    sink.push(FieldValue{std::uint64_t{42}});    // 8: numeric
    sink.push(FieldValue{3.14f});                // 9: float4
    sink.push(FieldValue{3.14});                 // 10: float8
    sink.push(FieldValue{std::string{"test"}});  // 11: text
    std::vector<uint8_t> bytes = {1, 2, 3};
    sink.push(FieldValue{bytes});  // 12: bytea

    const auto params = sink.native_packet();

    EXPECT_EQ(params->oids[0], 0);                             // NULL - inferred
    EXPECT_EQ(params->oids[1], OidTypeRegistry::oid_bool);     // 16
    EXPECT_EQ(params->oids[2], OidTypeRegistry::oid_char);     // 18
    EXPECT_EQ(params->oids[3], OidTypeRegistry::oid_int2);     // 21
    EXPECT_EQ(params->oids[4], OidTypeRegistry::oid_int4);     // 23
    EXPECT_EQ(params->oids[5], OidTypeRegistry::oid_int8);     // 20
    EXPECT_EQ(params->oids[6], OidTypeRegistry::oid_int4);     // 23 (uint16 promoted)
    EXPECT_EQ(params->oids[7], OidTypeRegistry::oid_int8);     // 20 (uint32 promoted)
    EXPECT_EQ(params->oids[8], OidTypeRegistry::oid_numeric);  // 1700
    EXPECT_EQ(params->oids[9], OidTypeRegistry::oid_float4);   // 700
    EXPECT_EQ(params->oids[10], OidTypeRegistry::oid_float8);  // 701
    EXPECT_EQ(params->oids[11], OidTypeRegistry::oid_text);    // 25
    EXPECT_EQ(params->oids[12], OidTypeRegistry::oid_bytea);   // 17
}
