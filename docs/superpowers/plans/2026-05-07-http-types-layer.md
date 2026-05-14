# HTTP Redesign — PR 1: Types Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the protocol-agnostic core types of the HTTP redesign (Headers, Body, Request, Response, RequestContext, errors, ResponseFactory) as a self-contained `Demiplane::Component::Http::Types` static library with full unit test coverage. After this PR, the new layer is usable from tests and ready for PR 2 (Routing) to build on top. The existing `http_server/` library is left untouched and continues to build and link as before; this PR is strictly additive.

**Architecture:** Pure data + utility layer. No drivers, no servers, no I/O coroutines that touch the network. `Headers` is a tagged-union facade with `BeastBacking` (view over `boost::beast::http::fields`) and `OwnedBacking` (arena-owned strings) variants. `Body` is a virtual base with `EmptyBody`, `StringBody`, and `BeastRequestBody` concrete implementations and buffered helpers (`read_to_string`, `read_json`, `read_form`, `read_multipart`) that return `gears::Outcome` for failure paths. `Request` and `Response` are plain structs with fluent setters on `Response`. `RequestContext` exposes lazy header lookup, pre-decoded path/query params backed by an arena, and a type-keyed middleware data bag. `errors.hpp` defines built-in error types each paired with an ADL `to_http_response(const E&) -> Response` overload.

**Tech Stack:**
- C++23 (deducing this for fluent setters; pmr; concepts)
- Boost.Beast (only for `boost::beast::http::fields*` in `Headers::BeastBacking`)
- Boost.Container (`small_flat_map` for arena-backed param maps)
- Boost.Asio (`asio::awaitable<T>` for `AsyncOutcome` — used by `Body::read_chunk` virtual)
- JsonCpp (`Json::Value`, `Json::CharReaderBuilder`)
- `gears::Outcome` (project's existing typed-error sum type)
- GoogleTest

---

## File Structure

```
components/http/types/
├─ CMakeLists.txt                      ← new, owned by this layer
├─ http_enums.hpp                      Protocol, HttpMethod, HttpStatus, HttpVersion
├─ async_outcome.hpp                   AsyncOutcome<T, Es...> alias
├─ headers/
│  ├─ headers.hpp
│  └─ headers.cpp
├─ body/
│  ├─ body.hpp                         abstract base + EmptyBody, StringBody, BeastRequestBody
│  └─ body.cpp                         buffered helpers (read_to_string, read_json, read_form, read_multipart)
├─ request/
│  ├─ request.hpp                      struct Request
│  └─ request.cpp                      (intentionally near-empty; reserved for non-inline helpers)
├─ response/
│  ├─ response.hpp                     struct Response + fluent setters
│  └─ response.cpp                     (out-of-line setter bodies if any)
├─ errors/
│  ├─ errors.hpp                       error structs + to_http_response declarations
│  └─ errors.cpp                       to_http_response definitions
├─ response_factory/
│  ├─ response_factory.hpp
│  └─ response_factory.cpp
└─ request_context/
   ├─ request_context.hpp
   └─ request_context.cpp

tests/unit_tests/http/types/
├─ test_headers.cpp
├─ test_body.cpp
├─ test_response.cpp
├─ test_response_factory.cpp
├─ test_errors.cpp
└─ test_request_context.cpp

components/http/CMakeLists.txt        ← edited: register Types subdirectory
tests/unit_tests/CMakeLists.txt       ← edited: add Http.Types unit test target
```

After this PR, `Demiplane::Component::Http` (the existing umbrella) gains `Demiplane::Component::Http::Types` as a transitive linked target. Headers like `<request_context/request_context.hpp>` and `<headers/headers.hpp>` resolve against `components/http/types` as the include root.

Build verification at the end of every task: `cmake --build build/release` from the project root completes without errors. Test verification: `ctest --output-on-failure -L unit -R Http.Types` passes.

---

## Task 1: Bootstrap directory tree + minimum-viable CMakeLists

**Files:**
- Create: `components/http/types/CMakeLists.txt`
- Create: `components/http/types/.keep` (empty placeholder so directory commits)
- Modify: `components/http/CMakeLists.txt`

**Goal of this task:** Get an empty `Demiplane::Component::Http::Types` static library compiling and linkable. Subsequent tasks will add real source files; this task just ensures the structure exists and the build doesn't break.

- [ ] **Step 1: Create the directory tree**

```bash
cd /home/grivin/Workspace/Demiplane
mkdir -p components/http/types/{headers,body,request,response,errors,response_factory,request_context}
```

- [ ] **Step 2: Create `components/http/types/CMakeLists.txt`**

```cmake
##############################################################################
# Http Types — protocol-agnostic core types
##############################################################################
add_library(${DMP_HTTP}.Types STATIC)

target_include_directories(${DMP_HTTP}.Types PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types
        PUBLIC
        Demiplane::Common::Gears
        Demiplane::Common::Scroll
        Boost::beast
        Boost::asio
        Boost::container
        JsonCpp::JsonCpp
)

# Source files added in subsequent tasks via target_sources.
##############################################################################
```

The `target_sources` calls live in subsequent tasks alongside the file they reference, keeping each task self-contained.

- [ ] **Step 3: Edit `components/http/CMakeLists.txt`** to register the new sub-CMakeLists *before* the existing `${DMP_HTTP}.Handler` so Types is built first.

The current file starts with:

```cmake
set(DMP_HTTP ${DMP_COMPONENT}.HTTP)


##############################################################################
# Http configs
##############################################################################
```

Change the top to:

```cmake
set(DMP_HTTP ${DMP_COMPONENT}.HTTP)


##############################################################################
# Http Types layer (new, PR 1 of redesign)
##############################################################################
add_subdirectory(types)
##############################################################################


##############################################################################
# Http configs
##############################################################################
```

- [ ] **Step 4: Add a placeholder source file so the static lib has at least one TU**

Create `components/http/types/types_placeholder.cpp`:

```cpp
// Placeholder TU to give Demiplane::Component::Http::Types a non-empty
// compilation unit during PR 1 bootstrap. Replaced by real source files
// in subsequent tasks; deleted at the end of PR 1.
namespace demiplane::http::detail {
    inline constexpr int types_layer_present = 1;
}
```

Append to `components/http/types/CMakeLists.txt` after the `target_link_libraries` block:

```cmake
target_sources(${DMP_HTTP}.Types PRIVATE types_placeholder.cpp)
```

- [ ] **Step 5: Build to verify the bootstrap is wired**

Run:

```bash
cmake --build build/release --target ${DMP_HTTP_FULL_NAME:-Demiplane.Component.HTTP.Types} -- -j4 2>&1 | tail -20
```

If the build directory doesn't exist, run from the repo root:

```bash
cmake --preset release && cmake --build build/release -- -j4 2>&1 | tail -30
```

Expected: build succeeds, `Demiplane.Component.HTTP.Types` static archive is produced under `build/release/components/http/types/`.

- [ ] **Step 6: Commit**

```bash
git add components/http/types components/http/CMakeLists.txt
git commit -m "feat(http): bootstrap Types layer scaffold

Adds empty Demiplane::Component::Http::Types static library that links
into the http component umbrella. No real source files yet; placeholder
TU only. Subsequent tasks add the core types one at a time."
```

---

## Task 2: http_enums.hpp

**Files:**
- Create: `components/http/types/http_enums.hpp`
- Create: `tests/unit_tests/http/types/test_http_enums.cpp`
- Modify: `components/http/types/CMakeLists.txt` (header-only, no source change)
- Modify: `tests/unit_tests/CMakeLists.txt` (register new test target)

**Goal:** Define the four protocol-agnostic enums (`Protocol`, `HttpMethod`, `HttpStatus`, `HttpVersion`) and helper conversion functions (`to_string`, `from_beast`).

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/types/test_http_enums.cpp`:

```cpp
#include <gtest/gtest.h>
#include <http_enums.hpp>

using namespace demiplane::http;

TEST(HttpEnumsTest, MethodToString) {
    EXPECT_EQ(to_string(HttpMethod::get),     std::string_view{"GET"});
    EXPECT_EQ(to_string(HttpMethod::post),    std::string_view{"POST"});
    EXPECT_EQ(to_string(HttpMethod::put),     std::string_view{"PUT"});
    EXPECT_EQ(to_string(HttpMethod::patch),   std::string_view{"PATCH"});
    EXPECT_EQ(to_string(HttpMethod::del),     std::string_view{"DELETE"});
    EXPECT_EQ(to_string(HttpMethod::head),    std::string_view{"HEAD"});
    EXPECT_EQ(to_string(HttpMethod::options), std::string_view{"OPTIONS"});
}

TEST(HttpEnumsTest, MethodFromBeast) {
    EXPECT_EQ(method_from_beast(boost::beast::http::verb::get),     HttpMethod::get);
    EXPECT_EQ(method_from_beast(boost::beast::http::verb::post),    HttpMethod::post);
    EXPECT_EQ(method_from_beast(boost::beast::http::verb::delete_), HttpMethod::del);
    EXPECT_EQ(method_from_beast(boost::beast::http::verb::unknown), HttpMethod::unknown);
}

TEST(HttpEnumsTest, StatusCodeNumericValue) {
    EXPECT_EQ(static_cast<int>(HttpStatus::ok),                    200);
    EXPECT_EQ(static_cast<int>(HttpStatus::created),               201);
    EXPECT_EQ(static_cast<int>(HttpStatus::no_content),            204);
    EXPECT_EQ(static_cast<int>(HttpStatus::bad_request),           400);
    EXPECT_EQ(static_cast<int>(HttpStatus::unauthorized),          401);
    EXPECT_EQ(static_cast<int>(HttpStatus::forbidden),             403);
    EXPECT_EQ(static_cast<int>(HttpStatus::not_found),             404);
    EXPECT_EQ(static_cast<int>(HttpStatus::method_not_allowed),    405);
    EXPECT_EQ(static_cast<int>(HttpStatus::conflict),              409);
    EXPECT_EQ(static_cast<int>(HttpStatus::payload_too_large),     413);
    EXPECT_EQ(static_cast<int>(HttpStatus::unprocessable_entity),  422);
    EXPECT_EQ(static_cast<int>(HttpStatus::internal_server_error), 500);
}

TEST(HttpEnumsTest, VersionNumeric) {
    EXPECT_EQ(static_cast<unsigned>(HttpVersion::http_1_0), 10u);
    EXPECT_EQ(static_cast<unsigned>(HttpVersion::http_1_1), 11u);
    EXPECT_EQ(static_cast<unsigned>(HttpVersion::http_2),   20u);
    EXPECT_EQ(static_cast<unsigned>(HttpVersion::http_3),   30u);
}
```

- [ ] **Step 2: Wire up the test target**

Edit `tests/unit_tests/CMakeLists.txt`. Find a stable location near the bottom of the existing `add_unit_test` blocks and add:

```cmake
##############################################################################
# Test HTTP Types layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Types
        http/types/test_http_enums.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Types
        PRIVATE
        Demiplane::Component::Http
        ${TEST_LIBS}
)
##############################################################################
```

The list of source files in this `add_unit_test` block grows as subsequent tasks add their tests; check the existing list before appending in later tasks.

- [ ] **Step 3: Configure + build the test target — expect failure**

```bash
cmake --preset release && cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -20
```

Expected: compilation fails because `http_enums.hpp` doesn't exist.

- [ ] **Step 4: Create `components/http/types/http_enums.hpp`**

```cpp
#pragma once

#include <cstdint>
#include <string_view>

#include <boost/beast/http/verb.hpp>

namespace demiplane::http {

    enum class Protocol : std::uint8_t {
        http1,
        http2,
        http3,
    };

    enum class HttpMethod : std::uint8_t {
        unknown,
        get,
        post,
        put,
        patch,
        del,        // not 'delete' — that's a keyword
        head,
        options,
    };

    // Numeric values match the HTTP wire status codes for clarity.
    enum class HttpStatus : std::uint16_t {
        ok                       = 200,
        created                  = 201,
        accepted                 = 202,
        no_content               = 204,
        moved_permanently        = 301,
        found                    = 302,
        see_other                = 303,
        not_modified             = 304,
        temporary_redirect       = 307,
        permanent_redirect       = 308,
        bad_request              = 400,
        unauthorized             = 401,
        forbidden                = 403,
        not_found                = 404,
        method_not_allowed       = 405,
        conflict                 = 409,
        gone                     = 410,
        payload_too_large        = 413,
        unsupported_media_type   = 415,
        unprocessable_entity     = 422,
        too_many_requests        = 429,
        internal_server_error    = 500,
        not_implemented          = 501,
        bad_gateway              = 502,
        service_unavailable      = 503,
        gateway_timeout          = 504,
    };

    // Beast uses the same convention (10 = HTTP/1.0, 11 = HTTP/1.1).
    enum class HttpVersion : std::uint8_t {
        http_1_0 = 10,
        http_1_1 = 11,
        http_2   = 20,
        http_3   = 30,
    };

    // Conversion helpers. Inline because they're trivial and called from
    // hot paths (driver translation layers).

    constexpr std::string_view to_string(HttpMethod m) noexcept {
        switch (m) {
            case HttpMethod::get:     return "GET";
            case HttpMethod::post:    return "POST";
            case HttpMethod::put:     return "PUT";
            case HttpMethod::patch:   return "PATCH";
            case HttpMethod::del:     return "DELETE";
            case HttpMethod::head:    return "HEAD";
            case HttpMethod::options: return "OPTIONS";
            case HttpMethod::unknown: return "UNKNOWN";
        }
        return "UNKNOWN";
    }

    constexpr HttpMethod method_from_beast(boost::beast::http::verb v) noexcept {
        using V = boost::beast::http::verb;
        switch (v) {
            case V::get:     return HttpMethod::get;
            case V::post:    return HttpMethod::post;
            case V::put:     return HttpMethod::put;
            case V::patch:   return HttpMethod::patch;
            case V::delete_: return HttpMethod::del;
            case V::head:    return HttpMethod::head;
            case V::options: return HttpMethod::options;
            default:         return HttpMethod::unknown;
        }
    }

    constexpr boost::beast::http::verb method_to_beast(HttpMethod m) noexcept {
        using V = boost::beast::http::verb;
        switch (m) {
            case HttpMethod::get:     return V::get;
            case HttpMethod::post:    return V::post;
            case HttpMethod::put:     return V::put;
            case HttpMethod::patch:   return V::patch;
            case HttpMethod::del:     return V::delete_;
            case HttpMethod::head:    return V::head;
            case HttpMethod::options: return V::options;
            case HttpMethod::unknown: return V::unknown;
        }
        return V::unknown;
    }

    constexpr HttpVersion version_from_beast(unsigned int v) noexcept {
        switch (v) {
            case 10: return HttpVersion::http_1_0;
            case 11: return HttpVersion::http_1_1;
            case 20: return HttpVersion::http_2;
            case 30: return HttpVersion::http_3;
            default: return HttpVersion::http_1_1;  // safest default for unknown
        }
    }

    constexpr unsigned int version_to_beast(HttpVersion v) noexcept {
        return static_cast<unsigned int>(v);
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Run the test target — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -20
```

Expected: build succeeds, all 4 tests pass.

- [ ] **Step 6: Commit**

```bash
git add components/http/types/http_enums.hpp \
        tests/unit_tests/http/types/test_http_enums.cpp \
        tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add http_enums (Protocol, HttpMethod, HttpStatus, HttpVersion)

Header-only enums + Beast-conversion helpers. Foundation for the
protocol-agnostic types in the http redesign."
```

---

## Task 3: async_outcome.hpp

**Files:**
- Create: `components/http/types/async_outcome.hpp`
- (No test file — header is a one-line alias; verified by use in subsequent tasks.)

**Goal:** Define the `AsyncOutcome<T, Es...>` alias used as the canonical handler return type.

- [ ] **Step 1: Create `components/http/types/async_outcome.hpp`**

```cpp
#pragma once

#include <boost/asio/awaitable.hpp>
#include <demiplane/gears>

namespace demiplane::http {

    /**
     * @brief asio coroutine that yields a typed-error sum result.
     *
     * Handlers and middleware return AsyncOutcome<Response, Errors...>; the
     * controller-bind layer converts the held alternative into a plain Response
     * via ADL to_http_response(const E&) on each error type.
     */
    template <typename T, typename... Es>
    using AsyncOutcome = boost::asio::awaitable<gears::Outcome<T, Es...>>;

    /**
     * @brief Convenience for the common "no typed errors" case.
     */
    using AsyncResponse = boost::asio::awaitable<class Response>;

    /**
     * @brief Convenience for fire-and-forget coroutines.
     */
    using AsyncVoid = boost::asio::awaitable<void>;

}  // namespace demiplane::http
```

The forward-declared `class Response` is fine here; `async_outcome.hpp` only declares the alias type. The full `Response` type is defined in Task 8 and `<response/response.hpp>` will be included by everyone who actually uses an `AsyncResponse` (the alias is just a type-spelling convenience).

- [ ] **Step 2: Build to verify the file parses**

```bash
cmake --build build/release --target Demiplane.Component.HTTP.Types -- -j4 2>&1 | tail -10
```

Expected: succeeds. The header is included by no one yet, but the placeholder TU still compiles.

- [ ] **Step 3: Commit**

```bash
git add components/http/types/async_outcome.hpp
git commit -m "feat(http/types): add AsyncOutcome alias

Spell out asio::awaitable<gears::Outcome<T, Es...>> once so handler
return types stay readable."
```

---

## Task 4: errors.hpp — error structs (declarations only)

**Files:**
- Create: `components/http/types/errors/errors.hpp`
- Modify: `components/http/types/CMakeLists.txt`

**Goal:** Define every built-in error struct and forward-declare the `to_http_response(const E&)` overloads. Implementations land in Task 11 once `Response` and `ResponseFactory` exist.

- [ ] **Step 1: Create `components/http/types/errors/errors.hpp`**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "http_enums.hpp"

namespace demiplane::http {

    // Forward declaration so this header doesn't need <response/response.hpp>.
    // Response is defined in Task 8.
    struct Response;

    // ── Application-level errors ─────────────────────────────────────────

    struct BadRequestError    { std::string message; };
    struct UnauthorizedError  { std::string message; };
    struct ForbiddenError     { std::string message; };
    struct NotFoundError      {
        std::string resource;   // e.g. "user"
        std::string id;         // e.g. "42"
    };
    struct ConflictError      { std::string message; };

    struct FieldError {
        std::string field;
        std::string detail;
    };
    struct UnprocessableEntityError {
        std::string message;
        std::vector<FieldError> fields;
    };

    struct PayloadTooLargeError {
        std::size_t limit = 0;
    };

    struct MethodNotAllowedError {
        std::vector<HttpMethod> allowed;
    };

    // ── Parsing / body errors ────────────────────────────────────────────

    struct JsonParseError      { std::string detail; };
    struct FormParseError      { std::string detail; };
    struct MultipartParseError { std::string detail; };
    struct BodyLimitExceeded   { std::size_t limit = 0; };

    // ── ADL conversion declarations ──────────────────────────────────────
    //
    // Defined in errors.cpp once Response and ResponseFactory exist. The
    // controller-bind layer (PR 2) finds these by ADL when collapsing
    // gears::Outcome<Response, Errors...> into a plain Response.
    //
    // Adding a new built-in error type means: declare struct here, add an
    // overload here, define it in errors.cpp.

    Response to_http_response(const BadRequestError& e);
    Response to_http_response(const UnauthorizedError& e);
    Response to_http_response(const ForbiddenError& e);
    Response to_http_response(const NotFoundError& e);
    Response to_http_response(const ConflictError& e);
    Response to_http_response(const UnprocessableEntityError& e);
    Response to_http_response(const PayloadTooLargeError& e);
    Response to_http_response(const MethodNotAllowedError& e);

    Response to_http_response(const JsonParseError& e);
    Response to_http_response(const FormParseError& e);
    Response to_http_response(const MultipartParseError& e);
    Response to_http_response(const BodyLimitExceeded& e);

}  // namespace demiplane::http
```

- [ ] **Step 2: Build to verify the header parses**

```bash
cmake --build build/release --target Demiplane.Component.HTTP.Types -- -j4 2>&1 | tail -10
```

Expected: succeeds. `errors.hpp` is currently included by no one; build is unchanged.

- [ ] **Step 3: Commit**

```bash
git add components/http/types/errors/errors.hpp
git commit -m "feat(http/types): add built-in error types and to_http_response decls

Plain-data error structs paired with ADL to_http_response() conversions.
Conversion definitions land in errors.cpp once Response exists."
```

---

## Task 5: Headers — full implementation with tests

**Files:**
- Create: `components/http/types/headers/headers.hpp`
- Create: `components/http/types/headers/headers.cpp`
- Create: `tests/unit_tests/http/types/test_headers.cpp`
- Modify: `components/http/types/CMakeLists.txt`
- Modify: `tests/unit_tests/CMakeLists.txt` (add to existing block from Task 2)

**Goal:** Tagged-union `Headers` with `BeastBacking` (view over `boost::beast::http::fields`) and `OwnedBacking` (arena-backed strings). Multi-value, case-insensitive, stable insertion order.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/types/test_headers.cpp`:

```cpp
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

#include <boost/beast/http.hpp>
#include <gtest/gtest.h>

#include <headers/headers.hpp>

using namespace demiplane::http;

class HeadersTest : public ::testing::Test {
protected:
    std::pmr::monotonic_buffer_resource resource_{4096};
    std::pmr::polymorphic_allocator<> alloc_{&resource_};
};

TEST_F(HeadersTest, OwnedAddAndGet) {
    Headers h{Headers::owned(alloc_)};
    h.add("Content-Type", "application/json");
    h.add("X-Trace-Id", "abc-123");

    auto ct = h.get("content-type");   // case-insensitive
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "application/json");

    auto trace = h.get("X-Trace-Id");
    ASSERT_TRUE(trace.has_value());
    EXPECT_EQ(*trace, "abc-123");

    EXPECT_FALSE(h.get("X-Missing").has_value());
}

TEST_F(HeadersTest, OwnedMultiValueGetAll) {
    Headers h{Headers::owned(alloc_)};
    h.add("Set-Cookie", "session=abc");
    h.add("Set-Cookie", "trace=xyz");

    auto all = h.get_all("set-cookie");
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0], "session=abc");
    EXPECT_EQ(all[1], "trace=xyz");

    auto first = h.get("set-cookie");
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, "session=abc");   // first-occurrence semantics
}

TEST_F(HeadersTest, OwnedSetReplacesAll) {
    Headers h{Headers::owned(alloc_)};
    h.add("X-Tag", "first");
    h.add("X-Tag", "second");
    h.set("X-Tag", "only");

    auto all = h.get_all("x-tag");
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0], "only");
}

TEST_F(HeadersTest, OwnedRemove) {
    Headers h{Headers::owned(alloc_)};
    h.add("X-A", "1");
    h.add("X-B", "2");
    h.remove("x-a");

    EXPECT_FALSE(h.get("X-A").has_value());
    EXPECT_TRUE(h.get("X-B").has_value());
}

TEST_F(HeadersTest, OwnedContainsAndIteration) {
    Headers h{Headers::owned(alloc_)};
    h.add("Host", "example.com");
    h.add("User-Agent", "test");

    EXPECT_TRUE(h.contains("host"));
    EXPECT_FALSE(h.contains("missing"));

    std::vector<std::string> seen_names;
    for (auto const& [n, v] : h) {
        seen_names.emplace_back(n);
    }
    ASSERT_EQ(seen_names.size(), 2u);
    EXPECT_EQ(seen_names[0], "Host");          // insertion order preserved
    EXPECT_EQ(seen_names[1], "User-Agent");
}

TEST_F(HeadersTest, BeastBackingViewsParsedFields) {
    boost::beast::http::fields fields;
    fields.insert("Content-Type", "text/plain");
    fields.insert("X-Custom", "value");

    Headers h = Headers::view_of_beast(fields);

    auto ct = h.get("Content-Type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "text/plain");
    EXPECT_TRUE(h.contains("x-custom"));
}

TEST_F(HeadersTest, AddOnBeastBackingPromotesToOwned) {
    boost::beast::http::fields fields;
    fields.insert("Content-Type", "text/plain");

    Headers h = Headers::view_of_beast(fields);
    h.add("X-Custom", "value");   // mutation forces switch to OwnedBacking

    EXPECT_TRUE(h.contains("Content-Type"));
    EXPECT_TRUE(h.contains("X-Custom"));
}

TEST_F(HeadersTest, GetOrFallback) {
    Headers h{Headers::owned(alloc_)};
    h.add("X-Tag", "value");

    EXPECT_EQ(h.get_or("X-Tag", "fallback"),    "value");
    EXPECT_EQ(h.get_or("X-Missing", "fallback"), "fallback");
}
```

- [ ] **Step 2: Append to the test target's source list**

Edit `tests/unit_tests/CMakeLists.txt`. Find the `add_unit_test(${UNIT_TESTING_TARGET}.Http.Types ...)` block from Task 2 and add `http/types/test_headers.cpp` to its source list:

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Types
        http/types/test_http_enums.cpp
        http/types/test_headers.cpp
)
```

- [ ] **Step 3: Configure + build — expect failure**

```bash
cmake --preset release && cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -15
```

Expected: `headers/headers.hpp: No such file or directory`.

- [ ] **Step 4: Create `components/http/types/headers/headers.hpp`**

```cpp
#pragma once

#include <cstddef>
#include <iterator>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <boost/beast/http/fields.hpp>

namespace demiplane::http {

    /**
     * @brief Multi-value, case-insensitive, insertion-ordered HTTP header set.
     *
     * Two storage modes hidden behind one API:
     *
     *   BeastBacking   — view over a parser-owned beast::http::fields. Used by
     *                    the h1 driver to expose incoming headers without
     *                    copying. Read-only; any mutation upgrades to owned.
     *   OwnedBacking   — std::pmr::vector<pair<pmr::string, pmr::string>>
     *                    stored in the request arena. Used for outgoing
     *                    responses, h2/h3 incoming (where header strings
     *                    don't have a request-scoped backing buffer), and
     *                    test/synthetic constructions.
     *
     * The variant is internal — handlers see only Headers&, regardless of
     * which protocol surfaced it.
     *
     * Comparisons against header names are case-insensitive.
     */
    class Headers {
    public:
        using value_type = std::pair<std::string_view, std::string_view>;

        // ── Factories ────────────────────────────────────────────────────

        /// Create an empty, owned-backing Headers in the supplied arena.
        static Headers owned(std::pmr::polymorphic_allocator<> alloc);

        /// Create a Headers that views (does not copy) a beast::fields.
        /// The fields object MUST outlive this Headers instance.
        static Headers view_of_beast(const boost::beast::http::fields& fields);

        // ── Read API ─────────────────────────────────────────────────────

        /// First value matching name (case-insensitive); nullopt if absent.
        std::optional<std::string_view> get(std::string_view name) const;

        /// First value or a fallback. Returns std::string so the caller owns
        /// the result regardless of which backing supplied it.
        std::string get_or(std::string_view name,
                           std::string_view fallback) const;

        /// All values matching name, in insertion order.
        std::vector<std::string_view> get_all(std::string_view name) const;

        bool contains(std::string_view name) const;

        // ── Write API (forces OwnedBacking) ──────────────────────────────

        /// Append a new (name, value) entry. Multi-value friendly.
        void add(std::string_view name, std::string_view value);

        /// Replace all entries with this name with a single (name, value).
        void set(std::string_view name, std::string_view value);

        /// Remove every entry matching name.
        void remove(std::string_view name);

        // ── Iteration ────────────────────────────────────────────────────
        //
        // Iterator yields std::pair<std::string_view, std::string_view> in
        // insertion order. Implementation detail: drops a thin adapter on
        // top of either backing.

        class const_iterator {
        public:
            using value_type        = Headers::value_type;
            using reference         = value_type;
            using difference_type   = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            const_iterator() = default;

            value_type operator*() const;
            const_iterator& operator++();
            const_iterator operator++(int);

            friend bool operator==(const const_iterator&,
                                   const const_iterator&) = default;

        private:
            friend class Headers;
            const Headers* h_ = nullptr;
            std::size_t idx_  = 0;
        };

        const_iterator begin() const;
        const_iterator end() const;

        std::size_t size() const;
        bool empty() const { return size() == 0; }

    private:
        struct BeastBacking {
            const boost::beast::http::fields* fields;
        };
        struct OwnedBacking {
            std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>> entries;

            explicit OwnedBacking(std::pmr::polymorphic_allocator<> alloc)
                : entries(alloc) {}
        };

        std::variant<BeastBacking, OwnedBacking> backing_;

        explicit Headers(BeastBacking b) : backing_{b} {}
        explicit Headers(OwnedBacking&& o) : backing_{std::move(o)} {}

        /// If currently BeastBacking, copy entries into a fresh OwnedBacking
        /// allocated in the supplied (or default) arena. Used by mutators.
        OwnedBacking& promote_to_owned(
            std::optional<std::pmr::polymorphic_allocator<>> alloc = {});
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/types/headers/headers.cpp`**

```cpp
#include "headers.hpp"

#include <algorithm>
#include <cctype>

namespace demiplane::http {

    namespace {
        constexpr unsigned char to_lower(unsigned char c) noexcept {
            return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + 32) : c;
        }

        bool iequals(std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (to_lower(static_cast<unsigned char>(a[i]))
                    != to_lower(static_cast<unsigned char>(b[i]))) {
                    return false;
                }
            }
            return true;
        }
    }

    Headers Headers::owned(std::pmr::polymorphic_allocator<> alloc) {
        return Headers{OwnedBacking{alloc}};
    }

    Headers Headers::view_of_beast(const boost::beast::http::fields& fields) {
        return Headers{BeastBacking{&fields}};
    }

    Headers::OwnedBacking& Headers::promote_to_owned(
        std::optional<std::pmr::polymorphic_allocator<>> alloc) {
        if (auto* o = std::get_if<OwnedBacking>(&backing_)) return *o;

        // Currently BeastBacking — copy into a new OwnedBacking.
        const auto& bb = std::get<BeastBacking>(backing_);
        auto a = alloc.value_or(std::pmr::polymorphic_allocator<>{});
        OwnedBacking owned{a};
        for (const auto& f : *bb.fields) {
            std::pmr::string name{std::string_view(f.name_string()), a};
            std::pmr::string value{std::string_view(f.value()), a};
            owned.entries.emplace_back(std::move(name), std::move(value));
        }
        backing_ = std::move(owned);
        return std::get<OwnedBacking>(backing_);
    }

    std::optional<std::string_view> Headers::get(std::string_view name) const {
        return std::visit([&]<typename B>(const B& b) -> std::optional<std::string_view> {
            if constexpr (std::same_as<B, BeastBacking>) {
                auto it = b.fields->find(name);
                if (it == b.fields->end()) return std::nullopt;
                return std::string_view(it->value());
            } else {
                for (const auto& [n, v] : b.entries) {
                    if (iequals(std::string_view(n), name)) {
                        return std::string_view(v);
                    }
                }
                return std::nullopt;
            }
        }, backing_);
    }

    std::string Headers::get_or(std::string_view name,
                                std::string_view fallback) const {
        if (auto v = get(name)) return std::string{*v};
        return std::string{fallback};
    }

    std::vector<std::string_view> Headers::get_all(std::string_view name) const {
        std::vector<std::string_view> out;
        std::visit([&]<typename B>(const B& b) {
            if constexpr (std::same_as<B, BeastBacking>) {
                auto range = b.fields->equal_range(name);
                for (auto it = range.first; it != range.second; ++it) {
                    out.emplace_back(it->value());
                }
            } else {
                for (const auto& [n, v] : b.entries) {
                    if (iequals(std::string_view(n), name)) {
                        out.emplace_back(v);
                    }
                }
            }
        }, backing_);
        return out;
    }

    bool Headers::contains(std::string_view name) const {
        return get(name).has_value();
    }

    void Headers::add(std::string_view name, std::string_view value) {
        auto& owned = promote_to_owned();
        auto a = owned.entries.get_allocator();
        owned.entries.emplace_back(
            std::pmr::string{name, a},
            std::pmr::string{value, a});
    }

    void Headers::set(std::string_view name, std::string_view value) {
        auto& owned = promote_to_owned();
        // Erase all existing matches.
        std::erase_if(owned.entries, [&](const auto& kv) {
            return iequals(std::string_view(kv.first), name);
        });
        auto a = owned.entries.get_allocator();
        owned.entries.emplace_back(
            std::pmr::string{name, a},
            std::pmr::string{value, a});
    }

    void Headers::remove(std::string_view name) {
        auto& owned = promote_to_owned();
        std::erase_if(owned.entries, [&](const auto& kv) {
            return iequals(std::string_view(kv.first), name);
        });
    }

    std::size_t Headers::size() const {
        return std::visit([]<typename B>(const B& b) -> std::size_t {
            if constexpr (std::same_as<B, BeastBacking>) {
                return b.fields->size();
            } else {
                return b.entries.size();
            }
        }, backing_);
    }

    Headers::const_iterator Headers::begin() const {
        const_iterator it;
        it.h_ = this;
        it.idx_ = 0;
        return it;
    }

    Headers::const_iterator Headers::end() const {
        const_iterator it;
        it.h_ = this;
        it.idx_ = size();
        return it;
    }

    Headers::value_type Headers::const_iterator::operator*() const {
        return std::visit([&]<typename B>(const B& b) -> Headers::value_type {
            if constexpr (std::same_as<B, BeastBacking>) {
                auto it = b.fields->begin();
                std::advance(it, idx_);
                return {std::string_view(it->name_string()),
                        std::string_view(it->value())};
            } else {
                const auto& [n, v] = b.entries[idx_];
                return {std::string_view(n), std::string_view(v)};
            }
        }, h_->backing_);
    }

    Headers::const_iterator& Headers::const_iterator::operator++() {
        ++idx_;
        return *this;
    }

    Headers::const_iterator Headers::const_iterator::operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Wire the source files into the Types CMakeLists**

Append to `components/http/types/CMakeLists.txt`:

```cmake
target_sources(${DMP_HTTP}.Types PRIVATE
        headers/headers.cpp
)
```

- [ ] **Step 7: Build + run tests — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -15
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -25
```

Expected: build succeeds, all 8 Headers tests pass.

- [ ] **Step 8: Commit**

```bash
git add components/http/types/headers \
        tests/unit_tests/http/types/test_headers.cpp \
        components/http/types/CMakeLists.txt \
        tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add Headers with BeastBacking + OwnedBacking

Tagged-union facade — incoming requests view Beast's parsed fields
directly (no copies); outgoing responses and synthetic constructions
use arena-backed pmr::strings. Multi-value, case-insensitive,
insertion-ordered. Mutations on a BeastBacking promote to OwnedBacking."
```

---

## Task 6: Body — abstract base + EmptyBody + StringBody (no buffered helpers yet)

**Files:**
- Create: `components/http/types/body/body.hpp`
- Create: `components/http/types/body/body.cpp`
- Create: `tests/unit_tests/http/types/test_body.cpp`
- Modify: `components/http/types/CMakeLists.txt`
- Modify: `tests/unit_tests/CMakeLists.txt`

**Goal:** `Body` virtual base + two trivial concrete bodies. Buffered helpers (`read_to_string`, `read_json`, `read_form`, `read_multipart`) land in Task 11 once `errors.hpp` definitions exist; the helper *declarations* go on the base class now.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/types/test_body.cpp`:

```cpp
#include <cstring>
#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>

#include <body/body.hpp>

using namespace demiplane::http;

namespace {
    // Drive an awaitable to completion and return its value.
    template <typename T>
    T run_awaitable(boost::asio::awaitable<T> aw) {
        boost::asio::io_context ioc;
        auto fut = boost::asio::co_spawn(ioc, std::move(aw), boost::asio::use_future);
        ioc.run();
        return fut.get();
    }
}

TEST(BodyTest, EmptyBodyYieldsNoChunks) {
    EmptyBody body;
    auto chunk = run_awaitable(body.read_chunk());
    EXPECT_FALSE(chunk.has_value());
    EXPECT_EQ(body.size_hint().value_or(99), 0u);
}

TEST(BodyTest, StringBodyYieldsContentsThenEnd) {
    StringBody body{"hello, world"};

    auto first = run_awaitable(body.read_chunk());
    ASSERT_TRUE(first.has_value());
    std::string text(reinterpret_cast<const char*>(first->data()), first->size());
    EXPECT_EQ(text, "hello, world");

    auto second = run_awaitable(body.read_chunk());
    EXPECT_FALSE(second.has_value());

    EXPECT_EQ(body.size_hint().value_or(0), 12u);
}

TEST(BodyTest, StringBodyEmptyYieldsNoChunks) {
    StringBody body{""};
    auto chunk = run_awaitable(body.read_chunk());
    EXPECT_FALSE(chunk.has_value());
    EXPECT_EQ(body.size_hint().value_or(99), 0u);
}
```

- [ ] **Step 2: Append to test target sources**

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Types
        http/types/test_http_enums.cpp
        http/types/test_headers.cpp
        http/types/test_body.cpp
)
```

- [ ] **Step 3: Build — expect failure**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
```

Expected: `body/body.hpp: No such file or directory`.

- [ ] **Step 4: Create `components/http/types/body/body.hpp`**

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <demiplane/gears>
#include <json/json.h>

#include "../async_outcome.hpp"
#include "../errors/errors.hpp"

namespace demiplane::http {

    /// One field parsed from a multipart/form-data body.
    struct MultipartField {
        std::string name;
        std::string value;        // empty if filename != "" (file uploads
                                  // expose payload via separate hook)
        std::string content_type;
        std::string filename;     // empty for non-file fields
    };

    /**
     * @brief Streaming-truth body abstraction.
     *
     * Drivers expose request bodies as views into the receive buffer or as
     * arena-allocated buffers. Outgoing response bodies use one-shot or
     * streaming-producer subclasses.
     *
     * The buffered helpers (read_to_string et al.) are non-virtual; they
     * drive read_chunk() in a loop with a limit. They live in body.cpp and
     * are added in Task 11 — only declared here for now.
     */
    class Body {
    public:
        virtual ~Body() = default;

        Body(const Body&) = delete;
        Body& operator=(const Body&) = delete;
        Body(Body&&) = default;
        Body& operator=(Body&&) = default;

        /**
         * Streaming read. Returns the next chunk of the body, or std::nullopt
         * when the body is exhausted. Implementations may return as little as
         * one byte at a time or as much as the entire body in one chunk.
         */
        virtual boost::asio::awaitable<std::optional<std::span<const std::byte>>>
            read_chunk() = 0;

        /// Best-effort size estimate. Set when Content-Length is known;
        /// std::nullopt for chunked-transfer-encoded bodies.
        virtual std::optional<std::size_t> size_hint() const = 0;

        // ── Buffered helpers (definitions in Task 11) ────────────────────

        AsyncOutcome<std::string, BodyLimitExceeded>
            read_to_string(std::size_t limit);

        AsyncOutcome<Json::Value, JsonParseError, BodyLimitExceeded>
            read_json(std::size_t limit);

        AsyncOutcome<std::unordered_map<std::string, std::string>,
                     FormParseError, BodyLimitExceeded>
            read_form(std::size_t limit);

        AsyncOutcome<std::vector<MultipartField>,
                     MultipartParseError, BodyLimitExceeded>
            read_multipart(std::size_t limit, std::string_view boundary);

    protected:
        Body() = default;
    };

    // ── EmptyBody ────────────────────────────────────────────────────────

    /// A body with no chunks. Used for GET/HEAD requests, 204/304 responses.
    class EmptyBody final : public Body {
    public:
        boost::asio::awaitable<std::optional<std::span<const std::byte>>>
            read_chunk() override;
        std::optional<std::size_t> size_hint() const override { return 0; }
    };

    // ── StringBody ───────────────────────────────────────────────────────

    /// A body backed by an owned std::string. One-shot read: returns the
    /// entire content in the first call to read_chunk(), then nullopt.
    class StringBody final : public Body {
    public:
        explicit StringBody(std::string content);

        boost::asio::awaitable<std::optional<std::span<const std::byte>>>
            read_chunk() override;
        std::optional<std::size_t> size_hint() const override {
            return content_.size();
        }

        /// Direct access — used by ResponseFactory and by drivers writing
        /// the response onto the wire.
        std::string_view view() const { return content_; }
        std::string take_content() && { return std::move(content_); }

    private:
        std::string content_;
        bool consumed_ = false;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/types/body/body.cpp`**

```cpp
#include "body.hpp"

#include <utility>

namespace demiplane::http {

    boost::asio::awaitable<std::optional<std::span<const std::byte>>>
    EmptyBody::read_chunk() {
        co_return std::nullopt;
    }

    StringBody::StringBody(std::string content)
        : content_{std::move(content)} {}

    boost::asio::awaitable<std::optional<std::span<const std::byte>>>
    StringBody::read_chunk() {
        if (consumed_ || content_.empty()) {
            consumed_ = true;
            co_return std::nullopt;
        }
        consumed_ = true;
        std::span<const std::byte> view{
            reinterpret_cast<const std::byte*>(content_.data()),
            content_.size()};
        co_return view;
    }

    // Buffered helper definitions land in Task 11.

}  // namespace demiplane::http
```

- [ ] **Step 6: Wire source into Types CMakeLists**

Append to `components/http/types/CMakeLists.txt`:

```cmake
target_sources(${DMP_HTTP}.Types PRIVATE
        body/body.cpp
)
```

- [ ] **Step 7: Build — expect link failure** referencing the still-undefined `read_to_string` / `read_json` / `read_form` / `read_multipart` declarations.

Actually, since those are member functions declared on `Body`, but no test calls them yet, this should *link* fine — undefined methods only break linking if referenced. The tests don't reference them yet. Run to confirm:

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -15
```

Expected: build succeeds. (If it fails on undefined symbol references, that means a test file in this PR is calling a buffered helper — defer that test to Task 11.)

- [ ] **Step 8: Run tests — expect pass**

```bash
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -20
```

Expected: 11 tests pass (8 from Headers, 3 from Body).

- [ ] **Step 9: Commit**

```bash
git add components/http/types/body \
        tests/unit_tests/http/types/test_body.cpp \
        components/http/types/CMakeLists.txt \
        tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add Body abstract base + EmptyBody + StringBody

read_chunk() is the streaming primitive. EmptyBody yields no chunks;
StringBody yields its contents in one chunk then nullopt. Buffered
helpers (read_to_string, read_json, read_form, read_multipart) are
declared but not yet defined — they land in Task 11 once errors.cpp
exists."
```

---

## Task 7: Request struct

**Files:**
- Create: `components/http/types/request/request.hpp`
- Create: `components/http/types/request/request.cpp`
- (No standalone test — `Request` is exercised via tests in Tasks 8-15.)
- Modify: `components/http/types/CMakeLists.txt`

**Goal:** Plain value type carrying everything an HTTP request consists of (method, target, version, headers, body) without leaking Beast types.

- [ ] **Step 1: Create `components/http/types/request/request.hpp`**

```cpp
#pragma once

#include <memory>
#include <string>

#include "../body/body.hpp"
#include "../headers/headers.hpp"
#include "../http_enums.hpp"

namespace demiplane::http {

    /**
     * @brief Protocol-agnostic HTTP request.
     *
     * Drivers translate from their wire representation into Request before
     * dispatch. Plain value type; no Beast types in the API.
     *
     * `target` is the raw, undecoded request target as it appeared on the
     * wire (e.g. "/users/42?q=foo"). Path/query splitting and URL decoding
     * happen in RequestContext, not here.
     *
     * `body` is owned via unique_ptr because Body subclasses are not
     * value-equivalent (StringBody vs EmptyBody vs BeastRequestBody have
     * different layouts). The pointer is never null — empty bodies are
     * concrete EmptyBody instances.
     */
    struct Request {
        HttpMethod method   = HttpMethod::unknown;
        HttpVersion version = HttpVersion::http_1_1;
        std::string target;
        Headers headers;
        std::unique_ptr<Body> body;
    };

}  // namespace demiplane::http
```

- [ ] **Step 2: Create `components/http/types/request/request.cpp`**

```cpp
// Reserved for non-inline Request helpers if needed in the future.
// Kept as a real TU so the module always has at least one .cpp under it
// (avoids stale `target_sources` lists with phantom .cpp paths).
#include "request.hpp"
```

- [ ] **Step 3: Wire source into Types CMakeLists**

```cmake
target_sources(${DMP_HTTP}.Types PRIVATE
        request/request.cpp
)
```

- [ ] **Step 4: Build to verify**

```bash
cmake --build build/release --target Demiplane.Component.HTTP.Types -- -j4 2>&1 | tail -10
```

Expected: succeeds.

- [ ] **Step 5: Commit**

```bash
git add components/http/types/request components/http/types/CMakeLists.txt
git commit -m "feat(http/types): add Request struct

Plain value type aggregating method, version, target, headers, body.
Body is unique_ptr-owned; never null."
```

---

## Task 8: Response struct + fluent setters with tests

**Files:**
- Create: `components/http/types/response/response.hpp`
- Create: `components/http/types/response/response.cpp`
- Create: `tests/unit_tests/http/types/test_response.cpp`
- Modify: `components/http/types/CMakeLists.txt`
- Modify: `tests/unit_tests/CMakeLists.txt`

**Goal:** `Response` plain struct with fluent setters using deducing-this. Setters return ref-qualified self for both lvalue and rvalue chaining.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/types/test_response.cpp`:

```cpp
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <body/body.hpp>
#include <response/response.hpp>

using namespace demiplane::http;

TEST(ResponseTest, DefaultConstructible) {
    Response r;
    EXPECT_EQ(r.status, HttpStatus::ok);
    EXPECT_EQ(r.version, HttpVersion::http_1_1);
    EXPECT_TRUE(r.headers.empty());
}

TEST(ResponseTest, FluentSettersOnLValue) {
    Response r;
    r.with_status(HttpStatus::created)
     .with_header("Content-Type", "application/json")
     .with_body("{\"id\":42}");

    EXPECT_EQ(r.status, HttpStatus::created);
    auto ct = r.headers.get("content-type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "application/json");
    ASSERT_NE(r.body, nullptr);
    auto* sb = dynamic_cast<StringBody*>(r.body.get());
    ASSERT_NE(sb, nullptr);
    EXPECT_EQ(sb->view(), "{\"id\":42}");
}

TEST(ResponseTest, FluentSettersOnRValueReturnRValue) {
    Response r = Response{}
        .with_status(HttpStatus::no_content)
        .with_header("X-Custom", "value");

    EXPECT_EQ(r.status, HttpStatus::no_content);
    auto custom = r.headers.get("X-Custom");
    ASSERT_TRUE(custom.has_value());
    EXPECT_EQ(*custom, "value");
}

TEST(ResponseTest, KeepAliveDefaultIsTrue) {
    Response r;
    EXPECT_TRUE(r.keep_alive);
}

TEST(ResponseTest, ExplicitKeepAliveFalse) {
    Response r;
    r.with_keep_alive(false);
    EXPECT_FALSE(r.keep_alive);
}
```

- [ ] **Step 2: Append to test sources**

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Types
        http/types/test_http_enums.cpp
        http/types/test_headers.cpp
        http/types/test_body.cpp
        http/types/test_response.cpp
)
```

- [ ] **Step 3: Build — expect failure**

Expected: `response/response.hpp: No such file or directory`.

- [ ] **Step 4: Create `components/http/types/response/response.hpp`**

```cpp
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "../body/body.hpp"
#include "../headers/headers.hpp"
#include "../http_enums.hpp"

namespace demiplane::http {

    /**
     * @brief Protocol-agnostic HTTP response.
     *
     * Plain struct; fluent setters use deducing-this so chains work on both
     * lvalues and temporaries with the same syntax. body is a unique_ptr
     * so concrete subclasses (StringBody, StreamingProducerBody, EmptyBody)
     * fit polymorphically.
     *
     * The driver stamps Date/Server headers right before write — don't set
     * them here.
     */
    struct Response {
        HttpStatus status   = HttpStatus::ok;
        HttpVersion version = HttpVersion::http_1_1;
        bool keep_alive     = true;     // request->response keep-alive carry-over
        Headers headers;
        std::unique_ptr<Body> body = std::make_unique<EmptyBody>();

        // ── Fluent setters (deducing this; chain on lvalues + rvalues) ──

        template <typename Self>
        constexpr auto&& with_status(this Self&& self, HttpStatus s) noexcept {
            self.status = s;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& with_version(this Self&& self, HttpVersion v) noexcept {
            self.version = v;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& with_keep_alive(this Self&& self, bool keep) noexcept {
            self.keep_alive = keep;
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& with_header(this Self&& self,
                           std::string_view name,
                           std::string_view value) {
            self.headers.add(name, value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& with_body(this Self&& self, std::string content) {
            self.body = std::make_unique<StringBody>(std::move(content));
            return std::forward<Self>(self);
        }

        template <typename Self, typename BodyT>
            requires std::derived_from<BodyT, Body>
        auto&& with_body(this Self&& self, std::unique_ptr<BodyT> b) {
            self.body = std::move(b);
            return std::forward<Self>(self);
        }
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/types/response/response.cpp`**

```cpp
// Currently no out-of-line definitions — Response is header-only — but
// keep the TU so the CMake source list isn't a phantom path.
#include "response.hpp"
```

- [ ] **Step 6: Wire source**

Append to `components/http/types/CMakeLists.txt`:

```cmake
target_sources(${DMP_HTTP}.Types PRIVATE
        response/response.cpp
)
```

- [ ] **Step 7: Build + run tests — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -15
```

Expected: all 16 tests pass.

- [ ] **Step 8: Commit**

```bash
git add components/http/types/response \
        tests/unit_tests/http/types/test_response.cpp \
        components/http/types/CMakeLists.txt \
        tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add Response struct with fluent setters

Deducing-this fluent setters chain on both lvalues and rvalues. Body
defaults to EmptyBody so a default-constructed Response is valid. Driver
stamps Date/Server headers; factory does not."
```

---

## Task 9: ResponseFactory

**Files:**
- Create: `components/http/types/response_factory/response_factory.hpp`
- Create: `components/http/types/response_factory/response_factory.cpp`
- Create: `tests/unit_tests/http/types/test_response_factory.cpp`
- Modify: `components/http/types/CMakeLists.txt`
- Modify: `tests/unit_tests/CMakeLists.txt`

**Goal:** Static factory producing canonical `Response` shapes for the common HTTP outcomes. Status/Content-Type pre-populated; body via `StringBody`; no `Date`/`Server` headers (driver stamps those).

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/types/test_response_factory.cpp`:

```cpp
#include <gtest/gtest.h>

#include <body/body.hpp>
#include <response_factory/response_factory.hpp>

using namespace demiplane::http;

TEST(ResponseFactoryTest, OkDefault) {
    auto r = ResponseFactory::ok();
    EXPECT_EQ(r.status, HttpStatus::ok);
    auto ct = r.headers.get("Content-Type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "text/plain");
    auto* sb = dynamic_cast<StringBody*>(r.body.get());
    ASSERT_NE(sb, nullptr);
    EXPECT_EQ(sb->view(), "");
}

TEST(ResponseFactoryTest, OkCustomBodyAndContentType) {
    auto r = ResponseFactory::ok("payload", "text/csv");
    EXPECT_EQ(r.status, HttpStatus::ok);
    auto ct = r.headers.get("Content-Type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "text/csv");
    auto* sb = dynamic_cast<StringBody*>(r.body.get());
    ASSERT_NE(sb, nullptr);
    EXPECT_EQ(sb->view(), "payload");
}

TEST(ResponseFactoryTest, JsonHardcodesContentType) {
    auto r = ResponseFactory::json("{\"k\":\"v\"}");
    EXPECT_EQ(r.status, HttpStatus::ok);
    auto ct = r.headers.get("Content-Type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "application/json");
}

TEST(ResponseFactoryTest, NotFound) {
    auto r = ResponseFactory::not_found();
    EXPECT_EQ(r.status, HttpStatus::not_found);
    auto ct = r.headers.get("Content-Type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "text/plain");
    auto* sb = dynamic_cast<StringBody*>(r.body.get());
    ASSERT_NE(sb, nullptr);
    EXPECT_EQ(sb->view(), "Not Found");
}

TEST(ResponseFactoryTest, NoContent) {
    auto r = ResponseFactory::no_content();
    EXPECT_EQ(r.status, HttpStatus::no_content);
    auto* eb = dynamic_cast<EmptyBody*>(r.body.get());
    ASSERT_NE(eb, nullptr);   // 204 has no body
    EXPECT_FALSE(r.headers.contains("Content-Type"));
}

TEST(ResponseFactoryTest, RedirectSetsLocationHeader) {
    auto r = ResponseFactory::redirect("/login");
    EXPECT_EQ(r.status, HttpStatus::found);
    auto loc = r.headers.get("Location");
    ASSERT_TRUE(loc.has_value());
    EXPECT_EQ(*loc, "/login");
}

TEST(ResponseFactoryTest, MethodNotAllowedSetsAllowHeader) {
    HttpMethod allow[] = {HttpMethod::get, HttpMethod::post};
    auto r = ResponseFactory::method_not_allowed(allow);
    EXPECT_EQ(r.status, HttpStatus::method_not_allowed);
    auto allow_hdr = r.headers.get("Allow");
    ASSERT_TRUE(allow_hdr.has_value());
    EXPECT_EQ(*allow_hdr, "GET, POST");
}

TEST(ResponseFactoryTest, Custom) {
    auto r = ResponseFactory::custom(HttpStatus::accepted, "queued", "text/plain");
    EXPECT_EQ(r.status, HttpStatus::accepted);
    auto* sb = dynamic_cast<StringBody*>(r.body.get());
    ASSERT_NE(sb, nullptr);
    EXPECT_EQ(sb->view(), "queued");
}
```

- [ ] **Step 2: Append to test sources**

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Types
        http/types/test_http_enums.cpp
        http/types/test_headers.cpp
        http/types/test_body.cpp
        http/types/test_response.cpp
        http/types/test_response_factory.cpp
)
```

- [ ] **Step 3: Build — expect failure**

Expected: `response_factory/response_factory.hpp: No such file or directory`.

- [ ] **Step 4: Create `components/http/types/response_factory/response_factory.hpp`**

```cpp
#pragma once

#include <span>
#include <string>
#include <string_view>

#include "../http_enums.hpp"
#include "../response/response.hpp"

namespace demiplane::http {

    /**
     * @brief Canonical-shape Response factories.
     *
     * Each method sets status, Content-Type (where applicable), and body.
     * Date and Server headers are *not* set here — drivers stamp those once
     * before write so every response has them regardless of construction
     * path.
     *
     * version defaults to HttpVersion::http_1_1; the driver overwrites with
     * the request's version before write.
     */
    class ResponseFactory {
    public:
        static Response ok(std::string body = "",
                           std::string_view content_type = "text/plain");

        static Response json(std::string body);

        static Response created(std::string body = "",
                                std::string_view content_type = "application/json");

        static Response no_content();

        static Response redirect(std::string_view location,
                                 HttpStatus status = HttpStatus::found);

        static Response not_found(std::string body    = "Not Found");
        static Response bad_request(std::string body  = "Bad Request");
        static Response unauthorized(std::string body = "Unauthorized");
        static Response forbidden(std::string body    = "Forbidden");
        static Response conflict(std::string body     = "Conflict");
        static Response payload_too_large(std::string body = "Payload Too Large");
        static Response unprocessable_entity(std::string body = "Unprocessable Entity");
        static Response internal_error(std::string body = "Internal Server Error");

        static Response method_not_allowed(std::span<const HttpMethod> allow);

        static Response custom(HttpStatus status,
                               std::string body,
                               std::string_view content_type = "text/plain");
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/types/response_factory/response_factory.cpp`**

```cpp
#include "response_factory.hpp"

#include <utility>

namespace demiplane::http {

    namespace {
        Response make_with_body(HttpStatus status,
                                std::string body,
                                std::string_view content_type) {
            Response r;
            r.status = status;
            r.headers.add("Content-Type", content_type);
            r.body = std::make_unique<StringBody>(std::move(body));
            return r;
        }
    }

    Response ResponseFactory::ok(std::string body, std::string_view ct) {
        return make_with_body(HttpStatus::ok, std::move(body), ct);
    }

    Response ResponseFactory::json(std::string body) {
        return make_with_body(HttpStatus::ok, std::move(body), "application/json");
    }

    Response ResponseFactory::created(std::string body, std::string_view ct) {
        return make_with_body(HttpStatus::created, std::move(body), ct);
    }

    Response ResponseFactory::no_content() {
        Response r;
        r.status = HttpStatus::no_content;
        // body stays EmptyBody; no Content-Type per RFC 7230.
        return r;
    }

    Response ResponseFactory::redirect(std::string_view location, HttpStatus status) {
        Response r;
        r.status = status;
        r.headers.add("Location", location);
        return r;
    }

    Response ResponseFactory::not_found(std::string body) {
        return make_with_body(HttpStatus::not_found, std::move(body), "text/plain");
    }
    Response ResponseFactory::bad_request(std::string body) {
        return make_with_body(HttpStatus::bad_request, std::move(body), "text/plain");
    }
    Response ResponseFactory::unauthorized(std::string body) {
        return make_with_body(HttpStatus::unauthorized, std::move(body), "text/plain");
    }
    Response ResponseFactory::forbidden(std::string body) {
        return make_with_body(HttpStatus::forbidden, std::move(body), "text/plain");
    }
    Response ResponseFactory::conflict(std::string body) {
        return make_with_body(HttpStatus::conflict, std::move(body), "text/plain");
    }
    Response ResponseFactory::payload_too_large(std::string body) {
        return make_with_body(HttpStatus::payload_too_large, std::move(body), "text/plain");
    }
    Response ResponseFactory::unprocessable_entity(std::string body) {
        return make_with_body(HttpStatus::unprocessable_entity, std::move(body), "text/plain");
    }
    Response ResponseFactory::internal_error(std::string body) {
        return make_with_body(HttpStatus::internal_server_error,
                              std::move(body), "text/plain");
    }

    Response ResponseFactory::method_not_allowed(std::span<const HttpMethod> allow) {
        Response r;
        r.status = HttpStatus::method_not_allowed;

        std::string allow_value;
        bool first = true;
        for (auto m : allow) {
            if (!first) allow_value += ", ";
            allow_value += to_string(m);
            first = false;
        }
        r.headers.add("Allow", allow_value);

        return r;
    }

    Response ResponseFactory::custom(HttpStatus status,
                                     std::string body,
                                     std::string_view ct) {
        return make_with_body(status, std::move(body), ct);
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Wire source**

```cmake
target_sources(${DMP_HTTP}.Types PRIVATE
        response_factory/response_factory.cpp
)
```

- [ ] **Step 7: Build + run tests — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -15
```

Expected: all 24 tests pass.

- [ ] **Step 8: Commit**

```bash
git add components/http/types/response_factory \
        tests/unit_tests/http/types/test_response_factory.cpp \
        components/http/types/CMakeLists.txt \
        tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add ResponseFactory canonical-shape helpers

ok/json/created/no_content/redirect plus 4xx/5xx variants. Sets status
and Content-Type; body via StringBody (or EmptyBody for 204). No Date
or Server header — drivers stamp those before write."
```

---

## Task 10: errors.cpp — to_http_response definitions

**Files:**
- Create: `components/http/types/errors/errors.cpp`
- Create: `tests/unit_tests/http/types/test_errors.cpp`
- Modify: `components/http/types/CMakeLists.txt`
- Modify: `tests/unit_tests/CMakeLists.txt`

**Goal:** Implement every `to_http_response(const E&)` overload declared in `errors.hpp`. Each maps to a sensible HTTP status + body via `ResponseFactory`.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/types/test_errors.cpp`:

```cpp
#include <gtest/gtest.h>

#include <body/body.hpp>
#include <errors/errors.hpp>
#include <response/response.hpp>

using namespace demiplane::http;

namespace {
    std::string body_of(const Response& r) {
        auto* sb = dynamic_cast<const StringBody*>(r.body.get());
        return sb ? std::string{sb->view()} : "";
    }
}

TEST(ToHttpResponseTest, BadRequestErrorYields400) {
    auto r = to_http_response(BadRequestError{"missing field"});
    EXPECT_EQ(r.status, HttpStatus::bad_request);
    EXPECT_EQ(body_of(r), "missing field");
}

TEST(ToHttpResponseTest, UnauthorizedErrorYields401) {
    auto r = to_http_response(UnauthorizedError{"bad token"});
    EXPECT_EQ(r.status, HttpStatus::unauthorized);
    EXPECT_EQ(body_of(r), "bad token");
}

TEST(ToHttpResponseTest, ForbiddenErrorYields403) {
    auto r = to_http_response(ForbiddenError{"not allowed"});
    EXPECT_EQ(r.status, HttpStatus::forbidden);
    EXPECT_EQ(body_of(r), "not allowed");
}

TEST(ToHttpResponseTest, NotFoundErrorYields404WithResourceAndId) {
    auto r = to_http_response(NotFoundError{"user", "42"});
    EXPECT_EQ(r.status, HttpStatus::not_found);
    EXPECT_EQ(body_of(r), "user 42 not found");
}

TEST(ToHttpResponseTest, ConflictErrorYields409) {
    auto r = to_http_response(ConflictError{"already exists"});
    EXPECT_EQ(r.status, HttpStatus::conflict);
    EXPECT_EQ(body_of(r), "already exists");
}

TEST(ToHttpResponseTest, UnprocessableEntityIncludesFields) {
    UnprocessableEntityError e{"validation failed",
        {{"name", "required"}, {"age", "must be positive"}}};
    auto r = to_http_response(e);
    EXPECT_EQ(r.status, HttpStatus::unprocessable_entity);
    auto body = body_of(r);
    EXPECT_NE(body.find("validation failed"), std::string::npos);
    EXPECT_NE(body.find("name: required"), std::string::npos);
    EXPECT_NE(body.find("age: must be positive"), std::string::npos);
}

TEST(ToHttpResponseTest, PayloadTooLargeIncludesLimit) {
    auto r = to_http_response(PayloadTooLargeError{1024});
    EXPECT_EQ(r.status, HttpStatus::payload_too_large);
    EXPECT_NE(body_of(r).find("1024"), std::string::npos);
}

TEST(ToHttpResponseTest, MethodNotAllowedSetsAllowHeader) {
    MethodNotAllowedError e{{HttpMethod::get, HttpMethod::head}};
    auto r = to_http_response(e);
    EXPECT_EQ(r.status, HttpStatus::method_not_allowed);
    auto allow = r.headers.get("Allow");
    ASSERT_TRUE(allow.has_value());
    EXPECT_EQ(*allow, "GET, HEAD");
}

TEST(ToHttpResponseTest, JsonParseErrorYields400) {
    auto r = to_http_response(JsonParseError{"unterminated string at line 3"});
    EXPECT_EQ(r.status, HttpStatus::bad_request);
    EXPECT_NE(body_of(r).find("unterminated string"), std::string::npos);
}

TEST(ToHttpResponseTest, FormParseErrorYields400) {
    auto r = to_http_response(FormParseError{"invalid encoding"});
    EXPECT_EQ(r.status, HttpStatus::bad_request);
}

TEST(ToHttpResponseTest, MultipartParseErrorYields400) {
    auto r = to_http_response(MultipartParseError{"missing boundary"});
    EXPECT_EQ(r.status, HttpStatus::bad_request);
}

TEST(ToHttpResponseTest, BodyLimitExceededYields413) {
    auto r = to_http_response(BodyLimitExceeded{16 * 1024 * 1024});
    EXPECT_EQ(r.status, HttpStatus::payload_too_large);
    EXPECT_NE(body_of(r).find("16777216"), std::string::npos);
}
```

- [ ] **Step 2: Append to test sources**

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Types
        http/types/test_http_enums.cpp
        http/types/test_headers.cpp
        http/types/test_body.cpp
        http/types/test_response.cpp
        http/types/test_response_factory.cpp
        http/types/test_errors.cpp
)
```

- [ ] **Step 3: Build — expect link failure** because `to_http_response` overloads aren't defined yet.

- [ ] **Step 4: Create `components/http/types/errors/errors.cpp`**

```cpp
#include "errors.hpp"

#include <sstream>

#include "../response/response.hpp"
#include "../response_factory/response_factory.hpp"

namespace demiplane::http {

    Response to_http_response(const BadRequestError& e) {
        return ResponseFactory::bad_request(e.message);
    }

    Response to_http_response(const UnauthorizedError& e) {
        return ResponseFactory::unauthorized(e.message);
    }

    Response to_http_response(const ForbiddenError& e) {
        return ResponseFactory::forbidden(e.message);
    }

    Response to_http_response(const NotFoundError& e) {
        std::string body = e.resource;
        if (!e.id.empty()) {
            body += ' ';
            body += e.id;
        }
        body += " not found";
        return ResponseFactory::not_found(std::move(body));
    }

    Response to_http_response(const ConflictError& e) {
        return ResponseFactory::conflict(e.message);
    }

    Response to_http_response(const UnprocessableEntityError& e) {
        std::ostringstream os;
        os << e.message;
        for (const auto& f : e.fields) {
            os << "\n" << f.field << ": " << f.detail;
        }
        return ResponseFactory::unprocessable_entity(os.str());
    }

    Response to_http_response(const PayloadTooLargeError& e) {
        std::ostringstream os;
        os << "Payload Too Large (limit " << e.limit << " bytes)";
        return ResponseFactory::payload_too_large(os.str());
    }

    Response to_http_response(const MethodNotAllowedError& e) {
        return ResponseFactory::method_not_allowed(e.allowed);
    }

    Response to_http_response(const JsonParseError& e) {
        return ResponseFactory::bad_request("JSON parse error: " + e.detail);
    }

    Response to_http_response(const FormParseError& e) {
        return ResponseFactory::bad_request("Form parse error: " + e.detail);
    }

    Response to_http_response(const MultipartParseError& e) {
        return ResponseFactory::bad_request("Multipart parse error: " + e.detail);
    }

    Response to_http_response(const BodyLimitExceeded& e) {
        std::ostringstream os;
        os << "Body Limit Exceeded (" << e.limit << " bytes)";
        return ResponseFactory::payload_too_large(os.str());
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Wire source**

```cmake
target_sources(${DMP_HTTP}.Types PRIVATE
        errors/errors.cpp
)
```

- [ ] **Step 6: Build + run tests — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -20
```

Expected: all 36 tests pass.

- [ ] **Step 7: Commit**

```bash
git add components/http/types/errors/errors.cpp \
        tests/unit_tests/http/types/test_errors.cpp \
        components/http/types/CMakeLists.txt \
        tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): implement to_http_response for all built-in errors

Each built-in error type now has its ADL conversion. Routes through
ResponseFactory so status codes, Content-Type, and (for 405) Allow
header stay consistent with handler-built responses."
```

---

## Task 11: Body buffered helpers

**Files:**
- Modify: `components/http/types/body/body.cpp` (add helper definitions)
- Modify: `tests/unit_tests/http/types/test_body.cpp` (add tests for helpers)

**Goal:** Implement `read_to_string`, `read_json`, `read_form`, `read_multipart` on `Body` using `read_chunk` in a loop with a limit. Each returns `AsyncOutcome<T, ...>` for typed failure.

- [ ] **Step 1: Append failing tests to `test_body.cpp`**

Append to the bottom of `tests/unit_tests/http/types/test_body.cpp`:

```cpp
TEST(BodyTest, ReadToStringSucceeds) {
    StringBody body{"hello"};
    auto outcome = run_awaitable(body.read_to_string(100));
    ASSERT_TRUE(outcome.is_success());
    EXPECT_EQ(outcome.value(), "hello");
}

TEST(BodyTest, ReadToStringExceedsLimitReturnsError) {
    StringBody body{"hello"};
    auto outcome = run_awaitable(body.read_to_string(3));   // limit < body
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<BodyLimitExceeded>());
    EXPECT_EQ(outcome.error<BodyLimitExceeded>().limit, 3u);
}

TEST(BodyTest, ReadJsonSucceeds) {
    StringBody body{R"({"a":1,"b":"two"})"};
    auto outcome = run_awaitable(body.read_json(1024));
    ASSERT_TRUE(outcome.is_success());
    EXPECT_EQ(outcome.value()["a"].asInt(), 1);
    EXPECT_EQ(outcome.value()["b"].asString(), "two");
}

TEST(BodyTest, ReadJsonMalformedReturnsError) {
    StringBody body{"not json"};
    auto outcome = run_awaitable(body.read_json(1024));
    ASSERT_TRUE(outcome.is_error());
    EXPECT_TRUE(outcome.holds_error<JsonParseError>());
}

TEST(BodyTest, ReadFormUrlDecodesValues) {
    StringBody body{"name=John%20Doe&city=New+York&empty="};
    auto outcome = run_awaitable(body.read_form(1024));
    ASSERT_TRUE(outcome.is_success());
    auto& form = outcome.value();
    EXPECT_EQ(form.at("name"), "John Doe");
    EXPECT_EQ(form.at("city"), "New York");
    EXPECT_EQ(form.at("empty"), "");
}

TEST(BodyTest, ReadFormMalformedEntryIsError) {
    StringBody body{"=value"};   // empty key
    auto outcome = run_awaitable(body.read_form(1024));
    ASSERT_TRUE(outcome.is_error());
    EXPECT_TRUE(outcome.holds_error<FormParseError>());
}
```

(Multipart tests are deferred — implementing the parser fully is its own task that won't fit a single bite-sized step. We add a minimal stub-test that exercises the shape.)

```cpp
TEST(BodyTest, ReadMultipartReturnsErrorWithoutBoundary) {
    StringBody body{"some-multipart-content"};
    auto outcome = run_awaitable(body.read_multipart(1024, ""));
    ASSERT_TRUE(outcome.is_error());
    EXPECT_TRUE(outcome.holds_error<MultipartParseError>());
}
```

- [ ] **Step 2: Build — expect link failure** referencing the four helpers.

- [ ] **Step 3: Append helper definitions to `components/http/types/body/body.cpp`**

```cpp
#include <sstream>

#include "../response_factory/response_factory.hpp"

namespace demiplane::http {

    namespace {
        // RFC 3986 percent-decoding + form '+' → space mapping.
        // Returns nullopt on malformed escapes so the caller can
        // surface a typed parse error.
        std::optional<std::string> url_decode(std::string_view in,
                                              bool plus_is_space = true) {
            std::string out;
            out.reserve(in.size());
            for (std::size_t i = 0; i < in.size(); ++i) {
                char c = in[i];
                if (c == '+' && plus_is_space) {
                    out.push_back(' ');
                } else if (c == '%') {
                    if (i + 2 >= in.size()) return std::nullopt;
                    auto hex = [](char x) -> int {
                        if (x >= '0' && x <= '9') return x - '0';
                        if (x >= 'a' && x <= 'f') return 10 + x - 'a';
                        if (x >= 'A' && x <= 'F') return 10 + x - 'A';
                        return -1;
                    };
                    int hi = hex(in[i + 1]);
                    int lo = hex(in[i + 2]);
                    if (hi < 0 || lo < 0) return std::nullopt;
                    out.push_back(static_cast<char>((hi << 4) | lo));
                    i += 2;
                } else {
                    out.push_back(c);
                }
            }
            return out;
        }

        // Drains a Body into a single std::string with limit enforcement.
        boost::asio::awaitable<gears::Outcome<std::string, BodyLimitExceeded>>
        drain_to_string(Body& body, std::size_t limit) {
            std::string out;
            while (true) {
                auto chunk = co_await body.read_chunk();
                if (!chunk) break;
                if (out.size() + chunk->size() > limit) {
                    co_return gears::err(BodyLimitExceeded{limit});
                }
                out.append(reinterpret_cast<const char*>(chunk->data()),
                           chunk->size());
            }
            co_return out;
        }
    }

    AsyncOutcome<std::string, BodyLimitExceeded>
    Body::read_to_string(std::size_t limit) {
        co_return co_await drain_to_string(*this, limit);
    }

    AsyncOutcome<Json::Value, JsonParseError, BodyLimitExceeded>
    Body::read_json(std::size_t limit) {
        auto drained = co_await drain_to_string(*this, limit);
        if (!drained.is_success()) {
            co_return gears::err(drained.error<BodyLimitExceeded>());
        }

        Json::Value root;
        std::string err_msg;
        Json::CharReaderBuilder builder;
        std::istringstream stream{std::move(drained).value()};
        if (!Json::parseFromStream(builder, stream, &root, &err_msg)) {
            co_return gears::err(JsonParseError{std::move(err_msg)});
        }
        co_return root;
    }

    AsyncOutcome<std::unordered_map<std::string, std::string>,
                 FormParseError, BodyLimitExceeded>
    Body::read_form(std::size_t limit) {
        auto drained = co_await drain_to_string(*this, limit);
        if (!drained.is_success()) {
            co_return gears::err(drained.error<BodyLimitExceeded>());
        }
        std::string body = std::move(drained).value();

        std::unordered_map<std::string, std::string> out;
        std::size_t i = 0;
        while (i < body.size()) {
            std::size_t amp = body.find('&', i);
            std::string_view pair{body.data() + i,
                                  amp == std::string::npos ? body.size() - i : amp - i};
            std::size_t eq = pair.find('=');

            std::string_view raw_key   = (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
            std::string_view raw_value = (eq == std::string_view::npos) ? std::string_view{}
                                                                        : pair.substr(eq + 1);
            if (raw_key.empty()) {
                co_return gears::err(FormParseError{"empty key in body"});
            }

            auto key = url_decode(raw_key);
            auto val = url_decode(raw_value);
            if (!key || !val) {
                co_return gears::err(FormParseError{"invalid percent-escape"});
            }
            out[*std::move(key)] = *std::move(val);

            if (amp == std::string::npos) break;
            i = amp + 1;
        }
        co_return out;
    }

    AsyncOutcome<std::vector<MultipartField>,
                 MultipartParseError, BodyLimitExceeded>
    Body::read_multipart(std::size_t limit, std::string_view boundary) {
        if (boundary.empty()) {
            co_return gears::err(MultipartParseError{"empty boundary"});
        }

        auto drained = co_await drain_to_string(*this, limit);
        if (!drained.is_success()) {
            co_return gears::err(drained.error<BodyLimitExceeded>());
        }
        const std::string body = std::move(drained).value();

        // Split on "--<boundary>" delimiters.
        const std::string delim = "--" + std::string{boundary};
        std::vector<MultipartField> out;

        std::size_t pos = body.find(delim);
        if (pos == std::string::npos) {
            co_return gears::err(MultipartParseError{"no boundary found"});
        }
        pos += delim.size();
        // After first boundary expect CRLF.
        while (pos < body.size()) {
            // End boundary is "--<boundary>--"
            if (pos + 2 <= body.size() && body[pos] == '-' && body[pos + 1] == '-') {
                break;   // end of multipart
            }
            // Skip CRLF after boundary
            if (pos + 1 < body.size() && body[pos] == '\r' && body[pos + 1] == '\n') {
                pos += 2;
            }
            // Parse part headers up to blank line
            std::size_t header_end = body.find("\r\n\r\n", pos);
            if (header_end == std::string::npos) {
                co_return gears::err(MultipartParseError{"unterminated part headers"});
            }

            std::string_view header_block{body.data() + pos, header_end - pos};
            std::size_t body_start = header_end + 4;
            std::size_t next = body.find("\r\n" + delim, body_start);
            if (next == std::string::npos) {
                co_return gears::err(MultipartParseError{"unterminated part body"});
            }
            std::string_view part_body{body.data() + body_start, next - body_start};

            MultipartField field;
            // Parse Content-Disposition / Content-Type from header block.
            std::size_t hp = 0;
            while (hp < header_block.size()) {
                std::size_t eol = header_block.find("\r\n", hp);
                std::string_view line{header_block.data() + hp,
                                      (eol == std::string_view::npos
                                          ? header_block.size() - hp
                                          : eol - hp)};
                if (line.starts_with("Content-Disposition:") ||
                    line.starts_with("content-disposition:")) {
                    auto find_param = [&](std::string_view key) -> std::string {
                        auto p = line.find(key);
                        if (p == std::string_view::npos) return {};
                        p += key.size();
                        if (p < line.size() && line[p] == '=') ++p;
                        if (p < line.size() && line[p] == '"') {
                            ++p;
                            auto e = line.find('"', p);
                            if (e == std::string_view::npos) return {};
                            return std::string{line.substr(p, e - p)};
                        }
                        return {};
                    };
                    field.name     = find_param("name");
                    field.filename = find_param("filename");
                } else if (line.starts_with("Content-Type:") ||
                           line.starts_with("content-type:")) {
                    auto colon = line.find(':');
                    if (colon != std::string_view::npos) {
                        auto v = line.substr(colon + 1);
                        while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.remove_prefix(1);
                        field.content_type = std::string{v};
                    }
                }
                if (eol == std::string_view::npos) break;
                hp = eol + 2;
            }

            field.value = std::string{part_body};
            out.emplace_back(std::move(field));

            pos = next + 2 + delim.size();   // skip "\r\n--<boundary>"
        }

        co_return out;
    }

}  // namespace demiplane::http
```

(The multipart parser above is adequate for v1: handles the well-formed common case, rejects malformed inputs with a typed error. Edge cases like base64 transfer-encoded parts are out of scope.)

- [ ] **Step 4: Build + run tests — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -25
```

Expected: all 43 tests pass.

- [ ] **Step 5: Commit**

```bash
git add components/http/types/body/body.cpp \
        tests/unit_tests/http/types/test_body.cpp
git commit -m "feat(http/types): implement Body buffered helpers

read_to_string, read_json, read_form, read_multipart drain read_chunk()
with a size limit. URL-decoding fixed (form data + path params; '+' → space,
%XX honored). Multipart parser handles the well-formed common case;
malformed input surfaces MultipartParseError. All return AsyncOutcome
with typed failure (BodyLimitExceeded, JsonParseError, FormParseError,
MultipartParseError)."
```

---

## Task 12: RequestContext — basic accessors + content-type predicates

**Files:**
- Create: `components/http/types/request_context/request_context.hpp`
- Create: `components/http/types/request_context/request_context.cpp`
- Create: `tests/unit_tests/http/types/test_request_context.cpp`
- Modify: `components/http/types/CMakeLists.txt`
- Modify: `tests/unit_tests/CMakeLists.txt`

**Goal:** Construction, header lookup, body access, content-type predicates. Path/query params and the type-keyed bag land in Task 13.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/types/test_request_context.cpp`:

```cpp
#include <memory>
#include <memory_resource>
#include <string>

#include <gtest/gtest.h>

#include <body/body.hpp>
#include <headers/headers.hpp>
#include <request/request.hpp>
#include <request_context/request_context.hpp>

using namespace demiplane::http;

class RequestContextTest : public ::testing::Test {
protected:
    std::pmr::monotonic_buffer_resource resource_{4096};
    std::pmr::polymorphic_allocator<> alloc_{&resource_};

    Request make_request(HttpMethod m, std::string target_str,
                         std::vector<std::pair<std::string,std::string>> hdrs = {},
                         std::string body_text = "") {
        Request req;
        req.method  = m;
        req.target  = std::move(target_str);
        req.version = HttpVersion::http_1_1;
        req.headers = Headers::owned(alloc_);
        for (auto const& [k, v] : hdrs) req.headers.add(k, v);
        if (body_text.empty()) {
            req.body = std::make_unique<EmptyBody>();
        } else {
            req.body = std::make_unique<StringBody>(std::move(body_text));
        }
        return req;
    }
};

TEST_F(RequestContextTest, MethodAndTarget) {
    auto req = make_request(HttpMethod::get, "/users");
    RequestContext ctx{std::move(req), alloc_};
    EXPECT_EQ(ctx.method(), HttpMethod::get);
    EXPECT_EQ(ctx.target(), "/users");
    EXPECT_EQ(ctx.version(), HttpVersion::http_1_1);
}

TEST_F(RequestContextTest, HeadersLazy) {
    auto req = make_request(HttpMethod::get, "/", {{"Host", "example.com"}});
    RequestContext ctx{std::move(req), alloc_};
    auto host = ctx.header("host");
    ASSERT_TRUE(host.has_value());
    EXPECT_EQ(*host, "example.com");
    EXPECT_FALSE(ctx.header("missing").has_value());
}

TEST_F(RequestContextTest, BodyAccess) {
    auto req = make_request(HttpMethod::post, "/", {}, "hello");
    RequestContext ctx{std::move(req), alloc_};
    EXPECT_EQ(ctx.body().size_hint().value_or(0), 5u);
}

TEST_F(RequestContextTest, ContentTypePredicates_Json) {
    auto req = make_request(HttpMethod::post, "/",
        {{"Content-Type", "application/json"}});
    RequestContext ctx{std::move(req), alloc_};
    EXPECT_TRUE(ctx.is_json());
    EXPECT_FALSE(ctx.is_form());
    EXPECT_FALSE(ctx.is_multipart());
}

TEST_F(RequestContextTest, ContentTypePredicates_Form) {
    auto req = make_request(HttpMethod::post, "/",
        {{"Content-Type", "application/x-www-form-urlencoded"}});
    RequestContext ctx{std::move(req), alloc_};
    EXPECT_FALSE(ctx.is_json());
    EXPECT_TRUE(ctx.is_form());
    EXPECT_FALSE(ctx.is_multipart());
}

TEST_F(RequestContextTest, ContentTypePredicates_Multipart) {
    auto req = make_request(HttpMethod::post, "/",
        {{"Content-Type", "multipart/form-data; boundary=---xx"}});
    RequestContext ctx{std::move(req), alloc_};
    EXPECT_FALSE(ctx.is_json());
    EXPECT_FALSE(ctx.is_form());
    EXPECT_TRUE(ctx.is_multipart());
}

TEST_F(RequestContextTest, AcceptsJsonOrHtml) {
    auto req_json = make_request(HttpMethod::get, "/",
        {{"Accept", "application/json, text/html;q=0.9"}});
    RequestContext ctx_json{std::move(req_json), alloc_};
    EXPECT_TRUE(ctx_json.accepts_json());
    EXPECT_TRUE(ctx_json.accepts_html());

    auto req_wild = make_request(HttpMethod::get, "/", {{"Accept", "*/*"}});
    RequestContext ctx_wild{std::move(req_wild), alloc_};
    EXPECT_TRUE(ctx_wild.accepts_json());
    EXPECT_TRUE(ctx_wild.accepts_html());
}

TEST_F(RequestContextTest, PathOnlySplitsTarget) {
    auto req = make_request(HttpMethod::get, "/users/42?q=foo&p=bar");
    RequestContext ctx{std::move(req), alloc_};
    EXPECT_EQ(ctx.path(),         "/users/42");
    EXPECT_EQ(ctx.query_string(), "q=foo&p=bar");
}

TEST_F(RequestContextTest, PathWithoutQuery) {
    auto req = make_request(HttpMethod::get, "/users/42");
    RequestContext ctx{std::move(req), alloc_};
    EXPECT_EQ(ctx.path(),         "/users/42");
    EXPECT_EQ(ctx.query_string(), "");
}
```

- [ ] **Step 2: Append to test sources**

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Types
        http/types/test_http_enums.cpp
        http/types/test_headers.cpp
        http/types/test_body.cpp
        http/types/test_response.cpp
        http/types/test_response_factory.cpp
        http/types/test_errors.cpp
        http/types/test_request_context.cpp
)
```

- [ ] **Step 3: Build — expect failure**

Expected: `request_context/request_context.hpp: No such file or directory`.

- [ ] **Step 4: Create `components/http/types/request_context/request_context.hpp`**

```cpp
#pragma once

#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <typeindex>

#include <boost/container/small_vector.hpp>

#include "../body/body.hpp"
#include "../headers/headers.hpp"
#include "../http_enums.hpp"
#include "../request/request.hpp"

namespace demiplane::http {

    /**
     * @brief Handler-facing view of one in-flight request.
     *
     * Owns the Request value moved in by the driver, plus arena-backed
     * containers for path/query params (Task 13) and middleware-set values
     * (Task 14). Header lookup is lazy — no eager parse on construction.
     *
     * RequestContext is move-only; passed by value into handlers.
     */
    class RequestContext {
    public:
        RequestContext(Request req, std::pmr::polymorphic_allocator<> alloc);

        RequestContext(const RequestContext&)            = delete;
        RequestContext& operator=(const RequestContext&) = delete;
        RequestContext(RequestContext&&)                 = default;
        RequestContext& operator=(RequestContext&&)      = default;

        // ── Request data ─────────────────────────────────────────────────
        HttpMethod  method()  const noexcept { return request_.method; }
        HttpVersion version() const noexcept { return request_.version; }
        std::string_view target() const noexcept { return request_.target; }
        std::string_view path()         const;
        std::string_view query_string() const;
        const Headers& headers() const noexcept { return request_.headers; }
        Body& body() { return *request_.body; }

        // ── Header convenience ───────────────────────────────────────────
        std::optional<std::string_view> header(std::string_view name) const {
            return request_.headers.get(name);
        }
        std::string header_or(std::string_view name,
                              std::string_view fallback) const {
            return request_.headers.get_or(name, fallback);
        }

        // ── Content-type / Accept predicates ─────────────────────────────
        bool is_json()      const;
        bool is_form()      const;
        bool is_multipart() const;
        bool accepts_json() const;
        bool accepts_html() const;

        // ── Arena access (for handler-allocated short-lived data) ────────
        std::pmr::polymorphic_allocator<> arena_alloc() const noexcept {
            return alloc_;
        }

    private:
        Request request_;
        std::pmr::polymorphic_allocator<> alloc_;

        // Lazy split of target into (path, query). Memoized on first access.
        mutable std::optional<std::string_view> cached_path_;
        mutable std::optional<std::string_view> cached_query_;
        void ensure_path_split() const;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/types/request_context/request_context.cpp`**

```cpp
#include "request_context.hpp"

#include <utility>

namespace demiplane::http {

    RequestContext::RequestContext(Request req,
                                   std::pmr::polymorphic_allocator<> alloc)
        : request_{std::move(req)}, alloc_{alloc} {}

    void RequestContext::ensure_path_split() const {
        if (cached_path_.has_value()) return;
        std::string_view t = request_.target;
        auto q = t.find('?');
        if (q == std::string_view::npos) {
            cached_path_  = t;
            cached_query_ = std::string_view{};
        } else {
            cached_path_  = t.substr(0, q);
            cached_query_ = t.substr(q + 1);
        }
    }

    std::string_view RequestContext::path() const {
        ensure_path_split();
        return *cached_path_;
    }

    std::string_view RequestContext::query_string() const {
        ensure_path_split();
        return *cached_query_;
    }

    namespace {
        bool contains(std::string_view haystack, std::string_view needle) {
            return haystack.find(needle) != std::string_view::npos;
        }
    }

    bool RequestContext::is_json() const {
        auto ct = header("content-type");
        return ct && contains(*ct, "application/json");
    }

    bool RequestContext::is_form() const {
        auto ct = header("content-type");
        return ct && contains(*ct, "application/x-www-form-urlencoded");
    }

    bool RequestContext::is_multipart() const {
        auto ct = header("content-type");
        return ct && contains(*ct, "multipart/form-data");
    }

    bool RequestContext::accepts_json() const {
        auto a = header("accept");
        return a && (contains(*a, "application/json") || contains(*a, "*/*"));
    }

    bool RequestContext::accepts_html() const {
        auto a = header("accept");
        return a && (contains(*a, "text/html") || contains(*a, "*/*"));
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Wire source**

```cmake
target_sources(${DMP_HTTP}.Types PRIVATE
        request_context/request_context.cpp
)
```

- [ ] **Step 7: Build + run tests — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -20
```

Expected: all 50 tests pass.

- [ ] **Step 8: Commit**

```bash
git add components/http/types/request_context \
        tests/unit_tests/http/types/test_request_context.cpp \
        components/http/types/CMakeLists.txt \
        tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add RequestContext basic accessors + predicates

Method/target/version/headers/body access via the moved-in Request.
Lazy path/query split memoized on first access. Content-Type and Accept
predicates (is_json/is_form/is_multipart/accepts_json/accepts_html)."
```

---

## Task 13: RequestContext — path/query params with URL decoding

**Files:**
- Modify: `components/http/types/request_context/request_context.hpp`
- Modify: `components/http/types/request_context/request_context.cpp`
- Modify: `tests/unit_tests/http/types/test_request_context.cpp`

**Goal:** Add `path_param<T>(name)`, `path_param_or<T>(name, fallback)`, `query<T>(name)`, `query_or<T>(name, fallback)`. Path params are set by the routing layer (PR 2) — `RequestContext` exposes a `set_path_params` mutator. Query params are auto-parsed lazily from `query_string()` on first access. URL decoding applied throughout.

- [ ] **Step 1: Write failing tests**

Append to `tests/unit_tests/http/types/test_request_context.cpp`:

```cpp
TEST_F(RequestContextTest, QueryParamUrlDecoded) {
    auto req = make_request(HttpMethod::get, "/?name=John%20Doe&n=42&flag=on");
    RequestContext ctx{std::move(req), alloc_};
    EXPECT_EQ(ctx.query<std::string>("name").value_or(""), "John Doe");
    EXPECT_EQ(ctx.query<int>("n").value_or(0), 42);
    EXPECT_EQ(ctx.query<std::string>("flag").value_or(""), "on");
    EXPECT_FALSE(ctx.query<int>("missing").has_value());
}

TEST_F(RequestContextTest, QueryOrFallback) {
    auto req = make_request(HttpMethod::get, "/?n=10");
    RequestContext ctx{std::move(req), alloc_};
    EXPECT_EQ(ctx.query_or<int>("n", 99), 10);
    EXPECT_EQ(ctx.query_or<int>("missing", 99), 99);
}

TEST_F(RequestContextTest, QueryPlusBecomesSpace) {
    auto req = make_request(HttpMethod::get, "/?city=New+York");
    RequestContext ctx{std::move(req), alloc_};
    EXPECT_EQ(ctx.query<std::string>("city").value_or(""), "New York");
}

TEST_F(RequestContextTest, PathParamSetExternally) {
    auto req = make_request(HttpMethod::get, "/users/42");
    RequestContext ctx{std::move(req), alloc_};
    ctx.set_path_param("id", "42");
    EXPECT_EQ(ctx.path_param<int>("id").value_or(0), 42);
    EXPECT_EQ(ctx.path_param_or<std::string>("id", "fallback"), "42");
    EXPECT_FALSE(ctx.path_param<int>("missing").has_value());
}

TEST_F(RequestContextTest, PathParamConvertFailure) {
    auto req = make_request(HttpMethod::get, "/users/abc");
    RequestContext ctx{std::move(req), alloc_};
    ctx.set_path_param("id", "abc");
    EXPECT_FALSE(ctx.path_param<int>("id").has_value());
    EXPECT_EQ(ctx.path_param<std::string>("id").value_or(""), "abc");
}
```

- [ ] **Step 2: Build — expect failure** (the new methods don't exist yet).

- [ ] **Step 3: Add methods to `request_context.hpp`**

Insert into the `RequestContext` class public section:

```cpp
        // ── Path parameters (set by routing layer) ───────────────────────
        template <typename T>
        std::optional<T> path_param(std::string_view name) const;

        template <typename T>
        T path_param_or(std::string_view name, T fallback) const;

        /// Set a single path parameter. Called by the routing layer after
        /// pattern match; stores the (already URL-decoded) value in the
        /// arena.
        void set_path_param(std::string_view name, std::string_view value);

        // ── Query parameters (auto-parsed lazily from query_string) ──────
        template <typename T>
        std::optional<T> query(std::string_view name) const;

        template <typename T>
        T query_or(std::string_view name, T fallback) const;
```

And in the private section, add storage and the parser hook:

```cpp
        using ParamEntry = std::pair<std::pmr::string, std::pmr::string>;
        boost::container::small_vector<ParamEntry, 4,
            std::pmr::polymorphic_allocator<ParamEntry>> path_params_{
                std::pmr::polymorphic_allocator<ParamEntry>{alloc_}};

        mutable bool query_parsed_ = false;
        mutable boost::container::small_vector<ParamEntry, 4,
            std::pmr::polymorphic_allocator<ParamEntry>> query_params_{
                std::pmr::polymorphic_allocator<ParamEntry>{alloc_}};

        void ensure_query_parsed() const;
        std::optional<std::string_view> raw_query(std::string_view name) const;
        std::optional<std::string_view> raw_path_param(std::string_view name) const;

        template <typename T>
        static std::optional<T> convert_string(std::string_view value);
```

(The `boost::container::small_vector` initializer-with-allocator pattern needs `boost/container/small_vector.hpp` — header is already included via `request_context.hpp` since we added it. Don't forget to update the include list.)

Add `#include <boost/container/small_vector.hpp>` to the includes at the top of `request_context.hpp`.

- [ ] **Step 4: Add template specializations and helper definitions to `request_context.cpp`**

Append to `components/http/types/request_context/request_context.cpp`:

```cpp
#include <charconv>
#include <type_traits>

namespace demiplane::http {

    namespace {
        // Local copy of the URL decoder from body.cpp. Kept private to this
        // TU so we don't expose a public symbol; can extract to a shared
        // helper in PR 2 if a third caller appears.
        std::optional<std::string> url_decode(std::string_view in,
                                              bool plus_is_space = true) {
            std::string out;
            out.reserve(in.size());
            for (std::size_t i = 0; i < in.size(); ++i) {
                char c = in[i];
                if (c == '+' && plus_is_space) {
                    out.push_back(' ');
                } else if (c == '%') {
                    if (i + 2 >= in.size()) return std::nullopt;
                    auto hex = [](char x) -> int {
                        if (x >= '0' && x <= '9') return x - '0';
                        if (x >= 'a' && x <= 'f') return 10 + x - 'a';
                        if (x >= 'A' && x <= 'F') return 10 + x - 'A';
                        return -1;
                    };
                    int hi = hex(in[i + 1]);
                    int lo = hex(in[i + 2]);
                    if (hi < 0 || lo < 0) return std::nullopt;
                    out.push_back(static_cast<char>((hi << 4) | lo));
                    i += 2;
                } else {
                    out.push_back(c);
                }
            }
            return out;
        }
    }

    void RequestContext::ensure_query_parsed() const {
        if (query_parsed_) return;
        query_parsed_ = true;
        std::string_view qs = query_string();
        std::size_t i = 0;
        while (i < qs.size()) {
            std::size_t amp = qs.find('&', i);
            std::string_view pair{qs.data() + i,
                                  amp == std::string_view::npos ? qs.size() - i : amp - i};
            std::size_t eq = pair.find('=');
            std::string_view raw_k = (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
            std::string_view raw_v = (eq == std::string_view::npos) ? std::string_view{}
                                                                    : pair.substr(eq + 1);

            auto k = url_decode(raw_k);
            auto v = url_decode(raw_v);
            if (k && v) {
                std::pmr::string key{*k, alloc_};
                std::pmr::string val{*v, alloc_};
                query_params_.emplace_back(std::move(key), std::move(val));
            }
            // Malformed escapes are silently dropped at this layer; if a
            // handler wants strict parsing it should call body().read_form().

            if (amp == std::string_view::npos) break;
            i = amp + 1;
        }
    }

    std::optional<std::string_view>
    RequestContext::raw_query(std::string_view name) const {
        ensure_query_parsed();
        for (const auto& [k, v] : query_params_) {
            if (std::string_view(k) == name) {
                return std::string_view(v);
            }
        }
        return std::nullopt;
    }

    std::optional<std::string_view>
    RequestContext::raw_path_param(std::string_view name) const {
        for (const auto& [k, v] : path_params_) {
            if (std::string_view(k) == name) {
                return std::string_view(v);
            }
        }
        return std::nullopt;
    }

    void RequestContext::set_path_param(std::string_view name,
                                        std::string_view value) {
        path_params_.emplace_back(
            std::pmr::string{name,  alloc_},
            std::pmr::string{value, alloc_});
    }

    template <typename T>
    std::optional<T> RequestContext::convert_string(std::string_view value) {
        if constexpr (std::is_same_v<T, std::string>) {
            return std::string{value};
        } else if constexpr (std::is_same_v<T, std::string_view>) {
            return value;
        } else if constexpr (std::is_integral_v<T>) {
            T out{};
            auto [ptr, ec] = std::from_chars(value.data(),
                                              value.data() + value.size(), out);
            if (ec != std::errc{} || ptr != value.data() + value.size()) {
                return std::nullopt;
            }
            return out;
        } else if constexpr (std::is_floating_point_v<T>) {
            T out{};
            auto [ptr, ec] = std::from_chars(value.data(),
                                              value.data() + value.size(), out);
            if (ec != std::errc{} || ptr != value.data() + value.size()) {
                return std::nullopt;
            }
            return out;
        } else {
            static_assert(!std::is_same_v<T, T>,
                "RequestContext::convert_string: unsupported target type");
        }
    }

    template <typename T>
    std::optional<T> RequestContext::path_param(std::string_view name) const {
        auto raw = raw_path_param(name);
        if (!raw) return std::nullopt;
        return convert_string<T>(*raw);
    }

    template <typename T>
    T RequestContext::path_param_or(std::string_view name, T fallback) const {
        if (auto v = path_param<T>(name)) return *std::move(v);
        return fallback;
    }

    template <typename T>
    std::optional<T> RequestContext::query(std::string_view name) const {
        auto raw = raw_query(name);
        if (!raw) return std::nullopt;
        return convert_string<T>(*raw);
    }

    template <typename T>
    T RequestContext::query_or(std::string_view name, T fallback) const {
        if (auto v = query<T>(name)) return *std::move(v);
        return fallback;
    }

    // Explicit instantiations — the same set used by the existing
    // controllers + tests. Adding a new T means: add a line here.
    template std::optional<int>           RequestContext::path_param<int>(std::string_view) const;
    template std::optional<long>          RequestContext::path_param<long>(std::string_view) const;
    template std::optional<long long>     RequestContext::path_param<long long>(std::string_view) const;
    template std::optional<double>        RequestContext::path_param<double>(std::string_view) const;
    template std::optional<std::string>   RequestContext::path_param<std::string>(std::string_view) const;

    template int           RequestContext::path_param_or<int>(std::string_view, int) const;
    template long          RequestContext::path_param_or<long>(std::string_view, long) const;
    template long long     RequestContext::path_param_or<long long>(std::string_view, long long) const;
    template double        RequestContext::path_param_or<double>(std::string_view, double) const;
    template std::string   RequestContext::path_param_or<std::string>(std::string_view, std::string) const;

    template std::optional<int>           RequestContext::query<int>(std::string_view) const;
    template std::optional<long>          RequestContext::query<long>(std::string_view) const;
    template std::optional<long long>     RequestContext::query<long long>(std::string_view) const;
    template std::optional<double>        RequestContext::query<double>(std::string_view) const;
    template std::optional<std::string>   RequestContext::query<std::string>(std::string_view) const;

    template int           RequestContext::query_or<int>(std::string_view, int) const;
    template long          RequestContext::query_or<long>(std::string_view, long) const;
    template long long     RequestContext::query_or<long long>(std::string_view, long long) const;
    template double        RequestContext::query_or<double>(std::string_view, double) const;
    template std::string   RequestContext::query_or<std::string>(std::string_view, std::string) const;

}  // namespace demiplane::http
```

- [ ] **Step 5: Build + run tests — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -25
```

Expected: all 55 tests pass.

- [ ] **Step 6: Commit**

```bash
git add components/http/types/request_context \
        tests/unit_tests/http/types/test_request_context.cpp
git commit -m "feat(http/types): add path_param + query with URL decoding

Path params set by the (future) routing layer via set_path_param. Query
params parsed lazily on first access. URL-decoded everywhere — '+' is
space, %XX honored. Typed conversion via std::from_chars (int/long/double/
string). Storage is arena-backed boost::small_vector so 0-3 params have
no heap traffic."
```

---

## Task 14: RequestContext — type-keyed middleware data bag

**Files:**
- Modify: `components/http/types/request_context/request_context.hpp`
- Modify: `components/http/types/request_context/request_context.cpp`
- Modify: `tests/unit_tests/http/types/test_request_context.cpp`

**Goal:** `set<T>(value)` / `get<T>()` / `has<T>()` for middleware-set values flowing into handlers. Type-keyed via `std::type_index`. Storage is arena-backed; values are placement-newed into the arena.

- [ ] **Step 1: Write failing tests**

Append to `tests/unit_tests/http/types/test_request_context.cpp`:

```cpp
struct TraceId  { std::string value; };
struct UserPrincipal { int id; std::string name; };

TEST_F(RequestContextTest, BagSetAndGet) {
    auto req = make_request(HttpMethod::get, "/");
    RequestContext ctx{std::move(req), alloc_};

    EXPECT_FALSE(ctx.has<TraceId>());
    EXPECT_EQ(ctx.get<TraceId>(), nullptr);

    ctx.set<TraceId>(TraceId{"abc-123"});
    ASSERT_TRUE(ctx.has<TraceId>());
    auto* tid = ctx.get<TraceId>();
    ASSERT_NE(tid, nullptr);
    EXPECT_EQ(tid->value, "abc-123");
}

TEST_F(RequestContextTest, BagDifferentTypesCoexist) {
    auto req = make_request(HttpMethod::get, "/");
    RequestContext ctx{std::move(req), alloc_};
    ctx.set<TraceId>(TraceId{"abc"});
    ctx.set<UserPrincipal>(UserPrincipal{42, "alice"});

    auto* tid = ctx.get<TraceId>();
    auto* up  = ctx.get<UserPrincipal>();
    ASSERT_NE(tid, nullptr);
    ASSERT_NE(up,  nullptr);
    EXPECT_EQ(tid->value, "abc");
    EXPECT_EQ(up->name,   "alice");
    EXPECT_EQ(up->id,     42);
}

TEST_F(RequestContextTest, BagSetReplaces) {
    auto req = make_request(HttpMethod::get, "/");
    RequestContext ctx{std::move(req), alloc_};
    ctx.set<TraceId>(TraceId{"first"});
    ctx.set<TraceId>(TraceId{"second"});
    auto* tid = ctx.get<TraceId>();
    ASSERT_NE(tid, nullptr);
    EXPECT_EQ(tid->value, "second");
}
```

- [ ] **Step 2: Build — expect failure**.

- [ ] **Step 3: Add bag methods to `request_context.hpp`**

Add an include at the top:

```cpp
#include <typeindex>
```

Insert into the public section of `RequestContext`:

```cpp
        // ── Type-keyed middleware data bag ───────────────────────────────
        // Middleware sets values; later middleware and the handler get them.
        // Storage is arena-backed so the bag itself doesn't heap-allocate
        // for typical 0-4 entry workloads.

        template <typename T>
        void set(T value);

        template <typename T>
        T* get();

        template <typename T>
        const T* get() const;

        template <typename T>
        bool has() const;
```

Insert into the private section:

```cpp
        struct BagEntry {
            std::type_index key;
            void* ptr;                          // owned by destroyer below
            void (*destroyer)(void*) noexcept;  // calls type's dtor
        };

        boost::container::small_vector<BagEntry, 4,
            std::pmr::polymorphic_allocator<BagEntry>> bag_{
                std::pmr::polymorphic_allocator<BagEntry>{alloc_}};

        BagEntry* find_bag_entry(std::type_index key);
        const BagEntry* find_bag_entry(std::type_index key) const;
        void clear_bag() noexcept;

    public:
        // Bag entries hold dynamically-typed values; their destructors must
        // run when RequestContext dies. Default move semantics handle this
        // correctly because we run destroyers in clear_bag() from the
        // destructor.
        ~RequestContext();
```

(`~RequestContext()` is no longer trivially default — we have to invoke each entry's destroyer. Move ops still work because vector-of-bag is moveable; the moved-from object's `bag_` is empty so its destructor is a no-op.)

- [ ] **Step 4: Add template + helper definitions to `request_context.cpp`**

Append to `components/http/types/request_context/request_context.cpp`:

```cpp
#include <new>

namespace demiplane::http {

    RequestContext::~RequestContext() {
        clear_bag();
    }

    void RequestContext::clear_bag() noexcept {
        for (auto& e : bag_) {
            if (e.destroyer && e.ptr) e.destroyer(e.ptr);
        }
        bag_.clear();
    }

    RequestContext::BagEntry*
    RequestContext::find_bag_entry(std::type_index key) {
        for (auto& e : bag_) if (e.key == key) return &e;
        return nullptr;
    }

    const RequestContext::BagEntry*
    RequestContext::find_bag_entry(std::type_index key) const {
        for (const auto& e : bag_) if (e.key == key) return &e;
        return nullptr;
    }

    template <typename T>
    void RequestContext::set(T value) {
        static_assert(std::is_move_constructible_v<T>,
            "RequestContext::set: T must be move-constructible");

        std::type_index key{typeid(T)};

        if (auto* existing = find_bag_entry(key)) {
            // Replace: destroy old, in-place construct new.
            existing->destroyer(existing->ptr);
            // Allocate fresh storage in arena (the previous storage is
            // arena-bumped — we can't reclaim it, so we leak its bytes
            // until the arena is reset; same approach as std::pmr::string
            // reassignment).
            void* mem = alloc_.allocate_bytes(sizeof(T), alignof(T));
            new (mem) T(std::move(value));
            existing->ptr = mem;
            existing->destroyer = +[](void* p) noexcept {
                static_cast<T*>(p)->~T();
            };
            return;
        }

        void* mem = alloc_.allocate_bytes(sizeof(T), alignof(T));
        new (mem) T(std::move(value));
        bag_.push_back(BagEntry{
            .key = key,
            .ptr = mem,
            .destroyer = +[](void* p) noexcept {
                static_cast<T*>(p)->~T();
            },
        });
    }

    template <typename T>
    T* RequestContext::get() {
        auto* e = find_bag_entry(std::type_index{typeid(T)});
        return e ? static_cast<T*>(e->ptr) : nullptr;
    }

    template <typename T>
    const T* RequestContext::get() const {
        auto* e = find_bag_entry(std::type_index{typeid(T)});
        return e ? static_cast<const T*>(e->ptr) : nullptr;
    }

    template <typename T>
    bool RequestContext::has() const {
        return find_bag_entry(std::type_index{typeid(T)}) != nullptr;
    }

    // Explicit instantiations are NOT used here — set/get/has must work for
    // user-defined types in user code, so the templates must be visible to
    // the compiler when consumers compile their own .cpp files. Move the
    // definitions into request_context.hpp before this commit.

}  // namespace demiplane::http
```

- [ ] **Step 5: Move templated bag methods into the header**

The `set<T>`, `get<T>`, `has<T>` definitions must live in the header (user code instantiates them with user-defined types). Cut from `request_context.cpp` and paste into `request_context.hpp` after the class definition, inside `namespace demiplane::http { ... }`.

The non-templated helpers (`find_bag_entry`, `clear_bag`, `~RequestContext`) stay in the .cpp.

The header version of `set<T>` needs `<new>` for placement new and `<utility>` for `std::move` — add `#include <new>` and `#include <utility>` to the header.

- [ ] **Step 6: Build + run tests — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -20
```

Expected: all 58 tests pass.

- [ ] **Step 7: Commit**

```bash
git add components/http/types/request_context \
        tests/unit_tests/http/types/test_request_context.cpp
git commit -m "feat(http/types): add type-keyed middleware data bag

set<T>(value) / get<T>() / has<T>() — typed access for middleware-set
values flowing into handlers. Storage is arena-bumped boost::small_vector
of (type_index, void*, destroyer) tuples; SBO of 4 entries inline. ~RequestContext
runs destroyers via the destroyer function pointer."
```

---

## Task 15: Final layer integration — verify clean build, lint pass, sanitizer run

**Files:**
- (No source changes; verification only.)

**Goal:** Confirm the Types layer is shipped: builds clean, all tests pass, ASan/UBSan clean (since `RequestContext::set<T>` does manual placement-new).

- [ ] **Step 1: Build the entire project**

```bash
cmake --build build/release -- -j4 2>&1 | tail -20
```

Expected: no warnings or errors. The existing `http_server` library still builds against the unchanged old code.

- [ ] **Step 2: Run all unit tests, not just Http.Types**

```bash
ctest --test-dir build/release --output-on-failure -L unit 2>&1 | tail -30
```

Expected: every existing unit test continues to pass; `Http.Types` adds 58 new tests, all green.

- [ ] **Step 3: Run Http.Types under ASan + UBSan**

```bash
cmake --preset release-sanitize 2>&1 | tail -5 || \
    cmake -B build/asan -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
cmake --build build/asan --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -15
ctest --test-dir build/asan --output-on-failure -L unit -R Http.Types 2>&1 | tail -25
```

Expected: all 58 tests pass under sanitizers. The placement-new + destroyer pattern in the type-keyed bag is the most likely failure mode; ASan would catch a missed destructor or misaligned alloc.

If sanitizers fail, fix the underlying issue (most likely `clear_bag` ordering or alignment in `allocate_bytes`) and retry before committing.

- [ ] **Step 4: Quick lint pass**

If the project uses clang-tidy or similar:

```bash
find components/http/types -name '*.cpp' -o -name '*.hpp' | \
    xargs clang-tidy -p build/release --quiet 2>&1 | head -50
```

Expected: no new warnings beyond what's already in the project. Fix any new warnings before commit.

- [ ] **Step 5: Verify no .cpp references missing files**

```bash
ls components/http/types/types_placeholder.cpp 2>/dev/null && {
    git rm components/http/types/types_placeholder.cpp
    sed -i '/types_placeholder.cpp/d' components/http/types/CMakeLists.txt
    cmake --build build/release --target Demiplane.Component.HTTP.Types -- -j4 | tail -5
}
```

The placeholder TU created in Task 1 isn't needed anymore — every layer now has real source files. Remove it.

- [ ] **Step 6: Final commit (if step 5 changed anything)**

```bash
git add -u components/http/types/CMakeLists.txt components/http/types/
git commit -m "chore(http/types): drop bootstrap placeholder TU

Every Types subdirectory now has real source. The placeholder created
in PR 1's bootstrap is no longer needed."
```

If step 5 was a no-op (placeholder already removed), skip this commit.

---

## Self-Review

Spec coverage:

- §5.1 Headers — Tasks 5.
- §5.2 Body (streaming + buffered helpers) — Tasks 6 + 11.
- §5.3 Request — Task 7. §5.3 Response — Task 8.
- §5.4 RequestContext (lazy headers, body access, content-type predicates, path/query params w/ URL decode, type-keyed bag) — Tasks 12 + 13 + 14.
- §5.5 errors.hpp + ADL — Tasks 4 + 10.
- §5.6 AsyncOutcome — Task 3.
- §5.7 ResponseFactory — Task 9.
- §11 Allocation strategy — Tasks 5 (Headers BeastBacking), 6/11 (Body), 12-14 (RequestContext arena-backed containers).
- §13 CMake reorganization — Task 1 + per-task `target_sources` + Task 15 cleanup.
- §14.1 Unit tests — covered per task.
- §15 Decisions log entries that touch this PR — all reflected.

Placeholder scan: no TBDs, TODOs, "implement later"s in the plan body. Every code step has full code.

Type consistency: `Headers::owned` / `Headers::view_of_beast` consistent across tasks. `Body::read_to_string` signature matches across `body.hpp` declaration (Task 6) and `body.cpp` definition (Task 11). `RequestContext::set_path_param` signature matches its caller pattern in Task 13 + the prospective routing layer (PR 2). The internal `BagEntry`-with-function-pointer-destroyer is consistent in Tasks 14's header + cpp moves.

Out-of-scope explicit notes: BeastRequestBody (zero-copy span over Beast's body buffer) is not in this PR; it's a Body subclass added when h1 driver lands (PR 3). The unique_ptr<Body> indirection costs one heap alloc per request body — acceptable for v1; the SBO Body value type from the spec §5.2 is a future optimization that doesn't change handler-visible API.

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-05-07-http-types-layer.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration. Each task closes a TDD cycle (red → green → commit) before the next subagent starts.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

**Which approach?**

After PR 1 lands, the next plan to write is `2026-MM-DD-http-routing-layer.md` covering RouteRegistry, HttpController evolution, GroupBinding, Router, and the bake step.
