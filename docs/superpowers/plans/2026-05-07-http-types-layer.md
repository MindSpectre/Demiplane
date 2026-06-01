# HTTP Redesign — PR 1: Types Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Reconciled 2026-05-31** against the updated design spec. This supersedes the original PR1 plan. The four spec↔plan divergences are resolved here: `Body` is a **value type with SBO type-erasure** (no `unique_ptr`), `Request::target` is a **`string_view`** into the receive buffer, response construction is **ctx-scoped** (`ctx.json(...)`) and arena-backed, and the zero-additional-allocation invariant is **gated by a counting `memory_resource`** (response side in PR1). See spec §3, §5, §11, §15.

**Goal:** Build the protocol-agnostic core types (Headers, Body, Request, Response, RequestContext, errors, ResponseFactory) as a self-contained `Demiplane::Component::Http::Types` static library with full unit test coverage **and an allocation gate**. After this PR the new layer is usable from tests and ready for PR 2 (Routing). The existing `http_server/` library is untouched; this PR is strictly additive.

**Architecture (reconciled):** Pure data + utility layer. No drivers, no servers, no network I/O.

- `Headers` — tagged-union facade: `BeastBacking` (read-only view over `boost::beast::http::fields`) and `OwnedBacking` (arena-owned `pmr::string` pairs). **Always constructed with an allocator; no null/default state.** Mutators and view→owned promotion allocate through the bound allocator — never the global heap. Iteration is O(1) per step (the iterator holds the backing's native iterator, not an index it re-walks).
- `Body` — **value type, ~48-byte SBO, type-erased** (`read_chunk()` dispatched through an internal vtable; no `dynamic_cast`, no `unique_ptr<Body>`). PR1 payload kinds: `EmptyBody`, `OwnedBufferBody`. `BeastRequestBody` (zero-copy request body) lands in PR3; `StreamingProducerBody` later. A non-streaming body exposes `buffered_view()` for the driver fast-path and for tests (this replaces the old `dynamic_cast<StringBody*>` idiom). Buffered helpers (`read_to_string`/`read_json`/`read_form`/`read_multipart`) return `gears::Outcome`.
- `Request` — plain struct: `string_view target` (view into receive buffer), value `Body`, `Headers`. Valid only for the handler's duration (lifetime contract).
- `Response` — **stores a `std::pmr::polymorphic_allocator<>`** (so post-handler middleware mutation stays in the arena), value `Body`, `Headers`, `keep_alive`. `set_header` replaces / `add_header` appends. Default-constructed Response uses `new_delete` (cold path); ctx builds it with the arena.
- `RequestContext` — lazy header lookup, `target`-as-view path/query split (no SSO dangle), `query<T>`/`path_param<T>` **defined in the header** (any arithmetic or string type, no link-time type cap), type-keyed bag, and the **ctx-scoped response factories** (`ok`/`json`/`created`/`no_content`/`redirect`/`status`) that build arena-backed responses.
- `errors.hpp` — built-in error structs each paired with an **arena-free** `to_http_response(const E&) -> Response` (cold-path, global heap via the static `ResponseFactory`).

**Tech Stack:**
- C++23 (deducing this; pmr; concepts; `if constexpr`)
- Boost.Beast (only for `boost::beast::http::fields` in `Headers::BeastBacking`)
- Boost.Container (`small_vector` for arena-backed param/bag storage)
- Boost.Asio (`asio::awaitable<T>` for `Body::read_chunk` and `AsyncOutcome`)
- JsonCpp (`Json::Value`, `Json::CharReaderBuilder`)
- `gears::Outcome` (project's typed-error sum type — confirmed API: `visit`, `gears::err`, `is_success`, `holds_error<E>`, `error<E>`, `value`, `value_or`)
- GoogleTest

**Out of PR1 scope (deferred, noted where relevant):** `BeastRequestBody` and `ctx.stream(...)` / `StreamingProducerBody` (need the driver + `Body::Writer`, PR3+); routing/`set_path_param` caller is PR2; the request-side half of the allocation gate (PR3, when a real connection arena exists).

---

## File Structure

```
components/http/types/
├─ CMakeLists.txt                      ← new, owned by this layer
├─ http_enums.hpp                      Protocol, HttpMethod, HttpStatus, HttpVersion
├─ async_outcome.hpp                   AsyncOutcome<T, Es...> / AsyncResponse aliases
├─ url_decode/
│  ├─ url_decode.hpp                   RFC 3986 %XX + '+' decoding (shared by body + ctx)
│  └─ url_decode.cpp
├─ headers/
│  ├─ headers.hpp
│  └─ headers.cpp
├─ body/
│  ├─ body.hpp                         value-type SBO Body + EmptyBody/OwnedBufferBody payloads
│  └─ body.cpp                         vtable plumbing + buffered helpers
├─ request/
│  ├─ request.hpp                      struct Request (string_view target, value Body)
│  └─ request.cpp
├─ response/
│  ├─ response.hpp                     struct Response (stored allocator + fluent setters)
│  └─ response.cpp
├─ errors/
│  ├─ errors.hpp                       error structs + to_http_response decls
│  └─ errors.cpp                       to_http_response defs (cold path, via ResponseFactory)
├─ response_factory/
│  ├─ response_factory.hpp             static cold-path factory (errors/tests/synthetic)
│  └─ response_factory.cpp
└─ request_context/
   ├─ request_context.hpp             accessors, predicates, params, bag, ctx-scoped factories
   └─ request_context.cpp

tests/unit_tests/http/types/
├─ test_http_enums.cpp
├─ test_url_decode.cpp
├─ test_headers.cpp
├─ test_body.cpp
├─ test_response.cpp
├─ test_response_factory.cpp
├─ test_errors.cpp
├─ test_request_context.cpp
└─ test_allocation_gate.cpp           ← the invariant gate (response side)

components/http/CMakeLists.txt        ← edited: register Types subdirectory
tests/unit_tests/CMakeLists.txt       ← edited: add Http.Types unit test target
```

Build verification each task: `cmake --build build/release` from the project root completes clean. Test verification: `ctest --test-dir build/release --output-on-failure -L unit -R Http.Types` passes.

---

## Task 1: Bootstrap directory tree + minimum-viable CMakeLists

**Files:** Create `components/http/types/CMakeLists.txt`, `components/http/types/types_placeholder.cpp`; Modify `components/http/CMakeLists.txt`.

**Goal:** Get an empty `Demiplane::Component::Http::Types` static library compiling and linkable.

- [ ] **Step 1: Create the directory tree**

```bash
cd /home/grivin/Workspace/Demiplane
mkdir -p components/http/types/{url_decode,headers,body,request,response,errors,response_factory,request_context}
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
target_sources(${DMP_HTTP}.Types PRIVATE types_placeholder.cpp)
##############################################################################
```

- [ ] **Step 3: Create `components/http/types/types_placeholder.cpp`**

```cpp
// Placeholder TU to give Demiplane::Component::Http::Types a non-empty
// compilation unit during PR 1 bootstrap. Deleted in the final task.
namespace demiplane::http::detail {
    inline constexpr int types_layer_present = 1;
}
```

- [ ] **Step 4: Edit `components/http/CMakeLists.txt`** — register the new sub-CMakeLists *before* the existing `${DMP_HTTP}.Handler`. The current file starts:

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

- [ ] **Step 5: Build**

```bash
cmake --preset release && cmake --build build/release --target Demiplane.Component.HTTP.Types -- -j4 2>&1 | tail -30
```

Expected: build succeeds; static archive under `build/release/components/http/types/`.

- [ ] **Step 6: Commit**

```bash
git add components/http/types components/http/CMakeLists.txt
git commit -m "feat(http): bootstrap Types layer scaffold

Empty Demiplane::Component::Http::Types static library linked into the
http component umbrella. Placeholder TU only; real source lands per task."
```

---

## Task 2: http_enums.hpp

**Files:** Create `components/http/types/http_enums.hpp`, `tests/unit_tests/http/types/test_http_enums.cpp`; Modify `tests/unit_tests/CMakeLists.txt`.

**Goal:** The four protocol-agnostic enums + Beast conversion helpers.

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/types/test_http_enums.cpp`

```cpp
#include <gtest/gtest.h>
#include <http_enums.hpp>

using namespace demiplane::http;

TEST(HttpEnumsTest, MethodToString) {
    EXPECT_EQ(to_string(HttpMethod::get),     std::string_view{"GET"});
    EXPECT_EQ(to_string(HttpMethod::post),    std::string_view{"POST"});
    EXPECT_EQ(to_string(HttpMethod::del),     std::string_view{"DELETE"});
    EXPECT_EQ(to_string(HttpMethod::options), std::string_view{"OPTIONS"});
}

TEST(HttpEnumsTest, MethodFromBeast) {
    EXPECT_EQ(method_from_beast(boost::beast::http::verb::get),     HttpMethod::get);
    EXPECT_EQ(method_from_beast(boost::beast::http::verb::delete_), HttpMethod::del);
    EXPECT_EQ(method_from_beast(boost::beast::http::verb::unknown), HttpMethod::unknown);
}

TEST(HttpEnumsTest, StatusCodeNumericValue) {
    EXPECT_EQ(static_cast<int>(HttpStatus::ok),                    200);
    EXPECT_EQ(static_cast<int>(HttpStatus::no_content),            204);
    EXPECT_EQ(static_cast<int>(HttpStatus::not_found),             404);
    EXPECT_EQ(static_cast<int>(HttpStatus::method_not_allowed),    405);
    EXPECT_EQ(static_cast<int>(HttpStatus::payload_too_large),     413);
    EXPECT_EQ(static_cast<int>(HttpStatus::internal_server_error), 500);
}

TEST(HttpEnumsTest, VersionNumeric) {
    EXPECT_EQ(static_cast<unsigned>(HttpVersion::http_1_1), 11u);
    EXPECT_EQ(static_cast<unsigned>(HttpVersion::http_2),   20u);
}
```

- [ ] **Step 2: Wire up the test target** — edit `tests/unit_tests/CMakeLists.txt`, near the bottom of the `add_unit_test` blocks:

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

The source list grows as later tasks add tests; check before appending.

- [ ] **Step 3: Configure + build — expect failure** (`http_enums.hpp` missing).

```bash
cmake --preset release && cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -20
```

- [ ] **Step 4: Create `components/http/types/http_enums.hpp`**

```cpp
#pragma once

#include <cstdint>
#include <string_view>

#include <boost/beast/http/verb.hpp>

namespace demiplane::http {

    enum class Protocol : std::uint8_t { http1, http2, http3 };

    enum class HttpMethod : std::uint8_t {
        unknown, get, post, put, patch, del /* not 'delete' */, head, options,
    };

    enum class HttpStatus : std::uint16_t {
        ok = 200, created = 201, accepted = 202, no_content = 204,
        moved_permanently = 301, found = 302, see_other = 303,
        not_modified = 304, temporary_redirect = 307, permanent_redirect = 308,
        bad_request = 400, unauthorized = 401, forbidden = 403, not_found = 404,
        method_not_allowed = 405, conflict = 409, gone = 410,
        payload_too_large = 413, unsupported_media_type = 415,
        unprocessable_entity = 422, too_many_requests = 429,
        internal_server_error = 500, not_implemented = 501, bad_gateway = 502,
        service_unavailable = 503, gateway_timeout = 504,
    };

    enum class HttpVersion : std::uint8_t {
        http_1_0 = 10, http_1_1 = 11, http_2 = 20, http_3 = 30,
    };

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

    constexpr HttpVersion version_from_beast(unsigned v) noexcept {
        switch (v) {
            case 10: return HttpVersion::http_1_0;
            case 11: return HttpVersion::http_1_1;
            case 20: return HttpVersion::http_2;
            case 30: return HttpVersion::http_3;
            default: return HttpVersion::http_1_1;
        }
    }

    constexpr unsigned version_to_beast(HttpVersion v) noexcept {
        return static_cast<unsigned>(v);
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Build + run — expect pass.**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -20
```

- [ ] **Step 6: Commit**

```bash
git add components/http/types/http_enums.hpp tests/unit_tests/http/types/test_http_enums.cpp tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add http_enums + Beast conversion helpers"
```

---

## Task 3: async_outcome.hpp

**Files:** Create `components/http/types/async_outcome.hpp`. (No test — one-line aliases, exercised downstream.)

- [ ] **Step 1: Create `components/http/types/async_outcome.hpp`**

```cpp
#pragma once

#include <boost/asio/awaitable.hpp>
#include <demiplane/gears>

namespace demiplane::http {

    struct Response;   // defined in Task 8

    /// asio coroutine yielding a typed-error sum result. Handlers/middleware
    /// return AsyncOutcome<Response, Errors...>; the bind layer (PR2) collapses
    /// the held alternative into a plain Response via ADL to_http_response.
    template <typename T, typename... Es>
    using AsyncOutcome = boost::asio::awaitable<gears::Outcome<T, Es...>>;

    /// The common "no typed errors" case. Response is a struct (Task 8) — the
    /// forward declaration above suffices for the alias; users who instantiate
    /// it include <response/response.hpp>.
    using AsyncResponse = boost::asio::awaitable<Response>;

    using AsyncVoid = boost::asio::awaitable<void>;

}  // namespace demiplane::http
```

> Note: `Response` is forward-declared as a `struct` (it is a struct in Task 8) to keep the elaborated-type-specifier consistent.

- [ ] **Step 2: Build to verify it parses; Step 3: Commit**

```bash
cmake --build build/release --target Demiplane.Component.HTTP.Types -- -j4 2>&1 | tail -10
git add components/http/types/async_outcome.hpp
git commit -m "feat(http/types): add AsyncOutcome / AsyncResponse aliases"
```

---

## Task 4: errors.hpp — error structs + to_http_response declarations

**Files:** Create `components/http/types/errors/errors.hpp`.

**Goal:** Built-in error structs + forward-declared **arena-free** `to_http_response(const E&)` overloads (defs in Task 11).

- [ ] **Step 1: Create `components/http/types/errors/errors.hpp`**

```cpp
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "http_enums.hpp"

namespace demiplane::http {

    struct Response;   // defined in Task 8

    struct BadRequestError   { std::string message; };
    struct UnauthorizedError { std::string message; };
    struct ForbiddenError    { std::string message; };
    struct NotFoundError     { std::string resource; std::string id; };
    struct ConflictError     { std::string message; };

    struct FieldError { std::string field; std::string detail; };
    struct UnprocessableEntityError { std::string message; std::vector<FieldError> fields; };

    struct PayloadTooLargeError  { std::size_t limit = 0; };
    struct MethodNotAllowedError { std::vector<HttpMethod> allowed; };

    struct JsonParseError      { std::string detail; };
    struct FormParseError      { std::string detail; };
    struct MultipartParseError { std::string detail; };
    struct BodyLimitExceeded   { std::size_t limit = 0; };

    // ── ADL conversions ──────────────────────────────────────────────────
    // Intentionally arena-free: error responses (4xx/5xx) are the cold path
    // and construct on the global heap via the static ResponseFactory (spec
    // §5.5/§5.7). Keeps the user's extension point a clean 1-arg free function.
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

- [ ] **Step 2: Build; Step 3: Commit**

```bash
cmake --build build/release --target Demiplane.Component.HTTP.Types -- -j4 2>&1 | tail -10
git add components/http/types/errors/errors.hpp
git commit -m "feat(http/types): add built-in error types + arena-free to_http_response decls"
```

---

## Task 5: url_decode — shared RFC 3986 decoder

**Files:** Create `url_decode/url_decode.{hpp,cpp}`, `tests/unit_tests/http/types/test_url_decode.cpp`; Modify both CMakeLists.

**Goal:** One canonical decoder (the original plan duplicated it in body.cpp and request_context.cpp — extract it). Returns `std::nullopt` on malformed escapes so callers choose how to surface them.

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/types/test_url_decode.cpp`

```cpp
#include <gtest/gtest.h>
#include <url_decode/url_decode.hpp>

using namespace demiplane::http;

TEST(UrlDecodeTest, PlainPassthrough)      { EXPECT_EQ(url_decode("hello").value(), "hello"); }
TEST(UrlDecodeTest, PercentEscape)         { EXPECT_EQ(url_decode("John%20Doe").value(), "John Doe"); }
TEST(UrlDecodeTest, PlusIsSpaceByDefault)  { EXPECT_EQ(url_decode("New+York").value(), "New York"); }
TEST(UrlDecodeTest, PlusLiteralWhenOff)    { EXPECT_EQ(url_decode("a+b", false).value(), "a+b"); }
TEST(UrlDecodeTest, TruncatedEscapeFails)  { EXPECT_FALSE(url_decode("a%2").has_value()); }
TEST(UrlDecodeTest, BadHexFails)           { EXPECT_FALSE(url_decode("a%2G").has_value()); }
TEST(UrlDecodeTest, LowercaseHex)          { EXPECT_EQ(url_decode("%2f").value(), "/"); }
```

- [ ] **Step 2: Add `http/types/test_url_decode.cpp` to the `Http.Types` test source list.**

- [ ] **Step 3: Build — expect failure.**

- [ ] **Step 4: Create `components/http/types/url_decode/url_decode.hpp`**

```cpp
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace demiplane::http {

    /// RFC 3986 percent-decoding. With plus_is_space, '+' -> ' '
    /// (application/x-www-form-urlencoded). Returns nullopt on a malformed
    /// escape (truncated or non-hex) so callers can surface a typed error.
    std::optional<std::string> url_decode(std::string_view in, bool plus_is_space = true);

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/types/url_decode/url_decode.cpp`**

```cpp
#include "url_decode.hpp"

namespace demiplane::http {

    std::optional<std::string> url_decode(std::string_view in, bool plus_is_space) {
        std::string out;
        out.reserve(in.size());
        for (std::size_t i = 0; i < in.size(); ++i) {
            const char c = in[i];
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
                const int hi = hex(in[i + 1]);
                const int lo = hex(in[i + 2]);
                if (hi < 0 || lo < 0) return std::nullopt;
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(c);
            }
        }
        return out;
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Wire source** — append to `components/http/types/CMakeLists.txt`:

```cmake
target_sources(${DMP_HTTP}.Types PRIVATE url_decode/url_decode.cpp)
```

- [ ] **Step 7: Build + run — expect pass; Step 8: Commit**

```bash
git add components/http/types/url_decode tests/unit_tests/http/types/test_url_decode.cpp components/http/types/CMakeLists.txt tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add shared url_decode (RFC 3986 + form '+')"
```

---

## Task 6: Headers — allocator-bound, O(1) iteration

**Files:** Create `headers/headers.{hpp,cpp}`, `tests/unit_tests/http/types/test_headers.cpp`; Modify both CMakeLists.

**Goal (reconciled):** Tagged-union `Headers`. **Always constructed with an allocator; no null/default state** — "empty" is an empty `OwnedBacking` bound to its allocator. Mutators and view→owned promotion use the **bound allocator, never the global heap**. Iteration is **O(1) per step** (the iterator holds the backing's native iterator).

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/types/test_headers.cpp`

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

TEST_F(HeadersTest, OwnedAddAndGetCaseInsensitive) {
    Headers h = Headers::owned(alloc_);
    h.add("Content-Type", "application/json");
    auto ct = h.get("content-type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "application/json");
    EXPECT_FALSE(h.get("X-Missing").has_value());
}

TEST_F(HeadersTest, OwnedMultiValueGetAll) {
    Headers h = Headers::owned(alloc_);
    h.add("Set-Cookie", "a=1");
    h.add("Set-Cookie", "b=2");
    auto all = h.get_all("set-cookie", alloc_);
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0], "a=1");
    EXPECT_EQ(all[1], "b=2");
    EXPECT_EQ(*h.get("set-cookie"), "a=1");   // first-occurrence
}

TEST_F(HeadersTest, SetReplacesAll) {
    Headers h = Headers::owned(alloc_);
    h.add("X-Tag", "first");
    h.add("X-Tag", "second");
    h.set("X-Tag", "only");
    auto all = h.get_all("x-tag", alloc_);
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0], "only");
}

TEST_F(HeadersTest, Remove) {
    Headers h = Headers::owned(alloc_);
    h.add("X-A", "1");
    h.add("X-B", "2");
    h.remove("x-a");
    EXPECT_FALSE(h.get("X-A").has_value());
    EXPECT_TRUE(h.get("X-B").has_value());
}

TEST_F(HeadersTest, IterationInsertionOrder) {
    Headers h = Headers::owned(alloc_);
    h.add("Host", "example.com");
    h.add("User-Agent", "test");
    std::vector<std::string> names;
    for (auto const& [n, v] : h) names.emplace_back(n);
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "Host");
    EXPECT_EQ(names[1], "User-Agent");
}

TEST_F(HeadersTest, BeastBackingViewsParsedFields) {
    boost::beast::http::fields fields;
    fields.insert("Content-Type", "text/plain");
    fields.insert("X-Custom", "value");
    Headers h = Headers::view_of_beast(fields);
    ASSERT_TRUE(h.get("Content-Type").has_value());
    EXPECT_EQ(*h.get("Content-Type"), "text/plain");
    EXPECT_TRUE(h.contains("x-custom"));
    // O(1)-per-step iteration must visit all entries:
    std::size_t n = 0;
    for (auto const& kv : h) { (void)kv; ++n; }
    EXPECT_EQ(n, 2u);
}

TEST_F(HeadersTest, MutationPromotesBeastToOwnedViaBoundAllocator) {
    boost::beast::http::fields fields;
    fields.insert("Content-Type", "text/plain");
    Headers h = Headers::view_of_beast(fields);
    h.promote_to_owned(alloc_);   // explicit allocator — never the global heap
    h.add("X-Custom", "value");
    EXPECT_TRUE(h.contains("Content-Type"));
    EXPECT_TRUE(h.contains("X-Custom"));
}

TEST_F(HeadersTest, GetOrFallback) {
    Headers h = Headers::owned(alloc_);
    h.add("X-Tag", "value");
    EXPECT_EQ(h.get_or("X-Tag", "fallback"), "value");
    EXPECT_EQ(h.get_or("X-Missing", "fallback"), "fallback");
}
```

- [ ] **Step 2: Add `http/types/test_headers.cpp` to the test sources.**

- [ ] **Step 3: Build — expect failure.**

- [ ] **Step 4: Create `components/http/types/headers/headers.hpp`**

```cpp
#pragma once

#include <cstddef>
#include <iterator>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <boost/beast/http/fields.hpp>

namespace demiplane::http {

    /**
     * @brief Multi-value, case-insensitive, insertion-ordered HTTP headers.
     *
     * Two backings behind one API:
     *   BeastBacking — read-only view over a parser-owned beast::http::fields
     *                  (incoming requests, zero copy).
     *   OwnedBacking — arena-owned pmr::string pairs (responses, h2/h3 incoming,
     *                  synthetic/test). ALWAYS carries its allocator.
     *
     * There is no default/null state: construct via owned(alloc) or
     * view_of_beast(fields). Mutators and promotion allocate through the bound
     * allocator — never the global heap.
     */
    class Headers {
    public:
        using value_type = std::pair<std::string_view, std::string_view>;

        // ── Factories ────────────────────────────────────────────────────
        static Headers owned(std::pmr::polymorphic_allocator<> alloc);
        static Headers view_of_beast(const boost::beast::http::fields& fields);

        // Headers is move-only (it may hold a pmr container).
        Headers(Headers&&) = default;
        Headers& operator=(Headers&&) = default;
        Headers(const Headers&) = delete;
        Headers& operator=(const Headers&) = delete;

        // ── Read API ─────────────────────────────────────────────────────
        std::optional<std::string_view> get(std::string_view name) const;
        std::string get_or(std::string_view name, std::string_view fallback) const;
        std::pmr::vector<std::string_view> get_all(std::string_view name,
                                                   std::pmr::polymorphic_allocator<> alloc) const;
        bool contains(std::string_view name) const;
        std::size_t size() const;
        bool empty() const { return size() == 0; }

        // ── Write API (requires OwnedBacking; promote first if viewing) ────
        void add(std::string_view name, std::string_view value);
        void set(std::string_view name, std::string_view value);
        void remove(std::string_view name);

        /// Copy a BeastBacking into a fresh OwnedBacking in `alloc`. No-op if
        /// already owned. MUST be called before mutating a viewing Headers.
        void promote_to_owned(std::pmr::polymorphic_allocator<> alloc);

        // ── Iteration (O(1) per step) ─────────────────────────────────────
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
            friend bool operator==(const const_iterator& a, const const_iterator& b) {
                return a.idx_ == b.idx_ && a.h_ == b.h_;
            }

        private:
            friend class Headers;
            const Headers* h_ = nullptr;
            std::size_t idx_  = 0;   // position; also drives beast_it_ advance
            boost::beast::http::fields::const_iterator beast_it_{};
        };

        const_iterator begin() const;
        const_iterator end() const;

    private:
        struct BeastBacking { const boost::beast::http::fields* fields; };
        struct OwnedBacking {
            std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>> entries;
            explicit OwnedBacking(std::pmr::polymorphic_allocator<> a) : entries(a) {}
        };
        std::variant<BeastBacking, OwnedBacking> backing_;

        explicit Headers(BeastBacking b) : backing_{b} {}
        explicit Headers(OwnedBacking&& o) : backing_{std::move(o)} {}

        OwnedBacking& as_owned();   // asserts owned; used by mutators after promote
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/types/headers/headers.cpp`**

```cpp
#include "headers.hpp"

#include <algorithm>
#include <cassert>

namespace demiplane::http {

    namespace {
        constexpr unsigned char lower(unsigned char c) noexcept {
            return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + 32) : c;
        }
        bool iequals(std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i)
                if (lower(static_cast<unsigned char>(a[i])) != lower(static_cast<unsigned char>(b[i])))
                    return false;
            return true;
        }
    }

    Headers Headers::owned(std::pmr::polymorphic_allocator<> alloc) {
        return Headers{OwnedBacking{alloc}};
    }
    Headers Headers::view_of_beast(const boost::beast::http::fields& fields) {
        return Headers{BeastBacking{&fields}};
    }

    void Headers::promote_to_owned(std::pmr::polymorphic_allocator<> alloc) {
        if (std::holds_alternative<OwnedBacking>(backing_)) return;
        const auto& bb = std::get<BeastBacking>(backing_);
        OwnedBacking owned{alloc};
        for (const auto& f : *bb.fields) {
            owned.entries.emplace_back(
                std::pmr::string{std::string_view(f.name_string()), alloc},
                std::pmr::string{std::string_view(f.value()),       alloc});
        }
        backing_ = std::move(owned);
    }

    Headers::OwnedBacking& Headers::as_owned() {
        assert(std::holds_alternative<OwnedBacking>(backing_) &&
               "mutating a viewing Headers — call promote_to_owned(alloc) first");
        return std::get<OwnedBacking>(backing_);
    }

    std::optional<std::string_view> Headers::get(std::string_view name) const {
        return std::visit([&]<typename B>(const B& b) -> std::optional<std::string_view> {
            if constexpr (std::same_as<B, BeastBacking>) {
                auto it = b.fields->find(name);
                if (it == b.fields->end()) return std::nullopt;
                return std::string_view(it->value());
            } else {
                for (const auto& [n, v] : b.entries)
                    if (iequals(std::string_view(n), name)) return std::string_view(v);
                return std::nullopt;
            }
        }, backing_);
    }

    std::string Headers::get_or(std::string_view name, std::string_view fallback) const {
        if (auto v = get(name)) return std::string{*v};
        return std::string{fallback};
    }

    std::pmr::vector<std::string_view>
    Headers::get_all(std::string_view name, std::pmr::polymorphic_allocator<> alloc) const {
        std::pmr::vector<std::string_view> out{alloc};
        std::visit([&]<typename B>(const B& b) {
            if constexpr (std::same_as<B, BeastBacking>) {
                auto range = b.fields->equal_range(name);
                for (auto it = range.first; it != range.second; ++it)
                    out.emplace_back(it->value());
            } else {
                for (const auto& [n, v] : b.entries)
                    if (iequals(std::string_view(n), name)) out.emplace_back(v);
            }
        }, backing_);
        return out;
    }

    bool Headers::contains(std::string_view name) const { return get(name).has_value(); }

    void Headers::add(std::string_view name, std::string_view value) {
        auto& o = as_owned();
        auto a = o.entries.get_allocator();
        o.entries.emplace_back(std::pmr::string{name, a}, std::pmr::string{value, a});
    }
    void Headers::set(std::string_view name, std::string_view value) {
        auto& o = as_owned();
        std::erase_if(o.entries, [&](const auto& kv) { return iequals(std::string_view(kv.first), name); });
        auto a = o.entries.get_allocator();
        o.entries.emplace_back(std::pmr::string{name, a}, std::pmr::string{value, a});
    }
    void Headers::remove(std::string_view name) {
        auto& o = as_owned();
        std::erase_if(o.entries, [&](const auto& kv) { return iequals(std::string_view(kv.first), name); });
    }

    std::size_t Headers::size() const {
        return std::visit([]<typename B>(const B& b) -> std::size_t {
            if constexpr (std::same_as<B, BeastBacking>) return static_cast<std::size_t>(
                std::distance(b.fields->begin(), b.fields->end()));
            else return b.entries.size();
        }, backing_);
    }

    Headers::const_iterator Headers::begin() const {
        const_iterator it;
        it.h_ = this;
        it.idx_ = 0;
        if (auto* bb = std::get_if<BeastBacking>(&backing_)) it.beast_it_ = bb->fields->begin();
        return it;
    }
    Headers::const_iterator Headers::end() const {
        const_iterator it;
        it.h_ = this;
        it.idx_ = size();
        if (auto* bb = std::get_if<BeastBacking>(&backing_)) it.beast_it_ = bb->fields->end();
        return it;
    }

    Headers::value_type Headers::const_iterator::operator*() const {
        return std::visit([&]<typename B>(const B& b) -> Headers::value_type {
            if constexpr (std::same_as<B, BeastBacking>) {
                return {std::string_view(beast_it_->name_string()),
                        std::string_view(beast_it_->value())};
            } else {
                const auto& [n, v] = b.entries[idx_];
                return {std::string_view(n), std::string_view(v)};
            }
        }, h_->backing_);
    }

    Headers::const_iterator& Headers::const_iterator::operator++() {
        ++idx_;
        if (std::holds_alternative<BeastBacking>(h_->backing_)) ++beast_it_;  // O(1)
        return *this;
    }
    Headers::const_iterator Headers::const_iterator::operator++(int) {
        auto tmp = *this; ++*this; return tmp;
    }

}  // namespace demiplane::http
```

> The `equal_range`-based `get_all` preserves insertion order for Beast (Beast keeps duplicate fields in insertion order). `size()` over Beast is `std::distance` (Beast's `fields` has no O(1) size); it is used only by `end()` and rarely otherwise.

- [ ] **Step 6: Wire source** — `target_sources(${DMP_HTTP}.Types PRIVATE headers/headers.cpp)`.

- [ ] **Step 7: Build + run — expect pass; Step 8: Commit**

```bash
git add components/http/types/headers tests/unit_tests/http/types/test_headers.cpp components/http/types/CMakeLists.txt tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add Headers (allocator-bound, O(1) iteration, no null state)

BeastBacking views parsed fields; OwnedBacking is arena-owned. Mutation
requires an explicit promote_to_owned(alloc) — never the global heap. The
const_iterator carries the backing's native iterator, so iteration is O(1)
per step (fixes the O(n^2) re-walk in the original design)."
```

---

## Task 7: Body — value type, SBO type-erasure, EmptyBody + OwnedBufferBody

**Files:** Create `body/body.{hpp,cpp}`, `tests/unit_tests/http/types/test_body.cpp`; Modify both CMakeLists.

**Goal (reconciled):** `Body` is a **value type** with ~48-byte SBO type-erased storage — **no `unique_ptr`, no `dynamic_cast`**. `read_chunk()` dispatches through an internal vtable. PR1 payloads: `EmptyBody`, `OwnedBufferBody`. A non-streaming body exposes `buffered_view()` (driver fast-path + the test inspection idiom that replaces `dynamic_cast<StringBody*>`). Buffered helpers land in Task 12.

> **Implementer note — the riskiest code in PR1.** The move/destroy/alignment plumbing is exactly what the ASan run in the final task stresses. Keep the payloads ≤ `kInlineSize` (static_assert enforces it) and never move a `Body` while a `read_chunk()` awaitable from it is still in flight (the awaitable captures the payload by address).

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/types/test_body.cpp`

```cpp
#include <string>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>

#include <body/body.hpp>

using namespace demiplane::http;

namespace {
    template <typename T>
    T run_awaitable(boost::asio::awaitable<T> aw) {
        boost::asio::io_context ioc;
        auto fut = boost::asio::co_spawn(ioc, std::move(aw), boost::asio::use_future);
        ioc.run();
        return fut.get();
    }
}

TEST(BodyTest, DefaultIsEmpty) {
    Body b;
    EXPECT_EQ(b.size_hint().value_or(99), 0u);
    ASSERT_TRUE(b.buffered_view().has_value());
    EXPECT_EQ(*b.buffered_view(), "");
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
}

TEST(BodyTest, OwnedBufferYieldsContentsThenEnd) {
    Body b = Body::owned("hello, world");
    ASSERT_TRUE(b.buffered_view().has_value());
    EXPECT_EQ(*b.buffered_view(), "hello, world");
    EXPECT_EQ(b.size_hint().value_or(0), 12u);

    auto first = run_awaitable(b.read_chunk());
    ASSERT_TRUE(first.has_value());
    std::string text(reinterpret_cast<const char*>(first->data()), first->size());
    EXPECT_EQ(text, "hello, world");
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
}

TEST(BodyTest, OwnedEmptyYieldsNoChunks) {
    Body b = Body::owned("");
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
    EXPECT_EQ(*b.buffered_view(), "");
}

TEST(BodyTest, MoveTransfersPayloadLeavesSourceEmpty) {
    Body src = Body::owned("payload");
    Body dst = std::move(src);
    EXPECT_EQ(*dst.buffered_view(), "payload");
    EXPECT_EQ(*src.buffered_view(), "");   // moved-from is a valid EmptyBody
    EXPECT_EQ(src.size_hint().value_or(99), 0u);
}

TEST(BodyTest, MoveAssignDestroysOldPayload) {
    Body a = Body::owned("aaa");
    Body b = Body::owned("bbb");
    a = std::move(b);
    EXPECT_EQ(*a.buffered_view(), "bbb");
    EXPECT_EQ(*b.buffered_view(), "");
}
```

- [ ] **Step 2: Add `http/types/test_body.cpp` to the test sources.**

- [ ] **Step 3: Build — expect failure.**

- [ ] **Step 4: Create `components/http/types/body/body.hpp`**

```cpp
#pragma once

#include <cstddef>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <demiplane/gears>
#include <json/json.h>

#include "../async_outcome.hpp"
#include "../errors/errors.hpp"

namespace demiplane::http {

    struct MultipartField {
        std::string name;
        std::string value;          // payload (small/non-file fields)
        std::string content_type;
        std::string filename;       // empty for non-file fields
    };

    /**
     * @brief Streaming-truth body, held as an SBO value type.
     *
     * Type-erased over a payload (EmptyBody / OwnedBufferBody in PR1;
     * BeastRequestBody in PR3; StreamingProducerBody later). Common bodies live
     * entirely inline — zero heap nodes, no unique_ptr, no dynamic_cast. The
     * driver writes a body by driving read_chunk(), or — for non-streaming
     * bodies — by writing buffered_view() in one shot.
     */
    class Body {
    public:
        Body() noexcept;                 // EmptyBody
        Body(Body&&) noexcept;
        Body& operator=(Body&&) noexcept;
        Body(const Body&) = delete;
        Body& operator=(const Body&) = delete;
        ~Body();

        static Body empty() noexcept { return Body{}; }
        static Body owned(std::string bytes);   // OwnedBufferBody

        // Streaming primitive. nullopt when exhausted. (Plain function returning
        // the payload's awaitable — no extra coroutine frame here.)
        boost::asio::awaitable<std::optional<std::span<const std::byte>>> read_chunk();

        std::optional<std::size_t> size_hint() const;

        /// Whole-body view for non-streaming bodies (driver fast-path + tests).
        /// nullopt for streaming bodies; "" for EmptyBody.
        std::optional<std::string_view> buffered_view() const;

        // ── Buffered helpers (defs in Task 12) ───────────────────────────
        AsyncOutcome<std::string, BodyLimitExceeded>
            read_to_string(std::size_t limit);
        AsyncOutcome<Json::Value, JsonParseError, BodyLimitExceeded>
            read_json(std::size_t limit);
        AsyncOutcome<std::unordered_map<std::string, std::string>, FormParseError, BodyLimitExceeded>
            read_form(std::size_t limit);
        AsyncOutcome<std::vector<MultipartField>, MultipartParseError, BodyLimitExceeded>
            read_multipart(std::size_t limit, std::string_view boundary);

    private:
        static constexpr std::size_t kInlineSize = 48;

        struct VTable {
            boost::asio::awaitable<std::optional<std::span<const std::byte>>> (*read_chunk)(void*);
            std::optional<std::size_t>      (*size_hint)(const void*);
            std::optional<std::string_view> (*buffered_view)(const void*);
            void (*move)(void* dst, void* src) noexcept;   // move-construct dst, destroy src
            void (*destroy)(void*) noexcept;
        };

        template <typename T> static const VTable* vtable_for() noexcept {
            static const VTable vt{
                +[](void* p) { return static_cast<T*>(p)->read_chunk(); },
                +[](const void* p) { return static_cast<const T*>(p)->size_hint(); },
                +[](const void* p) { return static_cast<const T*>(p)->buffered_view(); },
                +[](void* d, void* s) noexcept {
                    ::new (d) T(std::move(*static_cast<T*>(s)));
                    static_cast<T*>(s)->~T();
                },
                +[](void* p) noexcept { static_cast<T*>(p)->~T(); },
            };
            return &vt;
        }

        struct emplace_t {};
        template <typename T, typename... A>
        explicit Body(emplace_t, std::in_place_type_t<T>, A&&... a) : vt_{vtable_for<T>()} {
            static_assert(sizeof(T) <= kInlineSize, "Body payload exceeds SBO budget");
            static_assert(alignof(T) <= alignof(std::max_align_t));
            ::new (storage_) T(std::forward<A>(a)...);
        }

        void* obj() noexcept { return storage_; }
        const void* obj() const noexcept { return storage_; }

        alignas(std::max_align_t) std::byte storage_[kInlineSize];
        const VTable* vt_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/types/body/body.cpp`** (payloads + ctor/move/dtor; buffered helpers added in Task 12)

```cpp
#include "body.hpp"

namespace demiplane::http {

    namespace {
        struct EmptyPayload {
            boost::asio::awaitable<std::optional<std::span<const std::byte>>> read_chunk() {
                co_return std::nullopt;
            }
            std::optional<std::size_t> size_hint() const { return 0; }
            std::optional<std::string_view> buffered_view() const { return std::string_view{}; }
        };

        struct OwnedBufferPayload {
            std::string bytes;
            bool consumed = false;
            boost::asio::awaitable<std::optional<std::span<const std::byte>>> read_chunk() {
                if (consumed || bytes.empty()) { consumed = true; co_return std::nullopt; }
                consumed = true;
                co_return std::span<const std::byte>{
                    reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()};
            }
            std::optional<std::size_t> size_hint() const { return bytes.size(); }
            std::optional<std::string_view> buffered_view() const { return bytes; }
        };
    }

    Body::Body() noexcept : vt_{vtable_for<EmptyPayload>()} {
        ::new (storage_) EmptyPayload{};
    }

    Body Body::owned(std::string bytes) {
        return Body{emplace_t{}, std::in_place_type<OwnedBufferPayload>,
                    OwnedBufferPayload{std::move(bytes), false}};
    }

    Body::Body(Body&& o) noexcept : vt_{o.vt_} {
        vt_->move(storage_, o.storage_);          // move-construct ours, destroy o's payload
        o.vt_ = vtable_for<EmptyPayload>();        // o becomes a valid EmptyBody
        ::new (o.storage_) EmptyPayload{};
    }

    Body& Body::operator=(Body&& o) noexcept {
        if (this == &o) return *this;
        vt_->destroy(storage_);
        vt_ = o.vt_;
        vt_->move(storage_, o.storage_);
        o.vt_ = vtable_for<EmptyPayload>();
        ::new (o.storage_) EmptyPayload{};
        return *this;
    }

    Body::~Body() { vt_->destroy(storage_); }

    boost::asio::awaitable<std::optional<std::span<const std::byte>>> Body::read_chunk() {
        return vt_->read_chunk(obj());
    }
    std::optional<std::size_t> Body::size_hint() const { return vt_->size_hint(obj()); }
    std::optional<std::string_view> Body::buffered_view() const { return vt_->buffered_view(obj()); }

    // Buffered helper definitions land in Task 12.

}  // namespace demiplane::http
```

- [ ] **Step 6: Wire source** — `target_sources(${DMP_HTTP}.Types PRIVATE body/body.cpp)`.

- [ ] **Step 7: Build + run — expect pass** (5 Body tests; helpers untested until Task 12).

- [ ] **Step 8: Commit**

```bash
git add components/http/types/body tests/unit_tests/http/types/test_body.cpp components/http/types/CMakeLists.txt tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add value-type SBO Body (EmptyBody + OwnedBufferBody)

Body is a ~48-byte SBO value with internal-vtable dispatch — no unique_ptr,
no dynamic_cast. buffered_view() is the non-streaming inspection path (driver
fast-path + test idiom). Move leaves the source a valid EmptyBody."
```

---

## Task 8: Request struct

**Files:** Create `request/request.{hpp,cpp}`; Modify CMakeLists.

**Goal (reconciled):** `target` is a **`string_view`** into the receive buffer (zero-copy, and it survives `RequestContext` moves — the SSO dangle is structurally gone). `body` is a **value `Body`** (not `unique_ptr`).

- [ ] **Step 1: Create `components/http/types/request/request.hpp`**

```cpp
#pragma once

#include <string_view>

#include "../body/body.hpp"
#include "../headers/headers.hpp"
#include "../http_enums.hpp"

namespace demiplane::http {

    /**
     * @brief Protocol-agnostic HTTP request.
     *
     * `target` is the raw, undecoded request target as a VIEW into the receive
     * buffer (e.g. "/users/42?q=foo"). It — like the headers and body — is valid
     * only while the owning connection's buffers live, i.e. for the duration of
     * the handler. Copy out anything you keep. Path/query split + URL decode
     * happen in RequestContext.
     *
     * `headers` and `body` are move-only value types. A Request must be
     * constructed with a Headers bound to an allocator (there is no null state).
     */
    struct Request {
        HttpMethod  method  = HttpMethod::unknown;
        HttpVersion version = HttpVersion::http_1_1;
        std::string_view target;            // view into receive buffer
        Headers     headers;                // move-only; bound to an allocator
        Body        body;                   // value type; default EmptyBody

        explicit Request(Headers hdrs) : headers{std::move(hdrs)} {}
        Request(Request&&) = default;
        Request& operator=(Request&&) = default;
        Request(const Request&) = delete;
        Request& operator=(const Request&) = delete;
    };

}  // namespace demiplane::http
```

> `Request` takes its `Headers` at construction because `Headers` has no default state. Drivers build `Request{Headers::view_of_beast(parser.get())}` (PR3); tests build `Request{Headers::owned(alloc)}`.

- [ ] **Step 2: Create `components/http/types/request/request.cpp`**

```cpp
// Reserved for non-inline Request helpers. Kept as a real TU so the source
// list never references a phantom path.
#include "request.hpp"
```

- [ ] **Step 3: Wire source; Step 4: Build; Step 5: Commit**

```bash
cmake --build build/release --target Demiplane.Component.HTTP.Types -- -j4 2>&1 | tail -10
git add components/http/types/request components/http/types/CMakeLists.txt
git commit -m "feat(http/types): add Request (string_view target, value Body)

target views the receive buffer (zero-copy; no owned-string SSO dangle).
Headers supplied at construction since Headers has no null state."
```

---

## Task 9: Response struct — stored allocator + fluent setters

**Files:** Create `response/response.{hpp,cpp}`, `tests/unit_tests/http/types/test_response.cpp`; Modify both CMakeLists.

**Goal (reconciled):** `Response` **stores its allocator** (so middleware mutation after the handler stays in the arena), holds a value `Body`, defaults to `new_delete` (cold path), and exposes `set_header` (replace) / `add_header` (append) — the distinction the old single `with_header` lacked. Built on the hot path by `ctx` (Task 15).

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/types/test_response.cpp`

```cpp
#include <memory_resource>
#include <string>

#include <gtest/gtest.h>

#include <body/body.hpp>
#include <response/response.hpp>

using namespace demiplane::http;

TEST(ResponseTest, DefaultConstructible) {
    Response r;
    EXPECT_EQ(r.status, HttpStatus::ok);
    EXPECT_EQ(r.version, HttpVersion::http_1_1);
    EXPECT_TRUE(r.keep_alive);
    EXPECT_TRUE(r.headers.empty());
    ASSERT_TRUE(r.body.buffered_view().has_value());
    EXPECT_EQ(*r.body.buffered_view(), "");
}

TEST(ResponseTest, FluentSettersOnLValue) {
    Response r;
    r.with_status(HttpStatus::created)
     .set_header("Content-Type", "application/json")
     .with_body("{\"id\":42}");
    EXPECT_EQ(r.status, HttpStatus::created);
    ASSERT_TRUE(r.headers.get("content-type").has_value());
    EXPECT_EQ(*r.headers.get("content-type"), "application/json");
    EXPECT_EQ(*r.body.buffered_view(), "{\"id\":42}");
}

TEST(ResponseTest, FluentSettersOnRValue) {
    Response r = Response{}
        .with_status(HttpStatus::no_content)
        .set_header("X-Custom", "value");
    EXPECT_EQ(r.status, HttpStatus::no_content);
    EXPECT_EQ(*r.headers.get("X-Custom"), "value");
}

TEST(ResponseTest, SetHeaderReplacesAddHeaderAppends) {
    Response r;
    r.add_header("X-Tag", "a").add_header("X-Tag", "b");
    std::pmr::monotonic_buffer_resource res{1024};
    EXPECT_EQ(r.headers.get_all("x-tag", &res).size(), 2u);
    r.set_header("X-Tag", "only");
    EXPECT_EQ(r.headers.get_all("x-tag", &res).size(), 1u);
}

TEST(ResponseTest, ArenaConstructorBindsHeaderAllocator) {
    std::pmr::monotonic_buffer_resource res{4096};
    Response r{std::pmr::polymorphic_allocator<>{&res}};
    r.add_header("X-A", "1");   // must allocate in `res`, not the global heap
    EXPECT_TRUE(r.headers.contains("X-A"));
}
```

- [ ] **Step 2: Add `http/types/test_response.cpp` to the test sources.**

- [ ] **Step 3: Build — expect failure.**

- [ ] **Step 4: Create `components/http/types/response/response.hpp`**

```cpp
#pragma once

#include <memory_resource>
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
     * STORES its allocator: built on the hot path through RequestContext
     * (Task 15) it points at the request arena, so even middleware that mutates
     * the response AFTER the handler returns keeps allocations in the arena.
     * Default-constructed (ctx-less / error / test) it uses new_delete — the
     * cold path. Body is a value type (Task 7). Drivers stamp Date/Server.
     */
    struct Response {
        std::pmr::polymorphic_allocator<> alloc{};
        HttpStatus  status     = HttpStatus::ok;
        HttpVersion version    = HttpVersion::http_1_1;
        bool        keep_alive = true;
        Headers     headers;
        Body        body;                       // default EmptyBody

        explicit Response(std::pmr::polymorphic_allocator<> a = {})
            : alloc{a}, headers{Headers::owned(a)} {}

        Response(Response&&) = default;
        Response& operator=(Response&&) = default;
        Response(const Response&) = delete;
        Response& operator=(const Response&) = delete;

        // ── Fluent setters (deducing this; chain on lvalues + rvalues) ─────
        template <typename Self>
        auto&& with_status(this Self&& self, HttpStatus s) noexcept {
            self.status = s;
            return std::forward<Self>(self);
        }
        template <typename Self>
        auto&& with_version(this Self&& self, HttpVersion v) noexcept {
            self.version = v;
            return std::forward<Self>(self);
        }
        template <typename Self>
        auto&& with_keep_alive(this Self&& self, bool k) noexcept {
            self.keep_alive = k;
            return std::forward<Self>(self);
        }
        template <typename Self>
        auto&& set_header(this Self&& self, std::string_view n, std::string_view v) {
            self.headers.set(n, v);
            return std::forward<Self>(self);
        }
        template <typename Self>
        auto&& add_header(this Self&& self, std::string_view n, std::string_view v) {
            self.headers.add(n, v);
            return std::forward<Self>(self);
        }
        template <typename Self>
        auto&& with_body(this Self&& self, std::string content) {
            self.body = Body::owned(std::move(content));
            return std::forward<Self>(self);
        }
    };

}  // namespace demiplane::http
```

> Member order matters: `alloc` is declared before `headers` so the constructor's `Headers::owned(a)` binds to the just-initialized arena allocator. An empty `OwnedBacking` does not allocate until the first `add`/`set`.

- [ ] **Step 5: Create `components/http/types/response/response.cpp`**

```cpp
// Response is header-only (deducing-this setters inline). Real TU so the
// source list isn't a phantom path.
#include "response.hpp"
```

- [ ] **Step 6: Wire source; Step 7: Build + run — expect pass; Step 8: Commit**

```bash
git add components/http/types/response tests/unit_tests/http/types/test_response.cpp components/http/types/CMakeLists.txt tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add Response (stored allocator, set/add_header, value Body)

Response stores its pmr allocator so post-handler middleware mutation stays
in the arena. set_header replaces; add_header appends. Default = new_delete
(cold path); ctx builds it with the request arena (Task 15)."
```

---

## Task 10: ResponseFactory — static cold-path helper

**Files:** Create `response_factory/response_factory.{hpp,cpp}`, `tests/unit_tests/http/types/test_response_factory.cpp`; Modify both CMakeLists.

**Goal (reconciled):** A **static, global-heap, cold-path** factory for the two ctx-less contexts: `to_http_response` (Task 11/errors) and library/test/synthetic construction. The hot path is `ctx.json(...)` (Task 15).

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/types/test_response_factory.cpp`

```cpp
#include <gtest/gtest.h>

#include <body/body.hpp>
#include <response_factory/response_factory.hpp>

using namespace demiplane::http;

namespace { std::string body_of(const Response& r) {
    return std::string{r.body.buffered_view().value_or("")};
}}

TEST(ResponseFactoryTest, OkDefault) {
    auto r = ResponseFactory::ok();
    EXPECT_EQ(r.status, HttpStatus::ok);
    EXPECT_EQ(*r.headers.get("Content-Type"), "text/plain");
    EXPECT_EQ(body_of(r), "");
}
TEST(ResponseFactoryTest, JsonContentType) {
    auto r = ResponseFactory::json("{\"k\":\"v\"}");
    EXPECT_EQ(*r.headers.get("Content-Type"), "application/json");
    EXPECT_EQ(body_of(r), "{\"k\":\"v\"}");
}
TEST(ResponseFactoryTest, NoContentHasNoBodyNorContentType) {
    auto r = ResponseFactory::no_content();
    EXPECT_EQ(r.status, HttpStatus::no_content);
    EXPECT_EQ(body_of(r), "");
    EXPECT_FALSE(r.headers.contains("Content-Type"));
}
TEST(ResponseFactoryTest, RedirectSetsLocation) {
    auto r = ResponseFactory::redirect("/login");
    EXPECT_EQ(r.status, HttpStatus::found);
    EXPECT_EQ(*r.headers.get("Location"), "/login");
}
TEST(ResponseFactoryTest, MethodNotAllowedSetsAllow) {
    HttpMethod allow[] = {HttpMethod::get, HttpMethod::post};
    auto r = ResponseFactory::method_not_allowed(allow);
    EXPECT_EQ(r.status, HttpStatus::method_not_allowed);
    EXPECT_EQ(*r.headers.get("Allow"), "GET, POST");
}
TEST(ResponseFactoryTest, NotFound) {
    auto r = ResponseFactory::not_found();
    EXPECT_EQ(r.status, HttpStatus::not_found);
    EXPECT_EQ(body_of(r), "Not Found");
}
```

- [ ] **Step 2: Add to test sources. Step 3: Build — expect failure.**

- [ ] **Step 4: Create `components/http/types/response_factory/response_factory.hpp`**

```cpp
#pragma once

#include <span>
#include <string>
#include <string_view>

#include "../http_enums.hpp"
#include "../response/response.hpp"

namespace demiplane::http {

    /// Static, GLOBAL-HEAP, cold-path factory: error conversions
    /// (to_http_response) and library/test/synthetic responses. The hot path is
    /// RequestContext's arena-bound factories (ctx.json/ok/...). Neither sets
    /// Date/Server (drivers stamp those).
    class ResponseFactory {
    public:
        static Response ok          (std::string body = "", std::string_view ct = "text/plain");
        static Response json        (std::string body);
        static Response created     (std::string body = "", std::string_view ct = "application/json");
        static Response no_content  ();
        static Response redirect    (std::string_view location, HttpStatus status = HttpStatus::found);
        static Response not_found   (std::string body = "Not Found");
        static Response bad_request (std::string body = "Bad Request");
        static Response unauthorized(std::string body = "Unauthorized");
        static Response forbidden   (std::string body = "Forbidden");
        static Response conflict    (std::string body = "Conflict");
        static Response payload_too_large   (std::string body = "Payload Too Large");
        static Response unprocessable_entity(std::string body = "Unprocessable Entity");
        static Response internal_error      (std::string body = "Internal Server Error");
        static Response method_not_allowed  (std::span<const HttpMethod> allow);
        static Response custom(HttpStatus status, std::string body, std::string_view ct = "text/plain");
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/types/response_factory/response_factory.cpp`**

```cpp
#include "response_factory.hpp"

#include <utility>

namespace demiplane::http {

    namespace {
        Response with_body(HttpStatus s, std::string body, std::string_view ct) {
            Response r;                               // default alloc = new_delete (cold path)
            r.status = s;
            r.add_header("Content-Type", ct);
            r.body = Body::owned(std::move(body));
            return r;
        }
    }

    Response ResponseFactory::ok(std::string b, std::string_view ct)      { return with_body(HttpStatus::ok, std::move(b), ct); }
    Response ResponseFactory::json(std::string b)                         { return with_body(HttpStatus::ok, std::move(b), "application/json"); }
    Response ResponseFactory::created(std::string b, std::string_view ct) { return with_body(HttpStatus::created, std::move(b), ct); }

    Response ResponseFactory::no_content() {
        Response r; r.status = HttpStatus::no_content; return r;   // EmptyBody, no Content-Type
    }
    Response ResponseFactory::redirect(std::string_view location, HttpStatus status) {
        Response r; r.status = status; r.add_header("Location", location); return r;
    }

    Response ResponseFactory::not_found(std::string b)            { return with_body(HttpStatus::not_found, std::move(b), "text/plain"); }
    Response ResponseFactory::bad_request(std::string b)          { return with_body(HttpStatus::bad_request, std::move(b), "text/plain"); }
    Response ResponseFactory::unauthorized(std::string b)         { return with_body(HttpStatus::unauthorized, std::move(b), "text/plain"); }
    Response ResponseFactory::forbidden(std::string b)            { return with_body(HttpStatus::forbidden, std::move(b), "text/plain"); }
    Response ResponseFactory::conflict(std::string b)             { return with_body(HttpStatus::conflict, std::move(b), "text/plain"); }
    Response ResponseFactory::payload_too_large(std::string b)    { return with_body(HttpStatus::payload_too_large, std::move(b), "text/plain"); }
    Response ResponseFactory::unprocessable_entity(std::string b) { return with_body(HttpStatus::unprocessable_entity, std::move(b), "text/plain"); }
    Response ResponseFactory::internal_error(std::string b)       { return with_body(HttpStatus::internal_server_error, std::move(b), "text/plain"); }

    Response ResponseFactory::method_not_allowed(std::span<const HttpMethod> allow) {
        Response r; r.status = HttpStatus::method_not_allowed;
        std::string v; bool first = true;
        for (auto m : allow) { if (!first) v += ", "; v += to_string(m); first = false; }
        r.add_header("Allow", v);
        return r;
    }
    Response ResponseFactory::custom(HttpStatus s, std::string b, std::string_view ct) {
        return with_body(s, std::move(b), ct);
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Wire source; Step 7: Build + run — expect pass; Step 8: Commit**

```bash
git add components/http/types/response_factory tests/unit_tests/http/types/test_response_factory.cpp components/http/types/CMakeLists.txt tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add static cold-path ResponseFactory

Global-heap factory for ctx-less contexts (errors, tests). The hot path is
ctx.json/ok/... (Task 15). No Date/Server header (drivers stamp those)."
```

---

## Task 11: errors.cpp — to_http_response definitions

**Files:** Create `errors/errors.cpp`, `tests/unit_tests/http/types/test_errors.cpp`; Modify both CMakeLists.

**Goal:** Implement every `to_http_response(const E&)` via the static `ResponseFactory` (cold path, global heap).

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/types/test_errors.cpp`

```cpp
#include <gtest/gtest.h>

#include <body/body.hpp>
#include <errors/errors.hpp>
#include <response/response.hpp>

using namespace demiplane::http;

namespace { std::string body_of(const Response& r) {
    return std::string{r.body.buffered_view().value_or("")};
}}

TEST(ToHttpResponseTest, BadRequest400)   { auto r = to_http_response(BadRequestError{"x"}); EXPECT_EQ(r.status, HttpStatus::bad_request); EXPECT_EQ(body_of(r), "x"); }
TEST(ToHttpResponseTest, Unauthorized401) { EXPECT_EQ(to_http_response(UnauthorizedError{"t"}).status, HttpStatus::unauthorized); }
TEST(ToHttpResponseTest, Forbidden403)    { EXPECT_EQ(to_http_response(ForbiddenError{"n"}).status, HttpStatus::forbidden); }
TEST(ToHttpResponseTest, NotFound404)     { auto r = to_http_response(NotFoundError{"user","42"}); EXPECT_EQ(r.status, HttpStatus::not_found); EXPECT_EQ(body_of(r), "user 42 not found"); }
TEST(ToHttpResponseTest, Conflict409)     { EXPECT_EQ(to_http_response(ConflictError{"e"}).status, HttpStatus::conflict); }
TEST(ToHttpResponseTest, UnprocessableIncludesFields) {
    UnprocessableEntityError e{"bad", {{"name","required"},{"age","positive"}}};
    auto r = to_http_response(e);
    EXPECT_EQ(r.status, HttpStatus::unprocessable_entity);
    auto b = body_of(r);
    EXPECT_NE(b.find("name: required"), std::string::npos);
    EXPECT_NE(b.find("age: positive"), std::string::npos);
}
TEST(ToHttpResponseTest, PayloadTooLarge413) { auto r = to_http_response(PayloadTooLargeError{1024}); EXPECT_EQ(r.status, HttpStatus::payload_too_large); EXPECT_NE(body_of(r).find("1024"), std::string::npos); }
TEST(ToHttpResponseTest, MethodNotAllowedAllow) {
    MethodNotAllowedError e{{HttpMethod::get, HttpMethod::head}};
    auto r = to_http_response(e);
    EXPECT_EQ(r.status, HttpStatus::method_not_allowed);
    EXPECT_EQ(*r.headers.get("Allow"), "GET, HEAD");
}
TEST(ToHttpResponseTest, JsonParse400)     { EXPECT_EQ(to_http_response(JsonParseError{"oops"}).status, HttpStatus::bad_request); }
TEST(ToHttpResponseTest, BodyLimit413)     { EXPECT_EQ(to_http_response(BodyLimitExceeded{16}).status, HttpStatus::payload_too_large); }
```

- [ ] **Step 2: Add to test sources. Step 3: Build — expect link failure (defs missing).**

- [ ] **Step 4: Create `components/http/types/errors/errors.cpp`**

```cpp
#include "errors.hpp"

#include <sstream>

#include "../response/response.hpp"
#include "../response_factory/response_factory.hpp"

namespace demiplane::http {

    Response to_http_response(const BadRequestError& e)   { return ResponseFactory::bad_request(e.message); }
    Response to_http_response(const UnauthorizedError& e) { return ResponseFactory::unauthorized(e.message); }
    Response to_http_response(const ForbiddenError& e)    { return ResponseFactory::forbidden(e.message); }

    Response to_http_response(const NotFoundError& e) {
        std::string body = e.resource;
        if (!e.id.empty()) { body += ' '; body += e.id; }
        body += " not found";
        return ResponseFactory::not_found(std::move(body));
    }
    Response to_http_response(const ConflictError& e) { return ResponseFactory::conflict(e.message); }

    Response to_http_response(const UnprocessableEntityError& e) {
        std::ostringstream os; os << e.message;
        for (const auto& f : e.fields) os << "\n" << f.field << ": " << f.detail;
        return ResponseFactory::unprocessable_entity(os.str());
    }
    Response to_http_response(const PayloadTooLargeError& e) {
        std::ostringstream os; os << "Payload Too Large (limit " << e.limit << " bytes)";
        return ResponseFactory::payload_too_large(os.str());
    }
    Response to_http_response(const MethodNotAllowedError& e) {
        return ResponseFactory::method_not_allowed(e.allowed);
    }
    Response to_http_response(const JsonParseError& e)      { return ResponseFactory::bad_request("JSON parse error: " + e.detail); }
    Response to_http_response(const FormParseError& e)      { return ResponseFactory::bad_request("Form parse error: " + e.detail); }
    Response to_http_response(const MultipartParseError& e) { return ResponseFactory::bad_request("Multipart parse error: " + e.detail); }
    Response to_http_response(const BodyLimitExceeded& e) {
        std::ostringstream os; os << "Body Limit Exceeded (" << e.limit << " bytes)";
        return ResponseFactory::payload_too_large(os.str());
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Wire source; Step 6: Build + run — expect pass; Step 7: Commit**

```bash
git add components/http/types/errors/errors.cpp tests/unit_tests/http/types/test_errors.cpp components/http/types/CMakeLists.txt tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): implement to_http_response for all built-in errors (cold path)"
```

---

## Task 12: Body buffered helpers

**Files:** Modify `body/body.cpp`, `tests/unit_tests/http/types/test_body.cpp`.

**Goal:** `read_to_string`/`read_json`/`read_form`/`read_multipart` drive `read_chunk()` with a limit, returning `AsyncOutcome` with typed failure. Uses the shared `url_decode` (Task 5).

- [ ] **Step 1: Append failing tests to `test_body.cpp`**

```cpp
#include <url_decode/url_decode.hpp>   // (top of file)

TEST(BodyTest, ReadToStringSucceeds) {
    Body b = Body::owned("hello");
    auto o = run_awaitable(b.read_to_string(100));
    ASSERT_TRUE(o.is_success());
    EXPECT_EQ(o.value(), "hello");
}
TEST(BodyTest, ReadToStringLimitExceeded) {
    Body b = Body::owned("hello");
    auto o = run_awaitable(b.read_to_string(3));
    ASSERT_TRUE(o.is_error());
    EXPECT_TRUE(o.holds_error<BodyLimitExceeded>());
}
TEST(BodyTest, ReadJsonSucceeds) {
    Body b = Body::owned(R"({"a":1,"b":"two"})");
    auto o = run_awaitable(b.read_json(1024));
    ASSERT_TRUE(o.is_success());
    EXPECT_EQ(o.value()["a"].asInt(), 1);
    EXPECT_EQ(o.value()["b"].asString(), "two");
}
TEST(BodyTest, ReadJsonMalformed) {
    Body b = Body::owned("not json");
    auto o = run_awaitable(b.read_json(1024));
    ASSERT_TRUE(o.is_error());
    EXPECT_TRUE(o.holds_error<JsonParseError>());
}
TEST(BodyTest, ReadFormUrlDecodes) {
    Body b = Body::owned("name=John%20Doe&city=New+York&empty=");
    auto o = run_awaitable(b.read_form(1024));
    ASSERT_TRUE(o.is_success());
    EXPECT_EQ(o.value().at("name"), "John Doe");
    EXPECT_EQ(o.value().at("city"), "New York");
    EXPECT_EQ(o.value().at("empty"), "");
}
TEST(BodyTest, ReadFormEmptyKeyIsError) {
    Body b = Body::owned("=value");
    auto o = run_awaitable(b.read_form(1024));
    ASSERT_TRUE(o.is_error());
    EXPECT_TRUE(o.holds_error<FormParseError>());
}
TEST(BodyTest, ReadMultipartNoBoundaryIsError) {
    Body b = Body::owned("x");
    auto o = run_awaitable(b.read_multipart(1024, ""));
    ASSERT_TRUE(o.is_error());
    EXPECT_TRUE(o.holds_error<MultipartParseError>());
}
TEST(BodyTest, ReadMultipartWellFormed) {
    const std::string boundary = "X";
    std::string body =
        "--X\r\nContent-Disposition: form-data; name=\"field\"\r\n\r\nvalue\r\n--X--\r\n";
    Body b = Body::owned(body);
    auto o = run_awaitable(b.read_multipart(4096, boundary));
    ASSERT_TRUE(o.is_success());
    ASSERT_EQ(o.value().size(), 1u);
    EXPECT_EQ(o.value()[0].name, "field");
    EXPECT_EQ(o.value()[0].value, "value");
}
```

- [ ] **Step 2: Build — expect link failure (helpers undefined).**

- [ ] **Step 3: Append helper definitions to `body/body.cpp`**

```cpp
#include <sstream>

#include "../url_decode/url_decode.hpp"

namespace demiplane::http {

    namespace {
        boost::asio::awaitable<gears::Outcome<std::string, BodyLimitExceeded>>
        drain(Body& body, std::size_t limit) {
            std::string out;
            while (true) {
                auto chunk = co_await body.read_chunk();
                if (!chunk) break;
                if (out.size() + chunk->size() > limit) co_return gears::err(BodyLimitExceeded{limit});
                out.append(reinterpret_cast<const char*>(chunk->data()), chunk->size());
            }
            co_return out;
        }
    }

    AsyncOutcome<std::string, BodyLimitExceeded> Body::read_to_string(std::size_t limit) {
        co_return co_await drain(*this, limit);
    }

    AsyncOutcome<Json::Value, JsonParseError, BodyLimitExceeded> Body::read_json(std::size_t limit) {
        auto d = co_await drain(*this, limit);
        if (!d.is_success()) co_return gears::err(d.error<BodyLimitExceeded>());
        Json::Value root; std::string err;
        Json::CharReaderBuilder builder;
        std::istringstream stream{std::move(d).value()};
        if (!Json::parseFromStream(builder, stream, &root, &err))
            co_return gears::err(JsonParseError{std::move(err)});
        co_return root;
    }

    AsyncOutcome<std::unordered_map<std::string, std::string>, FormParseError, BodyLimitExceeded>
    Body::read_form(std::size_t limit) {
        auto d = co_await drain(*this, limit);
        if (!d.is_success()) co_return gears::err(d.error<BodyLimitExceeded>());
        const std::string body = std::move(d).value();
        std::unordered_map<std::string, std::string> out;
        std::size_t i = 0;
        while (i < body.size()) {
            std::size_t amp = body.find('&', i);
            std::string_view pair{body.data() + i, (amp == std::string::npos ? body.size() - i : amp - i)};
            std::size_t eq = pair.find('=');
            std::string_view rk = (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
            std::string_view rv = (eq == std::string_view::npos) ? std::string_view{} : pair.substr(eq + 1);
            if (rk.empty()) co_return gears::err(FormParseError{"empty key"});
            auto k = url_decode(rk); auto v = url_decode(rv);
            if (!k || !v) co_return gears::err(FormParseError{"invalid percent-escape"});
            out[*std::move(k)] = *std::move(v);
            if (amp == std::string::npos) break;
            i = amp + 1;
        }
        co_return out;
    }

    AsyncOutcome<std::vector<MultipartField>, MultipartParseError, BodyLimitExceeded>
    Body::read_multipart(std::size_t limit, std::string_view boundary) {
        if (boundary.empty()) co_return gears::err(MultipartParseError{"empty boundary"});
        auto d = co_await drain(*this, limit);
        if (!d.is_success()) co_return gears::err(d.error<BodyLimitExceeded>());
        const std::string body = std::move(d).value();
        const std::string delim = "--" + std::string{boundary};
        std::vector<MultipartField> out;

        std::size_t pos = body.find(delim);
        if (pos == std::string::npos) co_return gears::err(MultipartParseError{"no boundary found"});
        pos += delim.size();
        while (pos < body.size()) {
            if (pos + 2 <= body.size() && body[pos] == '-' && body[pos + 1] == '-') break;  // end
            if (pos + 1 < body.size() && body[pos] == '\r' && body[pos + 1] == '\n') pos += 2;
            std::size_t header_end = body.find("\r\n\r\n", pos);
            if (header_end == std::string::npos) co_return gears::err(MultipartParseError{"unterminated headers"});
            std::string_view header_block{body.data() + pos, header_end - pos};
            std::size_t body_start = header_end + 4;
            std::size_t next = body.find("\r\n" + delim, body_start);
            if (next == std::string::npos) co_return gears::err(MultipartParseError{"unterminated part"});
            std::string_view part_body{body.data() + body_start, next - body_start};

            MultipartField field;
            std::size_t hp = 0;
            while (hp < header_block.size()) {
                std::size_t eol = header_block.find("\r\n", hp);
                std::string_view line{header_block.data() + hp,
                                      (eol == std::string_view::npos ? header_block.size() - hp : eol - hp)};
                auto ci_starts = [](std::string_view l, std::string_view p) {
                    if (l.size() < p.size()) return false;
                    for (std::size_t k = 0; k < p.size(); ++k) {
                        char a = l[k], b = p[k];
                        if (a >= 'A' && a <= 'Z') a += 32;
                        if (b >= 'A' && b <= 'Z') b += 32;
                        if (a != b) return false;
                    }
                    return true;
                };
                if (ci_starts(line, "content-disposition:")) {
                    auto param = [&](std::string_view key) -> std::string {
                        auto p = line.find(key);
                        if (p == std::string_view::npos) return {};
                        p += key.size();
                        if (p < line.size() && line[p] == '=') ++p;
                        if (p < line.size() && line[p] == '"') {
                            ++p; auto e = line.find('"', p);
                            if (e == std::string_view::npos) return {};
                            return std::string{line.substr(p, e - p)};
                        }
                        return {};
                    };
                    field.name = param("name");
                    field.filename = param("filename");
                } else if (ci_starts(line, "content-type:")) {
                    auto colon = line.find(':');
                    auto v = line.substr(colon + 1);
                    while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.remove_prefix(1);
                    field.content_type = std::string{v};
                }
                if (eol == std::string_view::npos) break;
                hp = eol + 2;
            }
            field.value = std::string{part_body};
            out.emplace_back(std::move(field));
            pos = next + 2 + delim.size();
        }
        co_return out;
    }

}  // namespace demiplane::http
```

> The multipart parser handles the well-formed common case and rejects malformed input with a typed error; transfer-encoded parts and very large file streaming are out of scope (the spec notes large-upload streaming is a driver-era concern).

- [ ] **Step 4: Build + run — expect pass; Step 5: Commit**

```bash
git add components/http/types/body/body.cpp tests/unit_tests/http/types/test_body.cpp
git commit -m "feat(http/types): implement Body buffered helpers (string/json/form/multipart)

Drive read_chunk() with a size limit; typed failure via AsyncOutcome. Form/
multipart use the shared url_decode. Malformed input surfaces a parse error."
```

---

## Task 13: RequestContext — accessors, predicates, target-as-view split

**Files:** Create `request_context/request_context.{hpp,cpp}`, `tests/unit_tests/http/types/test_request_context.cpp`; Modify both CMakeLists.

**Goal:** Construction, header lookup, body access, content-type/Accept predicates, and the lazy `target`→(path, query) split. Because `target` is a **view into stable external storage** (not an owned SSO string), caching the split is safe across `RequestContext` moves.

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/types/test_request_context.cpp`

```cpp
#include <deque>
#include <memory_resource>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <body/body.hpp>
#include <headers/headers.hpp>
#include <request/request.hpp>
#include <request_context/request_context.hpp>

using namespace demiplane::http;

class RequestContextTest : public ::testing::Test {
protected:
    std::pmr::monotonic_buffer_resource resource_{8192};
    std::pmr::polymorphic_allocator<> alloc_{&resource_};
    std::deque<std::string> target_storage_;   // stable backing for string_view targets

    Request make_request(HttpMethod m, std::string target,
                         std::vector<std::pair<std::string, std::string>> hdrs = {},
                         std::string body_text = "") {
        Request req{Headers::owned(alloc_)};
        req.method  = m;
        req.version = HttpVersion::http_1_1;
        target_storage_.push_back(std::move(target));
        req.target  = target_storage_.back();          // view into stable storage
        for (auto const& [k, v] : hdrs) req.headers.add(k, v);
        req.body = body_text.empty() ? Body::empty() : Body::owned(std::move(body_text));
        return req;
    }
};

TEST_F(RequestContextTest, MethodTargetVersion) {
    RequestContext ctx{make_request(HttpMethod::get, "/users"), alloc_};
    EXPECT_EQ(ctx.method(), HttpMethod::get);
    EXPECT_EQ(ctx.target(), "/users");
    EXPECT_EQ(ctx.version(), HttpVersion::http_1_1);
}
TEST_F(RequestContextTest, HeaderLookup) {
    RequestContext ctx{make_request(HttpMethod::get, "/", {{"Host", "example.com"}}), alloc_};
    ASSERT_TRUE(ctx.header("host").has_value());
    EXPECT_EQ(*ctx.header("host"), "example.com");
    EXPECT_FALSE(ctx.header("missing").has_value());
}
TEST_F(RequestContextTest, BodyAccess) {
    RequestContext ctx{make_request(HttpMethod::post, "/", {}, "hello"), alloc_};
    EXPECT_EQ(ctx.body().size_hint().value_or(0), 5u);
}
TEST_F(RequestContextTest, ContentTypePredicates) {
    EXPECT_TRUE(RequestContext{make_request(HttpMethod::post, "/", {{"Content-Type","application/json"}}), alloc_}.is_json());
    EXPECT_TRUE(RequestContext{make_request(HttpMethod::post, "/", {{"Content-Type","application/x-www-form-urlencoded"}}), alloc_}.is_form());
    EXPECT_TRUE(RequestContext{make_request(HttpMethod::post, "/", {{"Content-Type","multipart/form-data; boundary=xx"}}), alloc_}.is_multipart());
}
TEST_F(RequestContextTest, PathSplit) {
    RequestContext ctx{make_request(HttpMethod::get, "/users/42?q=foo&p=bar"), alloc_};
    EXPECT_EQ(ctx.path(), "/users/42");
    EXPECT_EQ(ctx.query_string(), "q=foo&p=bar");
}
TEST_F(RequestContextTest, PathSplitNoQuery) {
    RequestContext ctx{make_request(HttpMethod::get, "/users/42"), alloc_};
    EXPECT_EQ(ctx.path(), "/users/42");
    EXPECT_EQ(ctx.query_string(), "");
}
TEST_F(RequestContextTest, CachedPathSurvivesMove) {
    RequestContext a{make_request(HttpMethod::get, "/u"), alloc_};  // short (SSO-length) target
    EXPECT_EQ(a.path(), "/u");                                      // populate the cache
    RequestContext b{std::move(a)};                                 // move after caching
    EXPECT_EQ(b.path(), "/u");                                      // view still valid (target is a view)
}
```

- [ ] **Step 2: Add to test sources. Step 3: Build — expect failure.**

- [ ] **Step 4: Create `components/http/types/request_context/request_context.hpp`**

```cpp
#pragma once

#include <memory_resource>
#include <optional>
#include <string_view>

#include "../body/body.hpp"
#include "../headers/headers.hpp"
#include "../http_enums.hpp"
#include "../request/request.hpp"

namespace demiplane::http {

    /**
     * @brief Handler-facing view of one in-flight request.
     *
     * Owns the moved-in Request + the request arena allocator. Header lookup is
     * lazy. The target→(path, query) split is memoized; since `target` is a VIEW
     * into stable connection-owned storage, the cached views survive moves (the
     * old owned-string SSO dangle is gone). Move-only; passed by value into
     * handlers. Valid only for the handler's duration.
     */
    class RequestContext {
    public:
        RequestContext(Request req, std::pmr::polymorphic_allocator<> alloc);

        RequestContext(RequestContext&&)            = default;
        RequestContext& operator=(RequestContext&&) = default;
        RequestContext(const RequestContext&)            = delete;
        RequestContext& operator=(const RequestContext&) = delete;

        HttpMethod  method()  const noexcept { return request_.method; }
        HttpVersion version() const noexcept { return request_.version; }
        std::string_view target() const noexcept { return request_.target; }
        std::string_view path()         const;
        std::string_view query_string() const;
        const Headers& headers() const noexcept { return request_.headers; }
        Body& body() noexcept { return request_.body; }

        std::optional<std::string_view> header(std::string_view name) const {
            return request_.headers.get(name);
        }
        std::string header_or(std::string_view name, std::string_view fallback) const {
            return request_.headers.get_or(name, fallback);
        }

        bool is_json()      const;
        bool is_form()      const;
        bool is_multipart() const;
        bool accepts_json() const;
        bool accepts_html() const;

        std::pmr::polymorphic_allocator<> arena_alloc() const noexcept { return alloc_; }

    private:
        Request request_;
        std::pmr::polymorphic_allocator<> alloc_;

        mutable std::optional<std::string_view> cached_path_;
        mutable std::optional<std::string_view> cached_query_;
        void ensure_split() const;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/types/request_context/request_context.cpp`**

```cpp
#include "request_context.hpp"

#include <utility>

namespace demiplane::http {

    RequestContext::RequestContext(Request req, std::pmr::polymorphic_allocator<> alloc)
        : request_{std::move(req)}, alloc_{alloc} {}

    void RequestContext::ensure_split() const {
        if (cached_path_.has_value()) return;
        std::string_view t = request_.target;
        auto q = t.find('?');
        if (q == std::string_view::npos) { cached_path_ = t; cached_query_ = std::string_view{}; }
        else { cached_path_ = t.substr(0, q); cached_query_ = t.substr(q + 1); }
    }
    std::string_view RequestContext::path() const { ensure_split(); return *cached_path_; }
    std::string_view RequestContext::query_string() const { ensure_split(); return *cached_query_; }

    namespace {
        bool has(std::string_view hay, std::string_view needle) {
            return hay.find(needle) != std::string_view::npos;
        }
    }

    bool RequestContext::is_json()      const { auto ct = header("content-type"); return ct && has(*ct, "application/json"); }
    bool RequestContext::is_form()      const { auto ct = header("content-type"); return ct && has(*ct, "application/x-www-form-urlencoded"); }
    bool RequestContext::is_multipart() const { auto ct = header("content-type"); return ct && has(*ct, "multipart/form-data"); }
    bool RequestContext::accepts_json() const { auto a = header("accept"); return a && (has(*a, "application/json") || has(*a, "*/*")); }
    bool RequestContext::accepts_html() const { auto a = header("accept"); return a && (has(*a, "text/html") || has(*a, "*/*")); }

}  // namespace demiplane::http
```

> `accepts_json()`/predicates use substring matching and treat `*/*` as acceptance (browser caveat in the design review) — kept simple for v1; strict content negotiation is a follow-up if needed.

- [ ] **Step 6: Wire source; Step 7: Build + run — expect pass; Step 8: Commit**

```bash
git add components/http/types/request_context tests/unit_tests/http/types/test_request_context.cpp components/http/types/CMakeLists.txt tests/unit_tests/CMakeLists.txt
git commit -m "feat(http/types): add RequestContext accessors + predicates + target-as-view split

Lazy path/query split memoized over the string_view target — survives moves
(no owned-string SSO dangle). Content-Type/Accept predicates."
```

---

## Task 14: RequestContext — path/query params (templates IN THE HEADER)

**Files:** Modify `request_context/{hpp,cpp}`, `test_request_context.cpp`.

**Goal (reconciled):** `path_param<T>` / `path_param_or<T>` / `query<T>` / `query_or<T>` for **any arithmetic or string type** — **definitions live in the header** so consumers instantiate them for their own `T` (no `.cpp` explicit-instantiation list, no `query<size_t>` link error). Query parsed lazily; URL-decoded throughout. `set_path_param` is the routing-layer hook (PR2).

- [ ] **Step 1: Append failing tests** — including the types the old plan could not link

```cpp
TEST_F(RequestContextTest, QueryUrlDecodedTypedConversions) {
    RequestContext ctx{make_request(HttpMethod::get, "/?name=John%20Doe&n=42&city=New+York"), alloc_};
    EXPECT_EQ(ctx.query<std::string>("name").value_or(""), "John Doe");
    EXPECT_EQ(ctx.query<int>("n").value_or(0), 42);
    EXPECT_EQ(ctx.query<std::string>("city").value_or(""), "New York");
    EXPECT_FALSE(ctx.query<int>("missing").has_value());
}
TEST_F(RequestContextTest, QueryArbitraryArithmeticTypesLink) {
    RequestContext ctx{make_request(HttpMethod::get, "/?p=7"), alloc_};
    EXPECT_EQ(ctx.query<std::size_t>("p").value_or(0), 7u);   // these would NOT link in the old plan
    EXPECT_EQ(ctx.query<unsigned>("p").value_or(0), 7u);
    EXPECT_DOUBLE_EQ(ctx.query<double>("p").value_or(0.0), 7.0);
}
TEST_F(RequestContextTest, QueryOrFallback) {
    RequestContext ctx{make_request(HttpMethod::get, "/?n=10"), alloc_};
    EXPECT_EQ(ctx.query_or<int>("n", 99), 10);
    EXPECT_EQ(ctx.query_or<int>("missing", 99), 99);
}
TEST_F(RequestContextTest, PathParamSetAndConvert) {
    RequestContext ctx{make_request(HttpMethod::get, "/users/42"), alloc_};
    ctx.set_path_param("id", "42");
    EXPECT_EQ(ctx.path_param<int>("id").value_or(0), 42);
    EXPECT_EQ(ctx.path_param_or<std::string>("id", "x"), "42");
    EXPECT_FALSE(ctx.path_param<int>("missing").has_value());
}
TEST_F(RequestContextTest, PathParamConvertFailure) {
    RequestContext ctx{make_request(HttpMethod::get, "/users/abc"), alloc_};
    ctx.set_path_param("id", "abc");
    EXPECT_FALSE(ctx.path_param<int>("id").has_value());
    EXPECT_EQ(ctx.path_param<std::string>("id").value_or(""), "abc");
}
```

- [ ] **Step 2: Build — expect failure.**

- [ ] **Step 3: Edit `request_context.hpp`** — add includes, storage, public template methods (defined inline), and a private `convert_string<T>`.

Add includes:

```cpp
#include <charconv>
#include <string>
#include <type_traits>

#include <boost/container/small_vector.hpp>
```

Add to the public section:

```cpp
        // ── Path parameters (set by the routing layer, PR2) ───────────────
        void set_path_param(std::string_view name, std::string_view value);

        template <typename T>
        std::optional<T> path_param(std::string_view name) const {
            auto raw = raw_path_param(name);
            return raw ? convert_string<T>(*raw) : std::nullopt;
        }
        template <typename T>
        T path_param_or(std::string_view name, T fallback) const {
            if (auto v = path_param<T>(name)) return *std::move(v);
            return fallback;
        }

        // ── Query parameters (lazily parsed from query_string) ────────────
        template <typename T>
        std::optional<T> query(std::string_view name) const {
            auto raw = raw_query(name);
            return raw ? convert_string<T>(*raw) : std::nullopt;
        }
        template <typename T>
        T query_or(std::string_view name, T fallback) const {
            if (auto v = query<T>(name)) return *std::move(v);
            return fallback;
        }
```

Add to the private section:

```cpp
        using ParamEntry = std::pair<std::pmr::string, std::pmr::string>;
        using ParamVec = boost::container::small_vector<
            ParamEntry, 4, std::pmr::polymorphic_allocator<ParamEntry>>;

        ParamVec path_params_{std::pmr::polymorphic_allocator<ParamEntry>{alloc_}};
        mutable bool query_parsed_ = false;
        mutable ParamVec query_params_{std::pmr::polymorphic_allocator<ParamEntry>{alloc_}};

        void ensure_query_parsed() const;
        std::optional<std::string_view> raw_query(std::string_view name) const;
        std::optional<std::string_view> raw_path_param(std::string_view name) const;

        // Defined in the header so consumers instantiate for their own T —
        // no .cpp explicit-instantiation list, no link cap on the type set.
        template <typename T>
        static std::optional<T> convert_string(std::string_view value) {
            if constexpr (std::is_same_v<T, std::string>) {
                return std::string{value};
            } else if constexpr (std::is_same_v<T, std::string_view>) {
                return value;
            } else if constexpr (std::is_arithmetic_v<T>) {
                T out{};
                auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
                if (ec != std::errc{} || ptr != value.data() + value.size()) return std::nullopt;
                return out;
            } else {
                static_assert(sizeof(T) == 0, "RequestContext: unsupported param type");
            }
        }
```

> Member-init order: `alloc_` is declared before `path_params_`/`query_params_`, so the small_vectors' allocator references the live `alloc_`.

- [ ] **Step 4: Append helper definitions to `request_context.cpp`**

```cpp
#include "../url_decode/url_decode.hpp"

namespace demiplane::http {

    void RequestContext::set_path_param(std::string_view name, std::string_view value) {
        path_params_.emplace_back(std::pmr::string{name, alloc_}, std::pmr::string{value, alloc_});
    }

    std::optional<std::string_view> RequestContext::raw_path_param(std::string_view name) const {
        for (const auto& [k, v] : path_params_)
            if (std::string_view(k) == name) return std::string_view(v);
        return std::nullopt;
    }

    void RequestContext::ensure_query_parsed() const {
        if (query_parsed_) return;
        query_parsed_ = true;
        std::string_view qs = query_string();
        std::size_t i = 0;
        while (i < qs.size()) {
            std::size_t amp = qs.find('&', i);
            std::string_view pair{qs.data() + i, (amp == std::string_view::npos ? qs.size() - i : amp - i)};
            std::size_t eq = pair.find('=');
            std::string_view rk = (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
            std::string_view rv = (eq == std::string_view::npos) ? std::string_view{} : pair.substr(eq + 1);
            auto k = url_decode(rk), v = url_decode(rv);
            if (k && v) query_params_.emplace_back(std::pmr::string{*k, alloc_}, std::pmr::string{*v, alloc_});
            // Malformed escapes are skipped here (see review note); a handler
            // wanting strict parsing uses body().read_form().
            if (amp == std::string_view::npos) break;
            i = amp + 1;
        }
    }

    std::optional<std::string_view> RequestContext::raw_query(std::string_view name) const {
        ensure_query_parsed();
        for (const auto& [k, v] : query_params_)
            if (std::string_view(k) == name) return std::string_view(v);
        return std::nullopt;
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Build + run — expect pass; Step 6: Commit**

```bash
git add components/http/types/request_context tests/unit_tests/http/types/test_request_context.cpp
git commit -m "feat(http/types): add path/query params (templates in header, URL-decoded)

query<T>/path_param<T> defined in the header so any arithmetic/string T
instantiates in consumer code — fixes the old .cpp explicit-instantiation
link cap (query<size_t> et al. now link). Arena-backed small_vector storage."
```

---

## Task 15: RequestContext — type-keyed bag + ctx-scoped response factories

**Files:** Modify `request_context/{hpp,cpp}`, `test_request_context.cpp`.

**Goal:** (a) `set<T>`/`get<T>`/`has<T>` type-keyed middleware bag (arena-backed, header-defined for user types); (b) the **ctx-scoped, arena-bound response factories** — `ok`/`json`/`created`/`no_content`/`redirect`/`status` — that build a `Response` whose headers + stored allocator point at the request arena (spec §5.4, option A). (`stream` is deferred to the driver PR.)

- [ ] **Step 1: Append failing tests**

```cpp
struct TraceId { std::string value; };
struct UserPrincipal { int id; std::string name; };

TEST_F(RequestContextTest, BagSetGetHas) {
    RequestContext ctx{make_request(HttpMethod::get, "/"), alloc_};
    EXPECT_FALSE(ctx.has<TraceId>());
    EXPECT_EQ(ctx.get<TraceId>(), nullptr);
    ctx.set<TraceId>(TraceId{"abc-123"});
    ASSERT_TRUE(ctx.has<TraceId>());
    EXPECT_EQ(ctx.get<TraceId>()->value, "abc-123");
}
TEST_F(RequestContextTest, BagDifferentTypesCoexistAndReplace) {
    RequestContext ctx{make_request(HttpMethod::get, "/"), alloc_};
    ctx.set<TraceId>(TraceId{"a"});
    ctx.set<UserPrincipal>(UserPrincipal{42, "alice"});
    ctx.set<TraceId>(TraceId{"b"});                 // replace
    EXPECT_EQ(ctx.get<TraceId>()->value, "b");
    EXPECT_EQ(ctx.get<UserPrincipal>()->name, "alice");
}
TEST_F(RequestContextTest, CtxJsonBuildsArenaBackedResponse) {
    RequestContext ctx{make_request(HttpMethod::get, "/"), alloc_};
    Response r = ctx.json("{\"ok\":true}");
    EXPECT_EQ(r.status, HttpStatus::ok);
    EXPECT_EQ(*r.headers.get("Content-Type"), "application/json");
    EXPECT_EQ(*r.body.buffered_view(), "{\"ok\":true}");
    EXPECT_EQ(r.alloc.resource(), ctx.arena_alloc().resource());   // arena-bound
}
TEST_F(RequestContextTest, CtxOkAndStatusAndNoContent) {
    RequestContext ctx{make_request(HttpMethod::get, "/"), alloc_};
    EXPECT_EQ(ctx.ok("hi").status, HttpStatus::ok);
    EXPECT_EQ(ctx.no_content().status, HttpStatus::no_content);
    EXPECT_EQ(ctx.status(HttpStatus::accepted, "queued").status, HttpStatus::accepted);
}

// Proves the bag runs each payload's destructor EXACTLY ONCE across a move
// (RequestContext is moved by value through the middleware chain). A regression
// to a fragile BagEntry move would double-destroy: `live` goes to -1 here, and
// the owning std::string member double-frees under ASan (Task 17).
TEST_F(RequestContextTest, BagDestructorRunsExactlyOnceAcrossMove) {
    static int live = 0;
    struct Tracked {
        std::string s = "payload";              // owning member -> double-free under ASan if double-destroyed
        Tracked() { ++live; }
        Tracked(Tracked&& o) noexcept : s(std::move(o.s)) { ++live; }
        ~Tracked() { --live; }
    };
    {
        RequestContext a{make_request(HttpMethod::get, "/"), alloc_};
        a.set<Tracked>(Tracked{});
        RequestContext b{std::move(a)};         // move a populated-bag context
    }                                           // both a and b destruct here
    EXPECT_EQ(live, 0);                         // exactly one destruction (-1 would mean double-destroy)
}
```

- [ ] **Step 2: Build — expect failure.**

- [ ] **Step 3: Edit `request_context.hpp`** — add includes, response include, bag + factory API. Add a non-trivial destructor (the bag runs payload destructors).

Add includes:

```cpp
#include <new>
#include <typeindex>

#include "../response/response.hpp"
```

Public section — bag + factories:

```cpp
        // ── Type-keyed middleware bag (arena-backed) ──────────────────────
        template <typename T> void set(T value) {
            static_assert(std::is_move_constructible_v<T>);
            std::type_index key{typeid(T)};
            if (auto* e = find_bag_entry(key)) {
                e->destroyer(e->ptr);
                void* mem = alloc_.allocate_bytes(sizeof(T), alignof(T));
                ::new (mem) T(std::move(value));
                e->ptr = mem;
                e->destroyer = +[](void* p) noexcept { static_cast<T*>(p)->~T(); };
                return;
            }
            void* mem = alloc_.allocate_bytes(sizeof(T), alignof(T));
            ::new (mem) T(std::move(value));
            bag_.push_back(BagEntry{key, mem, +[](void* p) noexcept { static_cast<T*>(p)->~T(); }});
        }
        template <typename T> T* get() {
            auto* e = find_bag_entry(std::type_index{typeid(T)});
            return e ? static_cast<T*>(e->ptr) : nullptr;
        }
        template <typename T> const T* get() const {
            auto* e = find_bag_entry(std::type_index{typeid(T)});
            return e ? static_cast<const T*>(e->ptr) : nullptr;
        }
        template <typename T> bool has() const {
            return find_bag_entry(std::type_index{typeid(T)}) != nullptr;
        }

        // ── Arena-bound response factories (hot path; spec §5.4) ──────────
        Response ok        (std::string body = "", std::string_view ct = "text/plain");
        Response json      (std::string body);
        Response created   (std::string body = "", std::string_view ct = "application/json");
        Response no_content();
        Response redirect  (std::string_view location, HttpStatus status = HttpStatus::found);
        Response status    (HttpStatus s, std::string body = "", std::string_view ct = "text/plain");

        ~RequestContext();
```

Private section — bag storage + lookup:

```cpp
        struct BagEntry {
            std::type_index key;
            void* ptr;
            void (*destroyer)(void*) noexcept;

            BagEntry(std::type_index k, void* p, void (*d)(void*) noexcept) noexcept
                : key{k}, ptr{p}, destroyer{d} {}
            // Move nulls the source ptr, so a moved-from RequestContext's bag
            // runs NO destroyers — double-destruction is impossible regardless
            // of small_vector's moved-from element behaviour. (~RequestContext
            // guards on `ptr`.) RequestContext is moved by value through the
            // middleware chain carrying a populated bag, so this path is hot.
            BagEntry(BagEntry&& o) noexcept
                : key{o.key}, ptr{o.ptr}, destroyer{o.destroyer} { o.ptr = nullptr; }
            BagEntry& operator=(BagEntry&& o) noexcept {
                key = o.key; ptr = o.ptr; destroyer = o.destroyer; o.ptr = nullptr; return *this;
            }
            BagEntry(const BagEntry&) = delete;
            BagEntry& operator=(const BagEntry&) = delete;
        };
        boost::container::small_vector<BagEntry, 4,
            std::pmr::polymorphic_allocator<BagEntry>> bag_{
                std::pmr::polymorphic_allocator<BagEntry>{alloc_}};

        BagEntry* find_bag_entry(std::type_index key) {
            for (auto& e : bag_) if (e.key == key) return &e;
            return nullptr;
        }
        const BagEntry* find_bag_entry(std::type_index key) const {
            for (auto& e : bag_) if (e.key == key) return const_cast<BagEntry*>(&e);
            return nullptr;
        }
```

> The bag bumps fresh arena storage on replace (the old bytes leak until the per-request arena `reset()` — by design for a monotonic resource). `~RequestContext` runs the payload destructors, guarding on `ptr`. `BagEntry`'s move ctor nulls the source `ptr`, so a moved-from context runs no destroyers — double-destruction is impossible regardless of `small_vector`'s moved-from element behaviour (the `BagDestructorRunsExactlyOnceAcrossMove` test + the Task 17 ASan run cover this).

- [ ] **Step 4: Append definitions to `request_context.cpp`** (no new includes — `Response`/`Body` are already visible via `request_context.hpp`; ctx builds responses directly in the arena, it does **not** route through the static `ResponseFactory`)

```cpp
namespace demiplane::http {

    RequestContext::~RequestContext() {
        for (auto& e : bag_) if (e.destroyer && e.ptr) e.destroyer(e.ptr);
    }

    namespace {
        Response build(std::pmr::polymorphic_allocator<> a, HttpStatus s,
                       std::string body, std::string_view ct, bool with_ct) {
            Response r{a};                       // alloc + headers bound to the arena
            r.status = s;
            if (with_ct) r.add_header("Content-Type", ct);
            if (!body.empty()) r.body = Body::owned(std::move(body));
            return r;
        }
    }

    Response RequestContext::ok(std::string b, std::string_view ct)      { return build(alloc_, HttpStatus::ok, std::move(b), ct, true); }
    Response RequestContext::json(std::string b)                         { return build(alloc_, HttpStatus::ok, std::move(b), "application/json", true); }
    Response RequestContext::created(std::string b, std::string_view ct) { return build(alloc_, HttpStatus::created, std::move(b), ct, true); }
    Response RequestContext::no_content()                                { return build(alloc_, HttpStatus::no_content, "", "", false); }
    Response RequestContext::status(HttpStatus s, std::string b, std::string_view ct) { return build(alloc_, s, std::move(b), ct, true); }

    Response RequestContext::redirect(std::string_view location, HttpStatus s) {
        Response r{alloc_};
        r.status = s;
        r.add_header("Location", location);
        return r;
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Confirm the bag templates live in the header** — `set<T>`/`get<T>`/`has<T>` are defined inline in the header above (user code instantiates them), `find_bag_entry`/`~RequestContext` in the `.cpp`. Confirm `<new>` and `<typeindex>` are included in the header.

- [ ] **Step 6: Build + run — expect pass; Step 7: Commit**

```bash
git add components/http/types/request_context tests/unit_tests/http/types/test_request_context.cpp
git commit -m "feat(http/types): add type-keyed bag + ctx-scoped arena response factories

set<T>/get<T>/has<T> arena-backed bag (header-defined for user types).
ctx.json/ok/created/no_content/redirect/status build Responses whose headers
and stored allocator point at the request arena (spec option A). ~RequestContext
runs bag destructors."
```

---

## Task 16: Allocation gate (response side) — the invariant test

**Files:** Create `tests/unit_tests/http/types/test_allocation_gate.cpp`; Modify `tests/unit_tests/CMakeLists.txt`.

**Goal (spec §11/§14):** Prove the zero-additional-allocation invariant for the **response side** (all PR1 can verify — no driver/connection arena yet). A counting `std::pmr::memory_resource` is installed as the global-default resource; building a success response via `ctx.json(...)` / `ctx.ok(...)` must perform **zero** global-heap framework allocations. The only permitted global allocation is the user's body string, which is constructed *before* the measured region.

- [ ] **Step 1: Write the test**

```cpp
#include <array>
#include <atomic>
#include <cstddef>
#include <memory_resource>
#include <string>

#include <gtest/gtest.h>

#include <body/body.hpp>
#include <headers/headers.hpp>
#include <request/request.hpp>
#include <request_context/request_context.hpp>
#include <response/response.hpp>

using namespace demiplane::http;

namespace {
    // Counts allocations routed to the upstream (global) resource.
    class CountingResource : public std::pmr::memory_resource {
    public:
        std::atomic<std::size_t> count{0};
        explicit CountingResource(std::pmr::memory_resource* up) : up_{up} {}
    private:
        void* do_allocate(std::size_t b, std::size_t a) override { ++count; return up_->allocate(b, a); }
        void  do_deallocate(void* p, std::size_t b, std::size_t a) override { up_->deallocate(p, b, a); }
        bool  do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
        std::pmr::memory_resource* up_;
    };
}

TEST(AllocationGateTest, CtxOkPerformsNoGlobalHeapAllocations) {
    CountingResource counter{std::pmr::get_default_resource()};
    auto* prev = std::pmr::set_default_resource(&counter);

    // Stack-backed arena — mirrors the real RequestArena (§6.1) and, crucially,
    // never draws from `counter`. A size-only monotonic_buffer_resource{8192}
    // would capture get_default_resource() (== &counter, just set above) as its
    // upstream and pull its first block THROUGH the counter, spuriously failing
    // the gate. Do NOT "simplify" this back to the size-only ctor.
    std::array<std::byte, 8192> buf;
    std::pmr::monotonic_buffer_resource arena{buf.data(), buf.size()};
    std::pmr::polymorphic_allocator<> arena_alloc{&arena};

    std::string target = "/health";
    Request req{Headers::owned(arena_alloc)};
    req.target = target;
    RequestContext ctx{std::move(req), arena_alloc};

    const std::size_t before = counter.count.load();
    Response r = ctx.ok();                 // empty body — pure framework path
    const std::size_t after = counter.count.load();

    std::pmr::set_default_resource(prev);
    EXPECT_EQ(after - before, 0u) << "ctx.ok() touched the global heap";
    EXPECT_EQ(*r.headers.get("Content-Type"), "text/plain");
}

TEST(AllocationGateTest, CtxJsonAllocatesNothingBeyondTheUserBody) {
    CountingResource counter{std::pmr::get_default_resource()};
    auto* prev = std::pmr::set_default_resource(&counter);

    std::array<std::byte, 8192> buf;
    std::pmr::monotonic_buffer_resource arena{buf.data(), buf.size()};   // stack-backed (see CtxOk note)
    std::pmr::polymorphic_allocator<> arena_alloc{&arena};

    std::string target = "/users/42";
    Request req{Headers::owned(arena_alloc)};
    req.target = target;
    RequestContext ctx{std::move(req), arena_alloc};

    std::string payload = R"({"id":42})";  // the user's body — constructed BEFORE the measured region
    const std::size_t before = counter.count.load();
    Response r = ctx.json(std::move(payload));   // moved in; no copy
    const std::size_t after = counter.count.load();

    std::pmr::set_default_resource(prev);
    EXPECT_EQ(after - before, 0u) << "ctx.json() framework path touched the global heap";
    EXPECT_EQ(*r.body.buffered_view(), R"({"id":42})");
}
```

> Scope note (spec §11.1): this gates the **response** side only. The request side (zero-copy `BeastRequestBody`, connection arena) does not exist until PR3; the full wire-path gate runs there. If a future libstdc++/Boost detail makes `ctx.ok()` touch the global heap (e.g. a small_vector growth path), that's a real regression to investigate, not a test to relax.

- [ ] **Step 2: Add `http/types/test_allocation_gate.cpp` to the test sources.**

- [ ] **Step 3: Build + run — expect pass.**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit -R Http.Types 2>&1 | tail -25
```

- [ ] **Step 4: Commit**

```bash
git add tests/unit_tests/http/types/test_allocation_gate.cpp tests/unit_tests/CMakeLists.txt
git commit -m "test(http/types): add response-side allocation gate

A counting memory_resource asserts ctx.ok()/ctx.json() perform zero
global-heap framework allocations (the only allowed alloc is the user's
body string, constructed outside the measured region). Enforces the
zero-additional-allocation invariant; request-side gate lands in PR3."
```

---

## Task 17: Final integration — clean build, sanitizers, drop placeholder

**Files:** verification only + remove `types_placeholder.cpp`.

**Goal:** Confirm the layer ships: clean build, all tests pass, ASan/UBSan clean (the Body SBO type-erasure and the bag's placement-new are the prime suspects).

- [ ] **Step 1: Full build**

```bash
cmake --build build/release -- -j4 2>&1 | tail -20
```

Expected: no warnings/errors; existing `http_server` still builds.

- [ ] **Step 2: Run all unit tests**

```bash
ctest --test-dir build/release --output-on-failure -L unit 2>&1 | tail -30
```

Expected: every existing test still passes; `Http.Types` is all green.

- [ ] **Step 3: ASan + UBSan** — exercises Body move/destroy, bag placement-new, and the allocation gate

```bash
cmake --preset release-sanitize 2>&1 | tail -5 || \
    cmake -B build/asan -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
cmake --build build/asan --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -15
ctest --test-dir build/asan --output-on-failure -L unit -R Http.Types 2>&1 | tail -25
```

Expected: clean. The most likely failure is in `Body` move (double-destroy / use-after-move) or the bag destroyers; fix the underlying issue before committing.

- [ ] **Step 4: Lint pass (if the project uses clang-tidy)**

```bash
find components/http/types -name '*.cpp' -o -name '*.hpp' | xargs clang-tidy -p build/release --quiet 2>&1 | head -50
```

- [ ] **Step 5: Drop the bootstrap placeholder**

```bash
git rm components/http/types/types_placeholder.cpp
sed -i '/types_placeholder.cpp/d' components/http/types/CMakeLists.txt
cmake --build build/release --target Demiplane.Component.HTTP.Types -- -j4 2>&1 | tail -5
```

- [ ] **Step 6: Commit**

```bash
git add -u components/http/types
git commit -m "chore(http/types): drop bootstrap placeholder TU

Every Types subdirectory now has real source."
```

---

## Self-Review

Spec coverage (reconciled spec §5/§11):
- §5.1 Headers (allocator-bound, no null state, O(1) iteration) — Task 6.
- §5.2 Body (value-type SBO, no unique_ptr/dynamic_cast, buffered_view, streaming-truth) — Task 7; buffered helpers — Task 12.
- §5.3 Request (string_view target, value Body) — Task 8. Response (stored allocator, set/add_header) — Task 9.
- §5.4 RequestContext (target-as-view split, lazy headers, params, bag, ctx-scoped factories) — Tasks 13–15.
- §5.5 errors + arena-free to_http_response — Tasks 4 + 11.
- §5.6 AsyncOutcome — Task 3.
- §5.7 static cold-path ResponseFactory — Task 10.
- §11 allocation invariant + verification scope (response side, PR1) — Task 16.
- §14.1 unit tests — per task, incl. `allocation_gate_test` and `url_decode` extraction.

Divergences from the original PR1 plan, all per the reconciled spec:
- Body: virtual base + `unique_ptr` → **value-type SBO** (no heap node, no dynamic_cast).
- Request::target: owned `std::string` → **`string_view`** (zero-copy; kills the SSO dangle).
- Response: gains a **stored allocator**; `with_header` → **`set_header`/`add_header`**.
- Response construction: static `ResponseFactory` on the hot path → **`ctx.json(...)`**; static factory retained for the cold/error path only.
- `query<T>`/`path_param<T>`: `.cpp` explicit-instantiation list → **header-defined** (no link cap; `query<size_t>` links).
- Test idiom: `dynamic_cast<StringBody*>` → **`body.buffered_view()`**.
- New: shared `url_decode` (de-duplicated), **allocation gate** test.

Deferred (noted in-plan): `BeastRequestBody` + `ctx.stream` / `StreamingProducerBody` (PR3+); routing `set_path_param` caller (PR2); request-side allocation gate (PR3).

Type consistency: `Headers::owned`/`view_of_beast` and `promote_to_owned(alloc)` consistent across tasks; `Body` factory/`buffered_view`/`read_chunk` signatures match between Task 7 (decl) and Task 12 (defs); `Response{alloc}` ctor + member order verified (alloc before headers); `RequestContext` member-init order (alloc_ before param/bag vectors) verified; bag `set/get/has` header-defined for user types, `~RequestContext` + `find_bag_entry` in the `.cpp`.

Allocation-invariant honesty: the gate (Task 16) verifies the **response side only**; the spec (§11.1) and this plan both state the full wire-path invariant is a PR3 acceptance target. No claim is made that PR1 proves the request side.

---

## Execution Handoff

**Plan reconciled and saved to `docs/superpowers/plans/2026-05-07-http-types-layer.md`.** It now matches the reconciled design spec (`2026-05-07-http-redesign-design.md`, updated 2026-05-31). Two execution options:

1. **Subagent-Driven (recommended)** — one fresh subagent per task, review between tasks. Each task closes a TDD cycle (red → green → commit). The Body SBO type-erasure (Task 7) and the allocation gate (Task 16) deserve an explicit review checkpoint.
2. **Inline Execution** — execute tasks in this session via executing-plans, batch with checkpoints.

After PR 1 lands, the next plan is `2026-MM-DD-http-routing-layer.md` (RouteRegistry, HttpController, GroupBinding, Router, the bake step, conflict detection) — where `set_path_param`'s caller and the typed-error→Response bake step land.
