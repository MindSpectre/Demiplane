// PostgreSQL FieldView Decoder Integration Tests
// Tests round-trip decoding of all supported types through real PostgreSQL
// Covers both TEXT format (via PQexec) and BINARY format (via PQexecParams)

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <test_fixture.hpp>

using namespace demiplane::test;
using namespace demiplane::db::postgres;

class FieldViewDecoderTest : public PgsqlTestFixture, virtual ::testing::Test {};

// ============== Boolean ==============

TEST_F(FieldViewDecoderTest, BoolTrueTextFormat) {
    auto result = executor().execute("SELECT true AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(result.value().get<bool>(0, 0));
}

TEST_F(FieldViewDecoderTest, BoolFalseTextFormat) {
    auto result = executor().execute("SELECT false AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_FALSE(result.value().get<bool>(0, 0));
}

TEST_F(FieldViewDecoderTest, BoolTrueBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", true);
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(result.value().get<bool>(0, 0));
}

TEST_F(FieldViewDecoderTest, BoolFalseBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", false);
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_FALSE(result.value().get<bool>(0, 0));
}

// ============== Int16 (SMALLINT) ==============

TEST_F(FieldViewDecoderTest, Int16TextFormat) {
    auto result = executor().execute("SELECT 42::int2 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int16_t>(0, 0), 42);
}

TEST_F(FieldViewDecoderTest, Int16NegativeTextFormat) {
    auto result = executor().execute("SELECT (-1)::int2 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int16_t>(0, 0), -1);
}

TEST_F(FieldViewDecoderTest, Int16MaxTextFormat) {
    auto result = executor().execute("SELECT 32767::int2 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int16_t>(0, 0), std::numeric_limits<int16_t>::max());
}

TEST_F(FieldViewDecoderTest, Int16MinTextFormat) {
    auto result = executor().execute("SELECT (-32768)::int2 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int16_t>(0, 0), std::numeric_limits<int16_t>::min());
}

TEST_F(FieldViewDecoderTest, Int16BinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", int16_t{42});
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int16_t>(0, 0), 42);
}

TEST_F(FieldViewDecoderTest, Int16NegativeBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", int16_t{-32768});
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int16_t>(0, 0), std::numeric_limits<int16_t>::min());
}

// ============== Int32 (INTEGER) ==============

TEST_F(FieldViewDecoderTest, Int32TextFormat) {
    auto result = executor().execute("SELECT 2147483647::int4 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int32_t>(0, 0), std::numeric_limits<int32_t>::max());
}

TEST_F(FieldViewDecoderTest, Int32MinTextFormat) {
    auto result = executor().execute("SELECT (-2147483648)::int4 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int32_t>(0, 0), std::numeric_limits<int32_t>::min());
}

TEST_F(FieldViewDecoderTest, Int32ZeroTextFormat) {
    auto result = executor().execute("SELECT 0::int4 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int32_t>(0, 0), 0);
}

TEST_F(FieldViewDecoderTest, Int32BinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", int32_t{2147483647});
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int32_t>(0, 0), std::numeric_limits<int32_t>::max());
}

TEST_F(FieldViewDecoderTest, Int32NegativeBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", int32_t{-2147483648});
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int32_t>(0, 0), std::numeric_limits<int32_t>::min());
}

// ============== Int64 (BIGINT) ==============

TEST_F(FieldViewDecoderTest, Int64TextFormat) {
    auto result = executor().execute("SELECT 9223372036854775807::int8 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int64_t>(0, 0), std::numeric_limits<int64_t>::max());
}

TEST_F(FieldViewDecoderTest, Int64ZeroTextFormat) {
    auto result = executor().execute("SELECT 0::int8 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int64_t>(0, 0), 0);
}

TEST_F(FieldViewDecoderTest, Int64BinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", int64_t{9223372036854775807});
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int64_t>(0, 0), std::numeric_limits<int64_t>::max());
}

TEST_F(FieldViewDecoderTest, Int64NegativeBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", int64_t{-9223372036854775807 - 1});
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int64_t>(0, 0), std::numeric_limits<int64_t>::min());
}

// ============== Float (REAL / float4) ==============

TEST_F(FieldViewDecoderTest, FloatTextFormat) {
    auto result = executor().execute("SELECT 3.14::real AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_FLOAT_EQ(result.value().get<float>(0, 0), 3.14f);
}

TEST_F(FieldViewDecoderTest, FloatNegativeTextFormat) {
    auto result = executor().execute("SELECT (-1.5)::real AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_FLOAT_EQ(result.value().get<float>(0, 0), -1.5f);
}

TEST_F(FieldViewDecoderTest, FloatNaNTextFormat) {
    auto result = executor().execute("SELECT 'NaN'::real AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(std::isnan(result.value().get<float>(0, 0)));
}

TEST_F(FieldViewDecoderTest, FloatInfinityTextFormat) {
    auto result = executor().execute("SELECT 'Infinity'::real AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(std::isinf(result.value().get<float>(0, 0)));
    EXPECT_GT(result.value().get<float>(0, 0), 0.0f);
}

TEST_F(FieldViewDecoderTest, FloatNegativeInfinityTextFormat) {
    auto result = executor().execute("SELECT '-Infinity'::real AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(std::isinf(result.value().get<float>(0, 0)));
    EXPECT_LT(result.value().get<float>(0, 0), 0.0f);
}

TEST_F(FieldViewDecoderTest, FloatBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", 3.14f);
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_FLOAT_EQ(result.value().get<float>(0, 0), 3.14f);
}

TEST_F(FieldViewDecoderTest, FloatNaNBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", std::numeric_limits<float>::quiet_NaN());
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(std::isnan(result.value().get<float>(0, 0)));
}

TEST_F(FieldViewDecoderTest, FloatInfinityBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", std::numeric_limits<float>::infinity());
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(std::isinf(result.value().get<float>(0, 0)));
    EXPECT_GT(result.value().get<float>(0, 0), 0.0f);
}

// ============== Double (DOUBLE PRECISION / float8) ==============

TEST_F(FieldViewDecoderTest, DoubleTextFormat) {
    auto result = executor().execute("SELECT 2.718281828::float8 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_DOUBLE_EQ(result.value().get<double>(0, 0), 2.718281828);
}

TEST_F(FieldViewDecoderTest, DoubleNaNTextFormat) {
    auto result = executor().execute("SELECT 'NaN'::float8 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(std::isnan(result.value().get<double>(0, 0)));
}

TEST_F(FieldViewDecoderTest, DoubleInfinityTextFormat) {
    auto result = executor().execute("SELECT 'Infinity'::float8 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(std::isinf(result.value().get<double>(0, 0)));
    EXPECT_GT(result.value().get<double>(0, 0), 0.0);
}

TEST_F(FieldViewDecoderTest, DoubleNegativeInfinityTextFormat) {
    auto result = executor().execute("SELECT '-Infinity'::float8 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(std::isinf(result.value().get<double>(0, 0)));
    EXPECT_LT(result.value().get<double>(0, 0), 0.0);
}

TEST_F(FieldViewDecoderTest, DoubleBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", 2.718281828);
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_DOUBLE_EQ(result.value().get<double>(0, 0), 2.718281828);
}

TEST_F(FieldViewDecoderTest, DoubleNaNBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", std::numeric_limits<double>::quiet_NaN());
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(std::isnan(result.value().get<double>(0, 0)));
}

TEST_F(FieldViewDecoderTest, DoubleNegativeInfinityBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", -std::numeric_limits<double>::infinity());
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_TRUE(std::isinf(result.value().get<double>(0, 0)));
    EXPECT_LT(result.value().get<double>(0, 0), 0.0);
}

// ============== String (TEXT) ==============

TEST_F(FieldViewDecoderTest, StringTextFormat) {
    auto result = executor().execute("SELECT 'hello world'::text AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<std::string>(0, 0), "hello world");
}

TEST_F(FieldViewDecoderTest, StringEmptyTextFormat) {
    auto result = executor().execute("SELECT ''::text AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<std::string>(0, 0), "");
}

TEST_F(FieldViewDecoderTest, StringBinaryFormat) {
    auto result = executor().execute("SELECT $1 AS val", std::string{"hello binary"});
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<std::string>(0, 0), "hello binary");
}

TEST_F(FieldViewDecoderTest, StringViewTextFormat) {
    auto result = executor().execute("SELECT 'view test'::text AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<std::string_view>(0, 0), "view test");
}

TEST_F(FieldViewDecoderTest, StringUnicodeTextFormat) {
    auto result = executor().execute("SELECT '\xc3\xa9\xc3\xa8\xc3\xaa'::text AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<std::string>(0, 0), "\xc3\xa9\xc3\xa8\xc3\xaa");
}

// ============== Bytea (Binary Data) ==============

TEST_F(FieldViewDecoderTest, ByteaHexTextFormat) {
    auto result = executor().execute("SELECT decode('DEADBEEF', 'hex')::bytea AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    auto bytes = result.value().get<std::vector<uint8_t>>(0, 0);
    ASSERT_EQ(bytes.size(), 4);
    EXPECT_EQ(bytes[0], 0xDE);
    EXPECT_EQ(bytes[1], 0xAD);
    EXPECT_EQ(bytes[2], 0xBE);
    EXPECT_EQ(bytes[3], 0xEF);
}

TEST_F(FieldViewDecoderTest, ByteaBinaryFormat) {
    std::vector<uint8_t> input{0x00, 0x01, 0xFF, 0xAB};
    auto result = executor().execute("SELECT $1 AS val", input);
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    auto bytes = result.value().get<std::vector<uint8_t>>(0, 0);
    EXPECT_EQ(bytes, input);
}

TEST_F(FieldViewDecoderTest, ByteaEmptyTextFormat) {
    auto result = executor().execute("SELECT ''::bytea AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    auto bytes = result.value().get<std::vector<uint8_t>>(0, 0);
    EXPECT_TRUE(bytes.empty());
}

// ============== NULL Handling ==============

TEST_F(FieldViewDecoderTest, NullIntReturnsNulloptFromGet) {
    auto result = executor().execute("SELECT NULL::int4 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    auto row = result.value().get_row(0);
    auto fv  = row.at(0);
    EXPECT_TRUE(fv.is_null());
    EXPECT_EQ(fv.get<int32_t>(), std::nullopt);
}

TEST_F(FieldViewDecoderTest, NullStringReturnsNulloptFromGet) {
    auto result = executor().execute("SELECT NULL::text AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    auto row = result.value().get_row(0);
    auto fv  = row.at(0);
    EXPECT_TRUE(fv.is_null());
    EXPECT_EQ(fv.get<std::string>(), std::nullopt);
}

TEST_F(FieldViewDecoderTest, NullMonostateSucceeds) {
    auto result = executor().execute("SELECT NULL::int4 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    auto row = result.value().get_row(0);
    auto fv  = row.at(0);
    EXPECT_TRUE(fv.is_null());
    EXPECT_NO_THROW((void)fv.as<std::monostate>());
}

TEST_F(FieldViewDecoderTest, NullAsIntThrows) {
    auto result = executor().execute("SELECT NULL::int4 AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    auto row = result.value().get_row(0);
    auto fv  = row.at(0);
    EXPECT_THROW((void)fv.as<int32_t>(), demiplane::db::null_conversion_error);
}

TEST_F(FieldViewDecoderTest, NullBoolReturnsNulloptFromGet) {
    auto result = executor().execute("SELECT NULL::bool AS val");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    auto row = result.value().get_row(0);
    auto fv  = row.at(0);
    EXPECT_TRUE(fv.is_null());
    EXPECT_EQ(fv.get<bool>(), std::nullopt);
}

// ============== Multiple Types in One Row ==============

TEST_F(FieldViewDecoderTest, MixedTypesTextFormat) {
    auto result = executor().execute("SELECT true AS b, 42::int4 AS i, 3.14::float8 AS d, 'hello'::text AS s");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    const auto& block = result.value();
    EXPECT_EQ(block.cols(), 4);
    EXPECT_TRUE(block.get<bool>(0, 0));
    EXPECT_EQ(block.get<int32_t>(0, 1), 42);
    EXPECT_DOUBLE_EQ(block.get<double>(0, 2), 3.14);
    EXPECT_EQ(block.get<std::string>(0, 3), "hello");
}

TEST_F(FieldViewDecoderTest, MixedTypesBinaryFormat) {
    auto result =
        executor().execute("SELECT $1 AS b, $2 AS i, $3 AS d, $4 AS s", true, int32_t{42}, 3.14, std::string{"hello"});
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    const auto& block = result.value();
    EXPECT_EQ(block.cols(), 4);
    EXPECT_TRUE(block.get<bool>(0, 0));
    EXPECT_EQ(block.get<int32_t>(0, 1), 42);
    EXPECT_DOUBLE_EQ(block.get<double>(0, 2), 3.14);
    EXPECT_EQ(block.get<std::string>(0, 3), "hello");
}

// ============== Multiple Rows ==============

TEST_F(FieldViewDecoderTest, MultipleRowsTextFormat) {
    auto result = executor().execute("SELECT * FROM (VALUES (1, 'Alice'), (2, 'Bob'), (3, 'Charlie')) AS t(id, name)");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    const auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);
    EXPECT_EQ(block.get<int32_t>(0, 0), 1);
    EXPECT_EQ(block.get<std::string>(0, 1), "Alice");
    EXPECT_EQ(block.get<int32_t>(1, 0), 2);
    EXPECT_EQ(block.get<std::string>(1, 1), "Bob");
    EXPECT_EQ(block.get<int32_t>(2, 0), 3);
    EXPECT_EQ(block.get<std::string>(2, 1), "Charlie");
}

// ============== Table Round-Trip (Insert + Select) ==============

TEST_F(FieldViewDecoderTest, RoundTripAllTypesViaTable) {
    // Create a table with diverse types
    auto create = executor().execute(R"(
        CREATE TEMPORARY TABLE test_decode (
            col_bool    BOOLEAN,
            col_int2    SMALLINT,
            col_int4    INTEGER,
            col_int8    BIGINT,
            col_float4  REAL,
            col_float8  DOUBLE PRECISION,
            col_text    TEXT,
            col_bytea   BYTEA
        )
    )");
    ASSERT_TRUE(create.is_success()) << create.error<ErrorContext>().format();

    // Insert via parameterized query (binary encoding)
    auto insert = executor().execute("INSERT INTO test_decode VALUES ($1, $2, $3, $4, $5, $6, $7, $8)",
                                     true,
                                     int16_t{-100},
                                     int32_t{999999},
                                     int64_t{1234567890123LL},
                                     1.5f,
                                     2.5,
                                     std::string{"test string"},
                                     std::vector<uint8_t>{0xCA, 0xFE});
    ASSERT_TRUE(insert.is_success()) << insert.error<ErrorContext>().format();

    // Select via raw SQL (text format results)
    auto text_result = executor().execute("SELECT * FROM test_decode");
    ASSERT_TRUE(text_result.is_success()) << text_result.error<ErrorContext>().format();

    const auto& tr = text_result.value();
    EXPECT_TRUE(tr.get<bool>(0, 0));
    EXPECT_EQ(tr.get<int16_t>(0, 1), -100);
    EXPECT_EQ(tr.get<int32_t>(0, 2), 999999);
    EXPECT_EQ(tr.get<int64_t>(0, 3), 1234567890123LL);
    EXPECT_FLOAT_EQ(tr.get<float>(0, 4), 1.5f);
    EXPECT_DOUBLE_EQ(tr.get<double>(0, 5), 2.5);
    EXPECT_EQ(tr.get<std::string>(0, 6), "test string");

    auto bytea = tr.get<std::vector<uint8_t>>(0, 7);
    ASSERT_EQ(bytea.size(), 2);
    EXPECT_EQ(bytea[0], 0xCA);
    EXPECT_EQ(bytea[1], 0xFE);

    // Select via parameterized query (binary format results)
    auto bin_result = executor().execute("SELECT * FROM test_decode WHERE $1", true);
    ASSERT_TRUE(bin_result.is_success()) << bin_result.error<ErrorContext>().format();

    const auto& br = bin_result.value();
    EXPECT_TRUE(br.get<bool>(0, 0));
    EXPECT_EQ(br.get<int16_t>(0, 1), -100);
    EXPECT_EQ(br.get<int32_t>(0, 2), 999999);
    EXPECT_EQ(br.get<int64_t>(0, 3), 1234567890123LL);
    EXPECT_FLOAT_EQ(br.get<float>(0, 4), 1.5f);
    EXPECT_DOUBLE_EQ(br.get<double>(0, 5), 2.5);
    EXPECT_EQ(br.get<std::string>(0, 6), "test string");

    auto bytea_bin = br.get<std::vector<uint8_t>>(0, 7);
    ASSERT_EQ(bytea_bin.size(), 2);
    EXPECT_EQ(bytea_bin[0], 0xCA);
    EXPECT_EQ(bytea_bin[1], 0xFE);
}
