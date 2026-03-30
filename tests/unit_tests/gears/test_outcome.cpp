#include <string>
#include <vector>

#include <gears_outcome.hpp>
#include <gtest/gtest.h>

#include "gears_utils.hpp"

using namespace demiplane::gears;

// Test error types
enum class IOError { FileNotFound, PermissionDenied, DiskFull };

enum class NetworkError { Timeout, ConnectionRefused, InvalidResponse };

struct ParseError {
    std::string message;
    int line_number;

    bool operator==(const ParseError& other) const {
        return message == other.message && line_number == other.line_number;
    }
};

// ============================================================================
// Basic Construction Tests
// ============================================================================

TEST(OutcomeTest, DefaultConstructionWithDefaultConstructibleType) {
    Outcome<int, IOError> result;
    EXPECT_TRUE(result.is_success());
    EXPECT_FALSE(result.is_error());
    EXPECT_EQ(result.value(), 0);
}

TEST(OutcomeTest, ConstructionFromValue) {
    Outcome<int, IOError> result(42);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 42);
}

TEST(OutcomeTest, ConstructionFromSuccessTag) {
    Outcome<int, IOError> result = Ok(42);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 42);
}

TEST(OutcomeTest, ConstructionFromErrorTag) {
    Outcome<int, IOError> result = Err(IOError::FileNotFound);
    EXPECT_TRUE(result.is_error());
    EXPECT_FALSE(result.is_success());
    EXPECT_TRUE(result.holds_error<IOError>());
    EXPECT_EQ(result.error<IOError>(), IOError::FileNotFound);
}

TEST(OutcomeTest, ConstructionWithMultipleErrorTypes) {
    Outcome<std::string, IOError, NetworkError> result1 = Err(IOError::DiskFull);
    EXPECT_TRUE(result1.holds_error<IOError>());
    EXPECT_FALSE(result1.holds_error<NetworkError>());

    Outcome<std::string, IOError, NetworkError> result2 = Err(NetworkError::Timeout);
    EXPECT_TRUE(result2.holds_error<NetworkError>());
    EXPECT_FALSE(result2.holds_error<IOError>());
}

TEST(OutcomeTest, SuccessFactoryMethod) {
    auto result = Outcome<int, IOError>::success(100);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 100);
}

TEST(OutcomeTest, ErrorFactoryMethod) {
    auto result = Outcome<int, IOError>::error(IOError::PermissionDenied);
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error<IOError>(), IOError::PermissionDenied);
}

TEST(OutcomeTest, ConstructionWithComplexType) {
    ParseError err{"Invalid syntax", 42};
    Outcome<std::string, ParseError> result = Err((err));
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error<ParseError>(), err);
}

// ============================================================================
// Operator bool and Value Access Tests
// ============================================================================

TEST(OutcomeTest, OperatorBoolSuccess) {
    Outcome<int, IOError> result(42);
    if (result) {
        EXPECT_EQ(*result, 42);
    } else {
        FAIL() << "Expected success";
    }
}

TEST(OutcomeTest, OperatorBoolError) {
    Outcome<int, IOError> result = Err(IOError::FileNotFound);
    if (!result) {
        EXPECT_EQ(result.error<IOError>(), IOError::FileNotFound);
    } else {
        FAIL() << "Expected error";
    }
}

TEST(OutcomeTest, ValueAccessThrowsOnError) {
    Outcome<int, IOError> result = Err(IOError::FileNotFound);
    EXPECT_THROW(unused_value(result.value()), std::bad_variant_access);
}

TEST(OutcomeTest, OperatorDereference) {
    Outcome<int, IOError> result(42);
    EXPECT_EQ(*result, 42);
    *result = 100;
    EXPECT_EQ(*result, 100);
}

TEST(OutcomeTest, OperatorArrow) {
    Outcome<std::string, IOError> result("hello");
    EXPECT_EQ(result->size(), 5);
    EXPECT_EQ(result->length(), 5);
}

TEST(OutcomeTest, ValueOr) {
    Outcome<int, IOError> success(42);
    EXPECT_EQ(success.value_or(100), 42);

    Outcome<int, IOError> error = Err(IOError::FileNotFound);
    EXPECT_EQ(error.value_or(100), 100);
}

TEST(OutcomeTest, MoveSemantics) {
    Outcome<std::string, IOError> result("hello world");
    std::string value = std::move(result).value();
    EXPECT_EQ(value, "hello world");
}

// ============================================================================
// Monadic Operations - and_then
// ============================================================================

TEST(OutcomeTest, AndThenSuccess) {
    Outcome<int, IOError> result(5);
    auto doubled = result.and_then([](int x) { return Outcome<int, IOError>(x * 2); });
    EXPECT_TRUE(doubled.is_success());
    EXPECT_EQ(doubled.value(), 10);
}

TEST(OutcomeTest, AndThenError) {
    Outcome<int, IOError> result = Err(IOError::FileNotFound);
    auto doubled                 = result.and_then([](int x) { return Outcome<int, IOError>(x * 2); });
    EXPECT_TRUE(doubled.is_error());
    EXPECT_EQ(doubled.error<IOError>(), IOError::FileNotFound);
}

TEST(OutcomeTest, AndThenChaining) {
    auto result =
        Outcome<int, IOError>(10).and_then([](int x) { return Outcome<int, IOError>(x + 5); }).and_then([](int x) {
            return Outcome<int, IOError>(x * 2);
        });
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 30);
}

TEST(OutcomeTest, AndThenErrorPropagation) {
    auto result = Outcome<int, IOError>(10)
                      .and_then([]([[maybe_unused]] int x) { return Outcome<int, IOError>::error(IOError::DiskFull); })
                      .and_then([](int x) { return Outcome<int, IOError>(x * 2); });
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error<IOError>(), IOError::DiskFull);
}

// ============================================================================
// Monadic Operations - or_else
// ============================================================================

TEST(OutcomeTest, OrElseSuccess) {
    Outcome<int, IOError> result(42);
    auto recovered = result.or_else([]() { return Outcome<int, IOError>(0); });
    EXPECT_TRUE(recovered.is_success());
    EXPECT_EQ(recovered.value(), 42);
}

TEST(OutcomeTest, OrElseError) {
    Outcome<int, IOError> result = Err(IOError::FileNotFound);
    auto recovered               = result.or_else([]() { return Outcome<int, IOError>(100); });
    EXPECT_TRUE(recovered.is_success());
    EXPECT_EQ(recovered.value(), 100);
}
TEST(OutcomeTest, OrElseErrorOther) {
    Outcome<int, IOError> result = Err(IOError::FileNotFound);
    auto recovered               = result.or_else([]() { return Outcome<int, NetworkError>(100); });
    EXPECT_TRUE(recovered.is_success());
    EXPECT_EQ(recovered.value(), 100);
}
// ============================================================================
// Monadic Operations - transform
// ============================================================================

TEST(OutcomeTest, TransformSuccess) {
    Outcome<int, IOError> result(5);
    auto squared = result.transform([](int x) { return x * x; });
    EXPECT_TRUE(squared.is_success());
    EXPECT_EQ(squared.value(), 25);
}

TEST(OutcomeTest, TransformError) {
    Outcome<int, IOError> result = Err(IOError::FileNotFound);
    auto squared                 = result.transform([](int x) { return x * x; });
    EXPECT_TRUE(squared.is_error());
    EXPECT_EQ(squared.error<IOError>(), IOError::FileNotFound);
}

TEST(OutcomeTest, TransformTypeChange) {
    Outcome<int, IOError> result(42);
    auto str_result = result.transform([](int x) { return std::to_string(x); });
    static_assert(std::is_same_v<decltype(str_result)::value_type, std::string>);
    EXPECT_TRUE(str_result.is_success());
    EXPECT_EQ(str_result.value(), "42");
}

TEST(OutcomeTest, TransformChaining) {
    auto result = Outcome<int, IOError>(10)
                      .transform([](int x) { return x + 5; })
                      .transform([](int x) { return x * 2; })
                      .transform([](int x) { return std::to_string(x); });
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), "30");
}

// ============================================================================
// Visit Pattern Tests
// ============================================================================

TEST(OutcomeTest, VisitSuccess) {
    Outcome<int, IOError, NetworkError> result(42);
    int value = result.visit([](int x) { return x; }, [](IOError) { return -1; }, [](NetworkError) { return -2; });
    EXPECT_EQ(value, 42);
}

TEST(OutcomeTest, VisitIOError) {
    Outcome<int, IOError, NetworkError> result = Err(IOError::FileNotFound);
    int value                                  = result.visit([]([[maybe_unused]] int x) { return 0; },
                             [](IOError err) { return static_cast<int>(err) + 10; },
                             [](NetworkError) { return -1; });
    EXPECT_EQ(value, 10);
}

TEST(OutcomeTest, VisitNetworkError) {
    Outcome<int, IOError, NetworkError> result = Err(NetworkError::Timeout);
    int value                                  = result.visit([]([[maybe_unused]] int x) { return 0; },
                             [](IOError) { return -1; },
                             [](NetworkError err) { return static_cast<int>(err) + 20; });
    EXPECT_EQ(value, 20);
}

// ============================================================================
// Void Specialization Tests
// ============================================================================

TEST(OutcomeVoidTest, DefaultConstruction) {
    Outcome<void, IOError> result;
    EXPECT_TRUE(result.is_success());
    EXPECT_FALSE(result.is_error());
}

TEST(OutcomeVoidTest, ConstructionFromMonostate) {
    Outcome<void, IOError> result = Ok();
    EXPECT_TRUE(result.is_success());
}

TEST(OutcomeVoidTest, ConstructionFromError) {
    Outcome<void, IOError> result = Err(IOError::PermissionDenied);
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error<IOError>(), IOError::PermissionDenied);
}

TEST(OutcomeVoidTest, SuccessFactory) {
    auto result = Outcome<void, IOError>::success();
    EXPECT_TRUE(result.is_success());
}

TEST(OutcomeVoidTest, ErrorFactory) {
    auto result = Outcome<void, IOError>::error(IOError::DiskFull);
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error<IOError>(), IOError::DiskFull);
}

TEST(OutcomeVoidTest, OperatorBool) {
    Outcome<void, IOError> success;
    EXPECT_TRUE(static_cast<bool>(success));

    Outcome<void, IOError> error = Err(IOError::FileNotFound);
    EXPECT_FALSE(static_cast<bool>(error));
}

TEST(OutcomeVoidTest, EnsureSuccess) {
    Outcome<void, IOError> success;
    EXPECT_NO_THROW(success.ensure_success());

    Outcome<void, IOError> error = Err(IOError::FileNotFound);
    EXPECT_THROW(error.ensure_success(), std::bad_variant_access);
}

TEST(OutcomeVoidTest, AndThen) {
    auto result = Outcome<void, IOError>().and_then([]() { return Outcome<int, IOError>(42); });
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 42);

    auto error_result =
        Outcome<void, IOError>::error(IOError::FileNotFound).and_then([]() { return Outcome<int, IOError>(42); });
    EXPECT_TRUE(error_result.is_error());
    EXPECT_EQ(error_result.error<IOError>(), IOError::FileNotFound);
}

TEST(OutcomeVoidTest, OrElse) {
    auto success = Outcome<void, IOError>().or_else([]() { return Outcome<void, IOError>::error(IOError::DiskFull); });
    EXPECT_TRUE(success.is_success());

    auto recovered =
        Outcome<void, IOError>::error(IOError::FileNotFound).or_else([]() { return Outcome<void, IOError>(); });
    EXPECT_TRUE(recovered.is_success());
}

TEST(OutcomeVoidTest, Transform) {
    auto result = Outcome<void, IOError>().transform([]() { return 42; });
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 42);

    auto error_result = Outcome<void, IOError>::error(IOError::FileNotFound).transform([]() { return 42; });
    EXPECT_TRUE(error_result.is_error());
}

TEST(OutcomeVoidTest, Visit) {
    Outcome<void, IOError, NetworkError> success;
    int value =
        success.visit([](std::monostate) { return 0; }, [](IOError) { return 1; }, [](NetworkError) { return 2; });
    EXPECT_EQ(value, 0);

    Outcome<void, IOError, NetworkError> error = Err(IOError::DiskFull);
    value                                      = error.visit([](std::monostate) { return 0; },
                        [](IOError err) { return static_cast<int>(err) + 10; },
                        [](NetworkError) { return 2; });
    EXPECT_EQ(value, 12);
}

// ============================================================================
// Combine Outcomes Tests
// ============================================================================

TEST(CombineOutcomesTest, AllSuccessNonVoid) {
    //?
}

TEST(CombineOutcomesTest, AllSuccessVoid) {
    Outcome<void, IOError> r1;
    Outcome<void, IOError> r2;
    Outcome<void, IOError> r3;

    auto combined = combine_outcomes(r1, r2, r3);
    EXPECT_TRUE(combined.is_success());
}

TEST(CombineOutcomesTest, FirstError) {
    Outcome<int, IOError> r1 = Err(IOError::FileNotFound);
    Outcome<int, IOError> r2(20);
    Outcome<int, IOError> r3(30);

    auto combined = combine_outcomes(r1, r2, r3);
    EXPECT_TRUE(combined.is_error());
}

TEST(CombineOutcomesTest, MiddleError) {
    Outcome<int, IOError> r1(10);
    Outcome<int, IOError> r2 = Err(IOError::PermissionDenied);
    Outcome<int, IOError> r3(30);

    auto combined = combine_outcomes(r1, r2, r3);
    EXPECT_TRUE(combined.is_error());
}

TEST(CombineOutcomesTest, MixedVoidAndNonVoid) {
    Outcome<void, IOError> r1;
    Outcome<int, IOError> r2(42);
    Outcome<std::string, IOError> r3("hello");

    auto combined = combine_outcomes(r1, r2, r3);
    EXPECT_TRUE(combined.is_success());
    EXPECT_EQ(std::get<0>(combined.value()), 42);
    EXPECT_EQ(std::get<1>(combined.value()), "hello");
}

// ============================================================================
// Real-World Usage Examples
// ============================================================================

// File reading example
Outcome<std::string, IOError> read_file(const std::string& path) {
    if (path.empty()) {
        return Err(IOError::FileNotFound);
    }
    if (path == "forbidden") {
        return Err(IOError::PermissionDenied);
    }
    return Ok(std::string("file contents"));
}

TEST(RealWorldTest, FileReadingSuccess) {
    auto result = read_file("test.txt");
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), "file contents");
}

TEST(RealWorldTest, FileReadingError) {
    auto result = read_file("");
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error<IOError>(), IOError::FileNotFound);
}

TEST(RealWorldTest, FileReadingChain) {
    auto result = read_file("test.txt")
                      .transform([](const std::string& content) { return content.size(); })
                      .and_then([](size_t size) { return Outcome<int, IOError>(static_cast<int>(size) * 2); });
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 26);  // "file contents" = 13 chars * 2
}

// Network request example
Outcome<std::string, NetworkError, ParseError> fetch_and_parse(const std::string& url) {
    if (url.empty()) {
        return Err(NetworkError::InvalidResponse);
    }
    if (url == "timeout") {
        return Err(NetworkError::Timeout);
    }
    if (url == "invalid_json") {
        return Err(ParseError{"Invalid JSON", 1});
    }
    return "parsed data";
}

TEST(RealWorldTest, NetworkRequestSuccess) {
    auto result = fetch_and_parse("https://api.example.com");
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), "parsed data");
}

TEST(RealWorldTest, NetworkRequestTimeout) {
    auto result = fetch_and_parse("timeout");
    EXPECT_TRUE(result.is_error());
    EXPECT_TRUE(result.holds_error<NetworkError>());
    EXPECT_EQ(result.error<NetworkError>(), NetworkError::Timeout);
}

TEST(RealWorldTest, NetworkRequestParseError) {
    auto result = fetch_and_parse("invalid_json");
    EXPECT_TRUE(result.is_error());
    EXPECT_TRUE(result.holds_error<ParseError>());
    EXPECT_EQ(result.error<ParseError>().line_number, 1);
}

// ============================================================================
// Edge Cases and Special Scenarios
// ============================================================================

TEST(EdgeCaseTest, MoveOnlyType) {
    Outcome<std::unique_ptr<int>, IOError> result = Ok(std::make_unique<int>(42));
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(*result.value(), 42);

    auto value = std::move(result).value();
    EXPECT_EQ(*value, 42);
}

TEST(EdgeCaseTest, LargeValueType) {
    std::vector<int> large_vec(1000, 42);
    Outcome<std::vector<int>, IOError> result = Ok(large_vec);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value().size(), 1000);
}

TEST(EdgeCaseTest, ConstCorrectness) {
    const Outcome<int, IOError> result(42);
    EXPECT_EQ(result.value(), 42);
    EXPECT_EQ(*result, 42);

    const Outcome<int, IOError> error = Err(IOError::FileNotFound);
    EXPECT_EQ(error.error<IOError>(), IOError::FileNotFound);
}
