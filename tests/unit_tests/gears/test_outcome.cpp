#include <string>
#include <vector>

#include <gears_outcome.hpp>
#include <gtest/gtest.h>

#include "gears_utils.hpp"

using namespace demiplane::gears;

// Test error types
enum class IOError { FileNotFound, PermissionDenied, DiskFull };

enum class NetworkError { Timeout, InvalidResponse };

struct ParseError {
    std::string message;
    int line_number;

    bool operator==(const ParseError& other) const {
        return message == other.message && line_number == other.line_number;
    }
};

// Value type with no default constructor — proves Outcome's monadic ops and
// factories never internally default-construct T.
struct NoDefault {
    int x;
    constexpr explicit NoDefault(int v) noexcept
        : x(v) {
    }
    NoDefault()                                       = delete;
    constexpr NoDefault(const NoDefault&)             = default;
    constexpr NoDefault(NoDefault&&)                  = default;
    constexpr NoDefault& operator=(const NoDefault&)  = default;
    constexpr NoDefault& operator=(NoDefault&&)       = default;
    constexpr bool operator==(const NoDefault&) const = default;
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
    Outcome<int, IOError> result = ok(42);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 42);
}

TEST(OutcomeTest, ConstructionFromErrorTag) {
    Outcome<int, IOError> result = err(IOError::FileNotFound);
    EXPECT_TRUE(result.is_error());
    EXPECT_FALSE(result.is_success());
    EXPECT_TRUE(result.holds_error<IOError>());
    EXPECT_EQ(result.error<IOError>(), IOError::FileNotFound);
}

TEST(OutcomeTest, ConstructionWithMultipleErrorTypes) {
    Outcome<std::string, IOError, NetworkError> result1 = err(IOError::DiskFull);
    EXPECT_TRUE(result1.holds_error<IOError>());
    EXPECT_FALSE(result1.holds_error<NetworkError>());

    Outcome<std::string, IOError, NetworkError> result2 = err(NetworkError::Timeout);
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
    ParseError parse_err{"Invalid syntax", 42};
    Outcome<std::string, ParseError> result = err(parse_err);
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error<ParseError>(), parse_err);
}

// ============================================================================
// Operator bool and Value Access Tests
// ============================================================================

TEST(OutcomeTest, OperatorBoolSuccess) {
    if (Outcome<int, IOError> result(42); result) {
        EXPECT_EQ(*result, 42);
    } else {
        FAIL() << "Expected success";
    }
}

TEST(OutcomeTest, OperatorBoolError) {
    if (Outcome<int, IOError> result = err(IOError::FileNotFound); !result) {
        EXPECT_EQ(result.error<IOError>(), IOError::FileNotFound);
    } else {
        FAIL() << "Expected error";
    }
}

TEST(OutcomeTest, ValueAccessThrowsOnError) {
    Outcome<int, IOError> result = err(IOError::FileNotFound);
    EXPECT_THROW(unused_value(result.value()), BadOutcomeAccess);
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
    constexpr Outcome<int, IOError> success(42);
    EXPECT_EQ(success.value_or(100), 42);

    constexpr Outcome<int, IOError> error = err(IOError::FileNotFound);
    EXPECT_EQ(error.value_or(100), 100);
}

TEST(OutcomeTest, MoveSemantics) {
    Outcome<std::string, IOError> result("hello world");
    const std::string value = std::move(result).value();
    EXPECT_EQ(value, "hello world");
}

// ============================================================================
// Monadic Operations - and_then
// ============================================================================

TEST(OutcomeTest, AndThenSuccess) {
    Outcome<int, IOError> result(5);
    auto doubled = result.and_then([](const int x) { return Outcome<int, IOError>(x * 2); });
    EXPECT_TRUE(doubled.is_success());
    EXPECT_EQ(doubled.value(), 10);
}

TEST(OutcomeTest, AndThenError) {
    Outcome<int, IOError> result = err(IOError::FileNotFound);
    auto doubled                 = result.and_then([](const int x) { return Outcome<int, IOError>(x * 2); });
    EXPECT_TRUE(doubled.is_error());
    EXPECT_EQ(doubled.error<IOError>(), IOError::FileNotFound);
}

TEST(OutcomeTest, AndThenChaining) {
    auto result = Outcome<int, IOError>(10)
                      .and_then([](const int x) { return Outcome<int, IOError>(x + 5); })
                      .and_then([](const int x) { return Outcome<int, IOError>(x * 2); });
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 30);
}

TEST(OutcomeTest, AndThenErrorPropagation) {
    auto result = Outcome<int, IOError>(10)
                      .and_then([]([[maybe_unused]] int x) { return Outcome<int, IOError>::error(IOError::DiskFull); })
                      .and_then([](const int x) { return Outcome<int, IOError>(x * 2); });
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
    Outcome<int, IOError> result = err(IOError::FileNotFound);
    auto recovered               = result.or_else([]() { return Outcome<int, IOError>(100); });
    EXPECT_TRUE(recovered.is_success());
    EXPECT_EQ(recovered.value(), 100);
}
TEST(OutcomeTest, OrElseErrorOther) {
    Outcome<int, IOError> result = err(IOError::FileNotFound);
    auto recovered               = result.or_else([]() { return Outcome<int, NetworkError>(100); });
    EXPECT_TRUE(recovered.is_success());
    EXPECT_EQ(recovered.value(), 100);
}
// ============================================================================
// Monadic Operations - transform
// ============================================================================

TEST(OutcomeTest, TransformSuccess) {
    const std::string y = "123";
    Outcome<int, IOError> result(5);
    Outcome<std::string, IOError> v = y;
    auto squared                    = result.transform([](const int x) { return x * x; });
    EXPECT_TRUE(squared.is_success());
    EXPECT_EQ(squared.value(), 25);
}

TEST(OutcomeTest, TransformError) {
    Outcome<int, IOError> result = err(IOError::FileNotFound);
    auto squared                 = result.transform([](const int x) { return x * x; });
    EXPECT_TRUE(squared.is_error());
    EXPECT_EQ(squared.error<IOError>(), IOError::FileNotFound);
}

TEST(OutcomeTest, TransformTypeChange) {
    Outcome<int, IOError> result(42);
    auto str_result = result.transform([](const int x) { return std::to_string(x); });
    static_assert(std::is_same_v<decltype(str_result)::value_type, std::string>);
    EXPECT_TRUE(str_result.is_success());
    EXPECT_EQ(str_result.value(), "42");
}

TEST(OutcomeTest, TransformChaining) {
    auto result = Outcome<int, IOError>(10)
                      .transform([](const int x) { return x + 5; })
                      .transform([](const int x) { return x * 2; })
                      .transform([](const int x) { return std::to_string(x); });
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), "30");
}

TEST(OutcomeTest, TransformVoidReturnFromNonVoidSource) {
    // Regression for fix #2: transform on a non-void Outcome must accept a
    // void-returning callable, producing Outcome<void, Errors...>.
    int observed = 0;
    auto result  = Outcome<int, IOError>(7).transform([&](const int x) { observed = x; });
    static_assert(std::is_same_v<decltype(result)::value_type, void>);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(observed, 7);

    auto error_result = Outcome<int, IOError>::error(IOError::DiskFull).transform([&](int) { observed = -1; });
    EXPECT_TRUE(error_result.is_error());
    EXPECT_EQ(error_result.error<IOError>(), IOError::DiskFull);
    EXPECT_EQ(observed, 7);  // callable not invoked on error path
}

// ============================================================================
// Visit Pattern Tests
// ============================================================================

TEST(OutcomeTest, VisitSuccess) {
    Outcome<int, IOError, NetworkError> result(42);
    const int value =
        result.visit([](const int x) { return x; }, [](IOError) { return -1; }, [](NetworkError) { return -2; });
    EXPECT_EQ(value, 42);
}

TEST(OutcomeTest, VisitIOError) {
    Outcome<int, IOError, NetworkError> result = err(IOError::FileNotFound);
    const int value                            = result.visit([]([[maybe_unused]] int x) { return 0; },
                                   [](IOError err) { return static_cast<int>(err) + 10; },
                                   [](NetworkError) { return -1; });
    EXPECT_EQ(value, 10);
}

TEST(OutcomeTest, VisitNetworkError) {
    Outcome<int, IOError, NetworkError> result = err(NetworkError::Timeout);
    const int value                            = result.visit([]([[maybe_unused]] int x) { return 0; },
                                   [](IOError) { return -1; },
                                   [](NetworkError err) { return static_cast<int>(err) + 20; });
    EXPECT_EQ(value, 20);
}

// ============================================================================
// Void Specialization Tests
// ============================================================================

TEST(OutcomeVoidTest, DefaultConstruction) {
    constexpr Outcome<void, IOError> result;
    EXPECT_TRUE(result.is_success());
    EXPECT_FALSE(result.is_error());
}

TEST(OutcomeVoidTest, ConstructionFromMonostate) {
    constexpr Outcome<void, IOError> result = ok();
    EXPECT_TRUE(result.is_success());
}

TEST(OutcomeVoidTest, ConstructionFromError) {
    Outcome<void, IOError> result = err(IOError::PermissionDenied);
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error<IOError>(), IOError::PermissionDenied);
}

TEST(OutcomeVoidTest, SuccessFactory) {
    constexpr auto result = Outcome<void, IOError>::success();
    EXPECT_TRUE(result.is_success());
}

TEST(OutcomeVoidTest, ErrorFactory) {
    constexpr auto result = Outcome<void, IOError>::error(IOError::DiskFull);
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error<IOError>(), IOError::DiskFull);
}

TEST(OutcomeVoidTest, OperatorBool) {
    constexpr Outcome<void, IOError> success;
    EXPECT_TRUE(static_cast<bool>(success));

    constexpr Outcome<void, IOError> error = err(IOError::FileNotFound);
    EXPECT_FALSE(static_cast<bool>(error));
}

TEST(OutcomeVoidTest, EnsureSuccess) {
    Outcome<void, IOError> success;
    EXPECT_NO_THROW(success.ensure_success());

    Outcome<void, IOError> error = err(IOError::FileNotFound);
    EXPECT_THROW(error.ensure_success(), BadOutcomeAccess);
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
    constexpr auto success =
        Outcome<void, IOError>().or_else([]() { return Outcome<void, IOError>::error(IOError::DiskFull); });
    EXPECT_TRUE(success.is_success());

    constexpr auto recovered =
        Outcome<void, IOError>::error(IOError::FileNotFound).or_else([]() { return Outcome<void, IOError>(); });
    EXPECT_TRUE(recovered.is_success());
}

TEST(OutcomeVoidTest, Transform) {
    auto result = Outcome<void, IOError>().transform([]() { return 42; });
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 42);

    constexpr auto error_result = Outcome<void, IOError>::error(IOError::FileNotFound).transform([]() { return 42; });
    EXPECT_TRUE(error_result.is_error());
}

TEST(OutcomeVoidTest, Visit) {
    Outcome<void, IOError, NetworkError> success;
    int value =
        success.visit([](std::monostate) { return 0; }, [](IOError) { return 1; }, [](NetworkError) { return 2; });
    EXPECT_EQ(value, 0);

    Outcome<void, IOError, NetworkError> error = err(IOError::DiskFull);
    value                                      = error.visit([](std::monostate) { return 0; },
                        [](IOError err) { return static_cast<int>(err) + 10; },
                        [](NetworkError) { return 2; });
    EXPECT_EQ(value, 12);
}

// ============================================================================
// Chain Outcomes Tests (renamed from combine_outcomes; short-circuit + error union)
// ============================================================================

TEST(ChainOutcomesTest, AllSuccessNonVoid) {
    Outcome<int, IOError> r1(10);
    Outcome<std::string, IOError> r2("hello");

    auto combined = chain_outcomes(r1, r2);
    EXPECT_TRUE(combined.is_success());
    EXPECT_EQ(std::get<0>(combined.value()), 10);
    EXPECT_EQ(std::get<1>(combined.value()), "hello");
}

TEST(ChainOutcomesTest, AllSuccessVoid) {
    constexpr Outcome<void, IOError> r1;
    constexpr Outcome<void, IOError> r2;
    constexpr Outcome<void, IOError> r3;

    constexpr auto combined = chain_outcomes(r1, r2, r3);
    EXPECT_TRUE(combined.is_success());
}

TEST(ChainOutcomesTest, FirstError) {
    constexpr Outcome<int, IOError> r1 = err(IOError::FileNotFound);
    constexpr Outcome<int, IOError> r2(20);
    constexpr Outcome<int, IOError> r3(30);

    constexpr auto combined = chain_outcomes(r1, r2, r3);
    EXPECT_TRUE(combined.is_error());
}

TEST(ChainOutcomesTest, MiddleError) {
    constexpr Outcome<int, IOError> r1(10);
    constexpr Outcome<int, IOError> r2 = err(IOError::PermissionDenied);
    constexpr Outcome<int, IOError> r3(30);

    constexpr auto combined = chain_outcomes(r1, r2, r3);
    EXPECT_TRUE(combined.is_error());
}

TEST(ChainOutcomesTest, MixedVoidAndNonVoid) {
    Outcome<void, IOError> r1;
    Outcome<int, IOError> r2(42);
    Outcome<std::string, IOError> r3("hello");

    auto combined = chain_outcomes(r1, r2, r3);
    EXPECT_TRUE(combined.is_success());
    EXPECT_EQ(std::get<0>(combined.value()), 42);
    EXPECT_EQ(std::get<1>(combined.value()), "hello");
}

TEST(ChainOutcomesTest, MergedErrorVariantHoldsFirstErrorType) {
    Outcome<int, IOError> r1 = err(IOError::FileNotFound);
    Outcome<int, NetworkError> r2(42);

    auto combined = chain_outcomes(r1, r2);
    static_assert(std::is_same_v<decltype(combined), Outcome<std::tuple<int, int>, IOError, NetworkError>>);
    EXPECT_TRUE(combined.is_error());
    EXPECT_TRUE(combined.holds_error<IOError>());
    EXPECT_EQ(combined.error<IOError>(), IOError::FileNotFound);
}

TEST(ChainOutcomesTest, MergedErrorVariantHoldsLaterErrorType) {
    Outcome<int, IOError> r1(10);
    Outcome<int, NetworkError> r2 = err(NetworkError::Timeout);

    auto combined = chain_outcomes(r1, r2);
    static_assert(std::is_same_v<decltype(combined), Outcome<std::tuple<int, int>, IOError, NetworkError>>);
    EXPECT_TRUE(combined.is_error());
    EXPECT_TRUE(combined.holds_error<NetworkError>());
    EXPECT_EQ(combined.error<NetworkError>(), NetworkError::Timeout);
}

TEST(ChainOutcomesTest, ShortCircuitOnFirstErrorOnly) {
    // Both r1 and r2 are errored; only r1's error should surface.
    constexpr Outcome<int, IOError> r1      = err(IOError::FileNotFound);
    constexpr Outcome<int, NetworkError> r2 = err(NetworkError::Timeout);

    constexpr auto combined = chain_outcomes(r1, r2);
    EXPECT_TRUE(combined.is_error());
    EXPECT_TRUE(combined.holds_error<IOError>());
    EXPECT_FALSE(combined.holds_error<NetworkError>());
}

// ============================================================================
// Widened and_then (multi-error) Tests
// ============================================================================

TEST(AndThenWideningTest, ResultTypeIsUnionOfErrorVariants) {
    Outcome<int, IOError> a(1);
    auto b = a.and_then([](const int x) -> Outcome<std::string, NetworkError> { return {std::to_string(x)}; });
    static_assert(std::is_same_v<decltype(b), Outcome<std::string, IOError, NetworkError>>);
    EXPECT_TRUE(b.is_success());
    EXPECT_EQ(b.value(), "1");
}

TEST(AndThenWideningTest, FirstErrorPropagatesIntoWiderResult) {
    Outcome<int, IOError> a = err(IOError::FileNotFound);
    auto b = a.and_then([](const int x) -> Outcome<std::string, NetworkError> { return {std::to_string(x)}; });
    static_assert(std::is_same_v<decltype(b), Outcome<std::string, IOError, NetworkError>>);
    EXPECT_TRUE(b.is_error());
    EXPECT_TRUE(b.holds_error<IOError>());
    EXPECT_EQ(b.error<IOError>(), IOError::FileNotFound);
}

TEST(AndThenWideningTest, SecondErrorPropagatesFromCallable) {
    Outcome<int, IOError> a(1);
    const auto b = a.and_then(
        []([[maybe_unused]] int x) -> Outcome<std::string, NetworkError> { return err(NetworkError::Timeout); });
    EXPECT_TRUE(b.is_error());
    EXPECT_TRUE(b.holds_error<NetworkError>());
}

TEST(AndThenWideningTest, NonDefaultConstructibleValueType) {
    // Regression for the default-construct + assign trap in monadic ops:
    // NoDefault has a deleted default ctor, so any code path that
    // internally default-constructs T must fail to compile.
    Outcome<NoDefault, IOError> a(NoDefault{7});
    auto b = std::move(a).and_then(
        [](NoDefault&& nd) -> Outcome<NoDefault, IOError> { return Outcome<NoDefault, IOError>{NoDefault{nd.x}}; });
    EXPECT_TRUE(b.is_success());
    EXPECT_EQ(b.value().x, 7);
}

TEST(AndThenWideningTest, ErrorFactoryWithNonDefaultConstructibleValueType) {
    // Regression for fix #1: Outcome<T,Es>::error(e) must not require T to be
    // default-constructible. NoDefault has no default ctor, so this only
    // compiles if error() goes through the ErrorTag ctor (no T involvement).
    auto a = Outcome<NoDefault, IOError>::error(IOError::FileNotFound);
    EXPECT_TRUE(a.is_error());
    EXPECT_EQ(a.error<IOError>(), IOError::FileNotFound);
}

TEST(AndThenWideningTest, RvalueAndThenMovesValue) {
    struct MoveOnly {
        std::unique_ptr<int> p;
        explicit MoveOnly(int v)
            : p(std::make_unique<int>(v)) {
        }
    };
    Outcome<MoveOnly, IOError> a(MoveOnly{5});
    auto b = std::move(a).and_then([](MoveOnly&& m) -> Outcome<int, IOError> { return {*m.p}; });
    EXPECT_TRUE(b.is_success());
    EXPECT_EQ(b.value(), 5);
}

TEST(AndThenWideningTest, VoidSourceWidensCleanly) {
    constexpr Outcome<void, IOError> a;
    constexpr auto b = a.and_then([]() -> Outcome<int, NetworkError> { return {42}; });
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(b)>, Outcome<int, IOError, NetworkError>>);
    EXPECT_TRUE(b.is_success());
    EXPECT_EQ(b.value(), 42);
}

TEST(AndThenWideningTest, VoidSourceErrorPropagates) {
    constexpr Outcome<void, IOError> a = err(IOError::FileNotFound);
    constexpr auto b                   = a.and_then([]() -> Outcome<int, NetworkError> { return {42}; });
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(b)>, Outcome<int, IOError, NetworkError>>);
    EXPECT_TRUE(b.is_error());
    EXPECT_TRUE(b.holds_error<IOError>());
}

TEST(AndThenWideningTest, EmptyErrorSetSourceWidensCleanly) {
    // Source has no Errors... — degenerate variant<> case.
    Outcome<int> a(1);
    auto b = a.and_then([](const int x) -> Outcome<std::string, IOError> { return {std::to_string(x)}; });
    static_assert(std::is_same_v<decltype(b), Outcome<std::string, IOError>>);
    EXPECT_TRUE(b.is_success());
    EXPECT_EQ(b.value(), "1");
}

// ============================================================================
// Ergonomics: success() lvalue, ==, swap
// ============================================================================

TEST(ErgonomicsTest, SuccessFactoryAcceptsLvalue) {
    std::string s = "hello";
    auto result   = Outcome<std::string, IOError>::success(s);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), "hello");
    EXPECT_EQ(s, "hello");  // unchanged — copied, not moved
}

TEST(ErgonomicsTest, EqualityCompareSuccess) {
    constexpr Outcome<int, IOError> a(42);
    constexpr Outcome<int, IOError> b(42);
    constexpr Outcome<int, IOError> c(7);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(ErgonomicsTest, EqualityCompareError) {
    constexpr Outcome<int, IOError> a = err(IOError::FileNotFound);
    constexpr Outcome<int, IOError> b = err(IOError::FileNotFound);
    constexpr Outcome<int, IOError> c = err(IOError::DiskFull);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(ErgonomicsTest, EqualitySuccessVsError) {
    constexpr Outcome<int, IOError> a(42);
    constexpr Outcome<int, IOError> b = err(IOError::FileNotFound);
    EXPECT_FALSE(a == b);
}

TEST(ErgonomicsTest, SwapSwapsState) {
    Outcome<int, IOError> a(42);
    Outcome<int, IOError> b = err(IOError::FileNotFound);
    swap(a, b);
    EXPECT_TRUE(a.is_error());
    EXPECT_EQ(a.error<IOError>(), IOError::FileNotFound);
    EXPECT_TRUE(b.is_success());
    EXPECT_EQ(b.value(), 42);
}

// ============================================================================
// Real-World Usage Examples
// ============================================================================

// File reading example
Outcome<std::string, IOError> read_file(const std::string& path) {
    if (path.empty()) {
        return err(IOError::FileNotFound);
    }
    if (path == "forbidden") {
        return err(IOError::PermissionDenied);
    }
    return ok(std::string("file contents"));
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
    auto result =
        read_file("test.txt")
            .transform([](const std::string& content) { return content.size(); })
            .and_then([](const std::size_t size) { return Outcome<int, IOError>(static_cast<int>(size) * 2); });
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value(), 26);  // "file contents" = 13 chars * 2
}

// Network request example
Outcome<std::string, NetworkError, ParseError> fetch_and_parse(const std::string& url) {
    if (url.empty()) {
        return err(NetworkError::InvalidResponse);
    }
    if (url == "timeout") {
        return err(NetworkError::Timeout);
    }
    if (url == "invalid_json") {
        return err(ParseError{"Invalid JSON", 1});
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
    Outcome<std::unique_ptr<int>, IOError> result = ok(std::make_unique<int>(42));
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(*result.value(), 42);

    const auto value = std::move(result).value();
    EXPECT_EQ(*value, 42);
}

TEST(EdgeCaseTest, LargeValueType) {
    std::vector<int> large_vec(1000, 42);
    Outcome<std::vector<int>, IOError> result = ok(large_vec);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.value().size(), 1000);
}

TEST(EdgeCaseTest, ConstCorrectness) {
    constexpr Outcome<int, IOError> result(42);
    EXPECT_EQ(result.value(), 42);
    EXPECT_EQ(*result, 42);

    constexpr Outcome<int, IOError> error = err(IOError::FileNotFound);
    EXPECT_EQ(error.error<IOError>(), IOError::FileNotFound);
}
