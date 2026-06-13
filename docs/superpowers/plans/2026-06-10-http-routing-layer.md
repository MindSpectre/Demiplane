# HTTP Redesign — PR 2: Routing Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the routing layer (`RouteRegistry`, `HttpController` + verb DSL, `Middleware` shape, the bake step with
Outcome→Response wiring, `GroupBinding`, `Router`, conflict detection) as per-leaf static/interface libraries aggregated
into `Demiplane.Component.HTTP.Routing`, with full unit-test coverage and a find_route allocation gate. Spec: §8 +
§12.2 PR 2 of `docs/superpowers/specs/2026-05-07-http-redesign-design.md`.

**Architecture:** Pure in-process layer on top of the landed Types layer — no drivers, no sockets, no Server.
Controllers register routes locally in `configure_routes()` via the verb DSL; the bake step
(`detail::ControllerBaker`, driven by `GroupBinding::add_controller`) composes prefix + middleware chain +
Outcome→Response collapse into one `ContextHandler` per route and merges it into the `RouteRegistry`. After `freeze()`
the registry is immutable (thread-safe by construction); `find_route()` is exact-map + segment-walk parametric, zero
global-heap allocations on the hot path (gated). `Router::dispatch()` is the thin facade drivers will call in PR 3.

**Tech Stack:** C++23 (concepts, deducing-this consumers, `std::to_underlying`, coroutines), `gears::Outcome`
(visit/err), Boost.Asio `awaitable`, `boost::unordered_flat_map` (transparent lookup), Boost.Container `small_vector`,
pmr arenas, GoogleTest. Links the landed PR1 leaf targets (`Demiplane.Component.HTTP.Types.*`).

---

## Reconciliation against the landed PR 1 code (read before executing)

The spec was written before PR 1 landed; these are the facts on the ground this plan is built against:

1. **CMake convention is per-leaf targets**, not one static lib per layer. PR 1 landed
   `${DMP_HTTP}.Types.Enums/.Headers/...` leaves, each owning its include dir (headers cross-reference as
   `<headers.hpp>`, never `<headers/headers.hpp>`), aggregated by an INTERFACE target `${DMP_HTTP}.Types`. There is
   **no `Demiplane::Component::Http::Types` alias** — tests link the dotted name `Demiplane.Component.HTTP.Types`.
   This plan follows that convention: leaves `${DMP_HTTP}.Routing.Middleware/.RouteRegistry/.Controller/.Group/.Router`
   + INTERFACE aggregate `${DMP_HTTP}.Routing` (dotted: `Demiplane.Component.HTTP.Routing`).
2. **`RequestContext::set_path_param(name, value)` is singular** (the landed API), not the spec's
   `set_path_params(vector)`. `Router::dispatch` loops over `ResolvedRoute::path_params` calling it.
3. **No old `Server` exists** — commit `d3c40ef` deleted the legacy component before PR 1, so the spec's "Old `Server`
   still wires it" is stale. `GroupBinding` is therefore constructed directly over a `RouteRegistry&` plus a
   controller-sink vector; `Server::in_group()` (PR 5) will construct it over its own members.
4. **Route parameter syntax decision:** a whole segment of the form `{name}` is a capture (`/users/{id}`). The spec
   names parametric routing but never fixes a syntax. Literal segments reject `%`, `{`, `}` at registration (spec §8.6
   split-before-decode + ambiguity guard).
5. **`PathNormalization` lives in the routing layer** for now (constructor parameter of `RouteRegistry`, default
   `collapse_trailing_slash`); `ServerConfig` (PR 6) will map its config enum onto it.
6. **Frame economy:** where the spec's §8.3 sketch wraps every layer in a coroutine, this plan uses plain lambdas that
   return the inner awaitable directly wherever no `co_await` is semantically needed (plain member/callable handlers,
   middleware wrappers). Only the Outcome-collapse wrapper is a coroutine. This keeps §11.1's budget at one frame per
   *user* middleware instead of two.
7. **`NEXUS_REGISTER` macro is broken as defined** (`static constexpr Policy nexus_policy` — constexpr static member
   without initializer) and has zero usages in the repo. Task 8 fixes it to `nexus_policy{}` and uses it on
   `HttpController` per spec §8.2.
8. **`convert_string<bool>` trap** (flagged at PR 1 final review): `query<bool>`/`path_param<bool>` is a hard compile
   error today because `bool` routes to `from_chars`. Task 3 adds a strict bool branch (`"1"/"true"/"0"/"false"`).
9. **`routing/firewall/` data types are deferred** — the old `firewall_config.hpp` was deleted upstream, nothing
   consumes `rate_limit`/`ip_rule` until someone writes rate-limit middleware (spec §2 non-goal 5). YAGNI.
10. **boost-unordered is installed** (transitive vcpkg dep) but not declared; Task 1 adds it to `vcpkg.json` and the
    root `boost-libs` list so `Boost::unordered` is a first-class target.

**Build prerequisites:** presets `release` (build/release) and `asan` (build/asan) both already have
`BUILD_HTTP:BOOL=ON` in their caches (verified). Fresh build dirs need `-DBUILD_HTTP=ON`.

---

## File Structure

```
components/http/routing/
├─ CMakeLists.txt                      ← aggregate INTERFACE ${DMP_HTTP}.Routing (grows per task)
├─ middleware/
│  ├─ CMakeLists.txt                   header-only leaf
│  └─ middleware.hpp                   ContextHandler, NextHandler, Middleware, add_basic_middleware
├─ route_registry/
│  ├─ CMakeLists.txt
│  ├─ route_registry.hpp               PathNormalization, RouteConflictError, ResolvedRoute, join_path, RouteRegistry
│  └─ route_registry.cpp
├─ controller/
│  ├─ CMakeLists.txt
│  ├─ controller.hpp                   HttpController + verb DSL + RouteHandler concept + detail::{traits, collapse_outcome, wrap_with_middleware, ControllerBaker}
│  └─ controller.cpp
├─ group/
│  ├─ CMakeLists.txt                   header-only leaf
│  └─ group.hpp                        GroupBinding
└─ router/
   ├─ CMakeLists.txt
   ├─ router.hpp                       Router
   └─ router.cpp

tests/unit_tests/http/routing/
├─ routing_test_utils.hpp              run_awaitable + RoutingTestBase (synthetic Request/RequestContext)
├─ test_route_registry.cpp             registration/freeze/conflicts/join_path + exact + parametric lookup
├─ test_controller.cpp                 verb DSL, bake mechanics, Outcome collapse, concept static_asserts
├─ test_middleware.cpp                 order, short-circuit, post-mutation, ctx bag, add_basic_middleware
├─ test_group.cpp                      prefixes, nesting, multi-controller, cross-controller conflicts
├─ test_router.cpp                     dispatch: 200/404/405/params/exception propagation
└─ test_routing_allocation_gate.cpp    find_route zero-global-alloc gate

Modified:
├─ vcpkg.json                                            + "boost-unordered"
├─ CMakeLists.txt (root)                                 + `unordered` in boost-libs
├─ components/http/CMakeLists.txt                        + add_subdirectory(routing)
├─ components/http/types/enums/http_enums.hpp            + kHttpMethodCount
├─ components/http/types/request_context/request_context.hpp  + convert_string<bool> branch
├─ components/http/types/url_decode/url_decode.{hpp,cpp} + url_decode_arena overload
├─ common/nexus/core/include/nexus.hpp                   NEXUS_REGISTER macro fix ({} initializer)
├─ tests/unit_tests/http/CMakeLists.txt                  + Http.Routing test target
└─ tests/unit_tests/http/types/test_{http_enums,request_context,url_decode}.cpp  + new tests
```

Build verification per task: `cmake --build build/release --target <target> -- -j4` clean. Test verification:
`ctest --test-dir build/release --output-on-failure -R <pattern>` passes. Work happens on the current branch
`component/http-1.1/v1.2`.

---

## Task 1: Bootstrap routing skeleton + Boost.Unordered + middleware shape

**Files:**
- Modify: `vcpkg.json`
- Modify: `CMakeLists.txt` (root, line ~120 `boost-libs` list)
- Create: `components/http/routing/CMakeLists.txt`
- Create: `components/http/routing/middleware/CMakeLists.txt`
- Create: `components/http/routing/middleware/middleware.hpp`
- Modify: `components/http/CMakeLists.txt`

**Goal:** Routing layer registered in the build; `Middleware`/`ContextHandler` aliases available. No test of its own —
pure aliases (same precedent as PR 1's `async_outcome.hpp`); first compile-coverage comes via Task 5's `.cpp`.

- [ ] **Step 1: Declare boost-unordered.** In `vcpkg.json`, the dependencies array currently starts:

```json
  "dependencies": [
    "boost-program-options",
```

Change to:

```json
  "dependencies": [
    "boost-program-options",
    "boost-unordered",
```

- [ ] **Step 2: Add the Boost component.** In the root `CMakeLists.txt` the list currently reads:

```cmake
list(APPEND boost-libs
        container
        stacktrace_backtrace
        thread
        beast
        asio
        system
)
```

Change to:

```cmake
list(APPEND boost-libs
        container
        unordered
        stacktrace_backtrace
        thread
        beast
        asio
        system
)
```

- [ ] **Step 3: Create the directory tree**

```bash
cd /home/grivin/Workspace/Demiplane
mkdir -p components/http/routing/{middleware,route_registry,controller,group,router} tests/unit_tests/http/routing
```

- [ ] **Step 4: Create `components/http/routing/middleware/middleware.hpp`**

```cpp
#pragma once

#include <functional>
#include <utility>

#include <async_outcome.hpp>
#include <request_context.hpp>

namespace demiplane::http {

    /// The baked, ready-to-call form of one route: prefix applied, middleware
    /// chain composed, Outcome→Response collapse wired (spec §8.3). Stored in
    /// the frozen RouteRegistry; the Router invokes it through a pointer —
    /// never a copy (copying a std::function may heap-allocate, spec §8.5).
    using ContextHandler = std::function<AsyncResponse(RequestContext)>;

    /// What a middleware calls to continue the chain. Passed by const& — NEVER
    /// by value: a by-value copy of the composed std::function would
    /// heap-allocate per layer per request (spec §8.3). The referent lives in
    /// the frozen baked chain and outlives any request, including the
    /// reference held by a middleware's suspended coroutine frame.
    using NextHandler = ContextHandler;

    /// A middleware can short-circuit (co_return without calling next),
    /// enrich the context (ctx.set<T>(...) then call next), or post-process
    /// (auto r = co_await next(std::move(ctx)); ...; co_return r;).
    using Middleware = std::function<AsyncResponse(RequestContext, const NextHandler&)>;

    /// Append several middlewares to a controller in one call (spec §8.4):
    ///     add_basic_middleware(*users, log_mw, request_id_mw, tracer_mw);
    /// They run in argument order (first = outermost).
    template <typename Controller, typename... Mws>
    void add_basic_middleware(Controller& controller, Mws&&... mws) {
        (controller.add_middleware(std::forward<Mws>(mws)), ...);
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/routing/middleware/CMakeLists.txt`**

```cmake
##############################################################################
# Http Routing — middleware shape (header-only)
##############################################################################
add_library(${DMP_HTTP}.Routing.Middleware INTERFACE middleware.hpp)

target_include_directories(${DMP_HTTP}.Routing.Middleware INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Routing.Middleware INTERFACE
        ${DMP_HTTP}.Types.AsyncOutcome
        ${DMP_HTTP}.Types.RequestContext
)
##############################################################################
```

- [ ] **Step 6: Create `components/http/routing/CMakeLists.txt`** (aggregate; later tasks append their
  `add_subdirectory` + link line)

```cmake
##############################################################################
# Http Routing — registry, controllers, bake step, groups, router
#
# Same convention as types/: each thing is its own leaf target owning its
# include dir (headers cross-reference as <route_registry.hpp>, never ../).
# The dotted ${DMP_HTTP}.Routing target below is an INTERFACE aggregate.
##############################################################################
add_subdirectory(middleware)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Routing INTERFACE)

target_link_libraries(${DMP_HTTP}.Routing INTERFACE
        ${DMP_HTTP}.Routing.Middleware
)
##############################################################################
```

- [ ] **Step 7: Register the layer.** In `components/http/CMakeLists.txt`, after the types block:

```cmake
##############################################################################
# Http Types layer (new, PR 1 of redesign)
##############################################################################
add_subdirectory(types)
##############################################################################
```

add:

```cmake
##############################################################################
# Http Routing layer (PR 2 of redesign)
##############################################################################
add_subdirectory(routing)
##############################################################################
```

- [ ] **Step 8: Configure + sanity build**

```bash
cmake --preset release 2>&1 | tail -5
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -5
```

Expected: configure succeeds (vcpkg may briefly re-resolve the manifest), existing Types tests still build. The
INTERFACE-only routing targets are validated at configure time; first object-code coverage is Task 5.

- [ ] **Step 9: Commit**

```bash
git add vcpkg.json CMakeLists.txt components/http/CMakeLists.txt components/http/routing
git commit -m "feat(http/routing): bootstrap routing layer skeleton + middleware shape

ContextHandler/NextHandler/Middleware aliases and the add_basic_middleware
helper as a header-only leaf; INTERFACE aggregate Demiplane.Component.HTTP.Routing
registered in the http component. boost-unordered declared in the manifest
and the root boost-libs list (flat_map lands with RouteRegistry)."
```

---

## Task 2: `kHttpMethodCount` in http_enums.hpp

**Files:**
- Modify: `components/http/types/enums/http_enums.hpp`
- Modify: `tests/unit_tests/http/types/test_http_enums.cpp`

**Goal:** A count constant to size per-method dispatch arrays (spec §8.5 `num_methods`, counting `unknown`).

- [ ] **Step 1: Write the failing test** — append to `tests/unit_tests/http/types/test_http_enums.cpp`:

```cpp
TEST(HttpEnumsTest, MethodCountCoversAllEnumerators) {
    static_assert(kHttpMethodCount == 8);
    // options is the last enumerator; unknown occupies slot 0.
    EXPECT_EQ(static_cast<std::size_t>(HttpMethod::options) + 1, kHttpMethodCount);
}
```

- [ ] **Step 2: Build — expect failure** (`kHttpMethodCount` undeclared):

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
```

- [ ] **Step 3: Implement.** In `components/http/types/enums/http_enums.hpp`: add `#include <cstddef>` to the
  includes (after `#include <cstdint>`), and directly after the `HttpMethod` enum's closing `};` insert:

```cpp
    /// Number of HttpMethod enumerators INCLUDING `unknown` (slot 0). Sizes
    /// per-method dispatch arrays in the routing layer. `unknown` can never be
    /// registered (RouteRegistry throws), so its slot stays empty and an
    /// unrecognized incoming verb falls out as 404/405 (spec §8.5).
    inline constexpr std::size_t kHttpMethodCount = 8;
```

- [ ] **Step 4: Build + run — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Types 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add components/http/types/enums/http_enums.hpp tests/unit_tests/http/types/test_http_enums.cpp
git commit -m "feat(http/types): add kHttpMethodCount for per-method dispatch arrays"
```

---

## Task 3: `convert_string<bool>` — defuse the query<bool> compile trap

**Files:**
- Modify: `components/http/types/request_context/request_context.hpp`
- Modify: `tests/unit_tests/http/types/test_request_context.cpp`

**Goal:** `bool` is arithmetic, so today `query<bool>` routes to `std::from_chars` — which has no bool overload — and
fails to compile deep inside the template (flagged at PR 1 final review). Add a strict bool branch *before* the
arithmetic one: `"1"/"true"` → true, `"0"/"false"` → false, anything else → nullopt.

- [ ] **Step 1: Write the failing test** — append to `tests/unit_tests/http/types/test_request_context.cpp`:

```cpp
TEST_F(RequestContextTest, QueryAndPathParamBool) {
    RequestContext ctx{make_request(HttpMethod::get, "/f?a=true&b=false&c=1&d=0&e=yes&f="), alloc_};
    EXPECT_EQ(ctx.query<bool>("a"), true);
    EXPECT_EQ(ctx.query<bool>("b"), false);
    EXPECT_EQ(ctx.query<bool>("c"), true);
    EXPECT_EQ(ctx.query<bool>("d"), false);
    EXPECT_EQ(ctx.query<bool>("e"), std::nullopt);  // strict: only true/false/1/0
    EXPECT_EQ(ctx.query<bool>("f"), std::nullopt);
    EXPECT_TRUE(ctx.query_or<bool>("missing", true));

    ctx.set_path_param("flag", "true");
    EXPECT_EQ(ctx.path_param<bool>("flag"), true);
}
```

- [ ] **Step 2: Build — expect failure** (a hard compile error today: `from_chars` has no `bool` overload — that IS
  the trap):

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -15
```

- [ ] **Step 3: Implement.** In `request_context.hpp`, `convert_string<T>` currently reads:

```cpp
            } else if constexpr (std::is_arithmetic_v<T>) {
```

Insert a bool branch before it, so the chain becomes:

```cpp
            } else if constexpr (std::is_same_v<T, bool>) {
                // is_arithmetic_v<bool> is true but from_chars has no bool
                // overload — this branch must precede the arithmetic one.
                // Strict by design: "1"/"true" and "0"/"false" only.
                if (value == "1" || value == "true")
                    return true;
                if (value == "0" || value == "false")
                    return false;
                return std::nullopt;
            } else if constexpr (std::is_arithmetic_v<T>) {
```

- [ ] **Step 4: Build + run — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Types 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add components/http/types/request_context/request_context.hpp tests/unit_tests/http/types/test_request_context.cpp
git commit -m "fix(http/types): support bool in query<T>/path_param<T> conversion

is_arithmetic_v<bool> routed bool to from_chars, which has no bool overload
— query<bool> was a hard compile error. Strict parsing: 1/true/0/false."
```

---

## Task 4: `url_decode_arena` — arena-allocating decoder for path captures

**Files:**
- Modify: `components/http/types/url_decode/url_decode.hpp`
- Modify: `components/http/types/url_decode/url_decode.cpp`
- Modify: `tests/unit_tests/http/types/test_url_decode.cpp`

**Goal:** Parametric route captures decode into the *request arena*, never the global heap (spec §8.5/§11). The
common no-escape case returns the input view unchanged — zero copy. Shares the hex/decode core with the existing
heap-returning `url_decode`.

- [ ] **Step 1: Write the failing test** — append to `tests/unit_tests/http/types/test_url_decode.cpp` (add
  `#include <memory_resource>` to its includes):

```cpp
class UrlDecodeArenaTest : public ::testing::Test {
protected:
    std::pmr::monotonic_buffer_resource res_{1024};
    std::pmr::polymorphic_allocator<> alloc_{&res_};
};

TEST_F(UrlDecodeArenaTest, PassthroughIsZeroCopy) {
    const std::string_view in = "plain-segment";
    auto out = url_decode_arena(in, false, alloc_);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, "plain-segment");
    EXPECT_EQ(out->data(), in.data());  // same buffer — no copy was made
}

TEST_F(UrlDecodeArenaTest, DecodesEscapesIntoArena) {
    auto out = url_decode_arena("John%20Doe", false, alloc_);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, "John Doe");
}

TEST_F(UrlDecodeArenaTest, PlusIsLiteralInPathMode) {
    const std::string_view in = "a+b";
    auto out = url_decode_arena(in, false, alloc_);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, "a+b");
    EXPECT_EQ(out->data(), in.data());  // '+' alone forces no rewrite in path mode
}

TEST_F(UrlDecodeArenaTest, PlusIsSpaceInFormMode) {
    auto out = url_decode_arena("a+b", true, alloc_);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, "a b");
}

TEST_F(UrlDecodeArenaTest, MalformedEscapesFail) {
    EXPECT_FALSE(url_decode_arena("a%2", false, alloc_).has_value());
    EXPECT_FALSE(url_decode_arena("a%2G", false, alloc_).has_value());
}
```

- [ ] **Step 2: Build — expect failure** (`url_decode_arena` undeclared):

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
```

- [ ] **Step 3: Extend `components/http/types/url_decode/url_decode.hpp`** — add `#include <memory_resource>` and the
  new declaration after the existing one:

```cpp
    /// Same decoding, allocating from `alloc` (the request arena) instead of
    /// the global heap — the routing hot path (spec §8.5/§11). Returns the
    /// INPUT view unchanged when no rewrite is needed (no '%', and no '+' in
    /// plus_is_space mode) — the common zero-copy case. Otherwise the result
    /// views arena storage: valid until the arena resets, never individually
    /// freed (monotonic). nullopt on a malformed escape.
    std::optional<std::string_view> url_decode_arena(std::string_view in, bool plus_is_space,
                                                     std::pmr::polymorphic_allocator<> alloc);
```

- [ ] **Step 4: Replace `components/http/types/url_decode/url_decode.cpp`** with the refactored shared-core version
  (the existing `url_decode` behavior is pinned by the existing tests — they must still pass):

```cpp
#include "url_decode.hpp"

namespace demiplane::http {

    namespace {
        constexpr int hex_val(const char x) noexcept {
            if (x >= '0' && x <= '9')
                return x - '0';
            if (x >= 'a' && x <= 'f')
                return 10 + x - 'a';
            if (x >= 'A' && x <= 'F')
                return 10 + x - 'A';
            return -1;
        }

        /// Decodes `in` into `out`, which must hold >= in.size() chars (the
        /// decoded form never grows). Returns the decoded length, or nullopt
        /// on a truncated/non-hex escape.
        std::optional<std::size_t> decode_into(const std::string_view in, const bool plus_is_space,
                                               char* out) noexcept {
            std::size_t n = 0;
            for (std::size_t i = 0; i < in.size(); ++i) {
                if (const char c = in[i]; c == '+' && plus_is_space) {
                    out[n++] = ' ';
                } else if (c == '%') {
                    if (i + 2 >= in.size())
                        return std::nullopt;
                    const int hi = hex_val(in[i + 1]);
                    const int lo = hex_val(in[i + 2]);
                    if (hi < 0 || lo < 0)
                        return std::nullopt;
                    out[n++] = static_cast<char>(hi << 4 | lo);
                    i += 2;
                } else {
                    out[n++] = c;
                }
            }
            return n;
        }
    }  // namespace

    std::optional<std::string> url_decode(const std::string_view in, const bool plus_is_space) {
        std::string out;
        out.resize(in.size());
        const auto n = decode_into(in, plus_is_space, out.data());
        if (!n)
            return std::nullopt;
        out.resize(*n);
        return out;
    }

    std::optional<std::string_view> url_decode_arena(const std::string_view in, const bool plus_is_space,
                                                     std::pmr::polymorphic_allocator<> alloc) {
        const bool needs_rewrite = in.find('%') != std::string_view::npos
                                   || (plus_is_space && in.find('+') != std::string_view::npos);
        if (!needs_rewrite)
            return in;  // zero-copy
        char* buf    = static_cast<char*>(alloc.allocate_bytes(in.size(), 1));
        const auto n = decode_into(in, plus_is_space, buf);
        if (!n)
            return std::nullopt;
        return std::string_view{buf, *n};
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Build + run — expect pass (old AND new url_decode tests)**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Types 2>&1 | tail -5
```

- [ ] **Step 6: Commit**

```bash
git add components/http/types/url_decode tests/unit_tests/http/types/test_url_decode.cpp
git commit -m "feat(http/types): add url_decode_arena (zero-copy passthrough, arena rewrite)

Parametric route captures (PR2) decode into the request arena, never the
global heap. No-escape inputs return the input view unchanged. Shared
decode core with the heap-returning url_decode."
```

---

## Task 5: RouteRegistry — registration, validation, conflicts, freeze, join_path

**Files:**
- Create: `components/http/routing/route_registry/route_registry.hpp`
- Create: `components/http/routing/route_registry/route_registry.cpp`
- Create: `components/http/routing/route_registry/CMakeLists.txt`
- Create: `tests/unit_tests/http/routing/routing_test_utils.hpp`
- Create: `tests/unit_tests/http/routing/test_route_registry.cpp`
- Modify: `components/http/routing/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** The registry's build phase: `add_route` with full validation, conflict *recording* (never throwing on
duplicates — spec §8.7 wants them aggregated), `freeze()`, and the `join_path` prefix helper. The complete header
lands here (including `find_route`'s declaration); `find_route`/lookup internals are implemented in Tasks 6–7.

- [ ] **Step 1: Create `tests/unit_tests/http/routing/routing_test_utils.hpp`** (shared by every routing test file)

```cpp
#pragma once

#include <deque>
#include <memory_resource>
#include <string>
#include <utility>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>

#include <body.hpp>
#include <headers.hpp>
#include <request.hpp>
#include <request_context.hpp>

namespace http_routing_test {

    template <typename T>
    T run_awaitable(boost::asio::awaitable<T> aw) {
        boost::asio::io_context ioc;
        auto fut = boost::asio::co_spawn(ioc, std::move(aw), boost::asio::use_future);
        ioc.run();
        return fut.get();  // rethrows handler exceptions
    }

    class RoutingTestBase : public ::testing::Test {
    protected:
        std::pmr::monotonic_buffer_resource resource_{8192};
        std::pmr::polymorphic_allocator<> alloc_{&resource_};
        std::deque<std::string> target_storage_;  // stable backing for string_view targets

        demiplane::http::Request make_request(const demiplane::http::HttpMethod m, std::string target) {
            using namespace demiplane::http;
            Request req{Headers::owned(alloc_)};
            req.method = m;
            target_storage_.push_back(std::move(target));
            req.target = target_storage_.back();
            return req;
        }

        demiplane::http::RequestContext make_ctx(const demiplane::http::HttpMethod m, std::string target) {
            return demiplane::http::RequestContext{make_request(m, std::move(target)), alloc_};
        }
    };

}  // namespace http_routing_test
```

- [ ] **Step 2: Write the failing tests** — `tests/unit_tests/http/routing/test_route_registry.cpp`:

```cpp
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include <route_registry.hpp>

#include "routing_test_utils.hpp"

using namespace demiplane::http;
using http_routing_test::run_awaitable;

namespace {
    ContextHandler tag_handler(std::string tag) {
        return [tag = std::move(tag)](RequestContext ctx) -> AsyncResponse {
            co_return ctx.ok(tag);
        };
    }
}

// ── Registration / freeze / conflicts ──────────────────────────────────────

TEST(RouteRegistryTest, FreezeWithoutConflicts) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users", tag_handler("u-get"));
    reg.add_route(HttpMethod::post, "/users", tag_handler("u-post"));
    reg.add_route(HttpMethod::get, "/health", tag_handler("h"));
    EXPECT_FALSE(reg.is_frozen());
    EXPECT_TRUE(reg.freeze().empty());
    EXPECT_TRUE(reg.is_frozen());
}

TEST(RouteRegistryTest, DuplicateIsRecordedNotThrown) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users", tag_handler("a"));
    reg.add_route(HttpMethod::get, "/users", tag_handler("b"));  // duplicate — no throw
    const auto conflicts = reg.freeze();
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0].method, HttpMethod::get);
    EXPECT_EQ(conflicts[0].path, "/users");
}

TEST(RouteRegistryTest, DuplicateDetectedAcrossNormalization) {
    RouteRegistry reg;  // default: collapse_trailing_slash
    reg.add_route(HttpMethod::get, "/users", tag_handler("a"));
    reg.add_route(HttpMethod::get, "/users/", tag_handler("b"));  // same normalized path
    EXPECT_EQ(reg.freeze().size(), 1u);
}

TEST(RouteRegistryTest, RegistrationAfterFreezeThrows) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/a", tag_handler("a"));
    (void)reg.freeze();
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/b", tag_handler("b")), std::logic_error);
}

TEST(RouteRegistryTest, InvalidRegistrationsThrow) {
    RouteRegistry reg;
    EXPECT_THROW(reg.add_route(HttpMethod::unknown, "/a", tag_handler("a")), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/a", ContextHandler{}), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "no-slash", tag_handler("a")), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "", tag_handler("a")), std::invalid_argument);
}

TEST(RouteRegistryTest, LiteralSegmentRejectsPercentAndBraces) {
    RouteRegistry reg;
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/a%20b", tag_handler("a")), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/a{b/c", tag_handler("a")), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/a}b", tag_handler("a")), std::invalid_argument);
}

TEST(RouteRegistryTest, ParamNameValidation) {
    RouteRegistry reg;
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/{}", tag_handler("a")), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/{id}/x/{id}", tag_handler("a")), std::invalid_argument);
    // valid parametric registration is fine
    reg.add_route(HttpMethod::get, "/{id}/x/{other}", tag_handler("a"));
    EXPECT_TRUE(reg.freeze().empty());
}

TEST(RouteRegistryTest, SameShapeDifferentParamNamesIsConflict) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/u/{a}", tag_handler("a"));
    reg.add_route(HttpMethod::post, "/u/{b}", tag_handler("b"));  // shape-equal, names differ
    const auto conflicts = reg.freeze();
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0].method, HttpMethod::post);
}

TEST(RouteRegistryTest, SameTemplateTwoMethodsIsNotAConflict) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/u/{id}", tag_handler("g"));
    reg.add_route(HttpMethod::post, "/u/{id}", tag_handler("p"));
    EXPECT_TRUE(reg.freeze().empty());
}

// ── join_path ───────────────────────────────────────────────────────────────

TEST(JoinPathTest, Battery) {
    EXPECT_EQ(join_path("", "/users"), "/users");
    EXPECT_EQ(join_path("/api/v1", "/users"), "/api/v1/users");
    EXPECT_EQ(join_path("/api/", "/users"), "/api/users");
    EXPECT_EQ(join_path("/api", "/"), "/api");
    EXPECT_EQ(join_path("", "/"), "/");
    EXPECT_EQ(join_path("/api", "users"), "/api/users");
    EXPECT_EQ(join_path("/", "/x"), "/x");
}
```

- [ ] **Step 3: Register the test target** — append to `tests/unit_tests/http/CMakeLists.txt`:

```cmake
##############################################################################
# Test HTTP Routing layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Routing
        routing/test_route_registry.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Routing
        PRIVATE
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Types
        ${TEST_LIBS}
)
##############################################################################
```

(The source list grows in Tasks 8–13; check before appending.)

- [ ] **Step 4: Configure + build — expect failure** (`route_registry.hpp` missing):

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -10
```

- [ ] **Step 5: Create `components/http/routing/route_registry/route_registry.hpp`** (complete header — lookup
  internals implemented across Tasks 5–7)

```cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory_resource>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/container/small_vector.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <demiplane/gears>

#include <errors.hpp>
#include <http_enums.hpp>
#include <middleware.hpp>

namespace demiplane::http {

    /// Path normalization policy (spec §8.6), applied at registration AND at
    /// lookup so both sides agree on the canonical form. Owned by the registry
    /// in PR 2; ServerConfig (PR 6) maps its config enum onto it.
    enum class PathNormalization : std::uint8_t {
        none,                     ///< exact byte match
        collapse_trailing_slash,  ///< "/users/" == "/users"   (default)
        collapse_multi_slash,     ///< + "/users//42" == "/users/42"
    };

    /// Build-phase registration conflict. Aggregated by freeze(); the Server
    /// (PR 5) turns a non-empty set into a RouteConflictAggregateError throw.
    struct RouteConflictError {
        HttpMethod method;
        std::string path;
        std::string detail;
    };

    /// One matched route: a pointer to the frozen registry's stored handler —
    /// NEVER a copy (copying a std::function may heap-allocate, spec §8.5) —
    /// plus the captured path parameters. Param NAME views point into the
    /// registry's route templates (stable while the registry lives); VALUE
    /// views point into the incoming path or the request arena (valid until
    /// the arena resets).
    struct ResolvedRoute {
        using ParamVec = boost::container::small_vector<
            std::pair<std::string_view, std::string_view>, 4,
            std::pmr::polymorphic_allocator<std::pair<std::string_view, std::string_view>>>;

        const ContextHandler* handler;
        ParamVec path_params;
    };

    /// Join a group prefix and a route path with slash hygiene:
    /// ("", "/users") -> "/users"; ("/api/", "/users") -> "/api/users";
    /// ("/api", "/") -> "/api". Build-phase helper (heap OK).
    [[nodiscard]] std::string join_path(std::string_view prefix, std::string_view path);

    /**
     * @brief Method+path route table: exact + parametric, baked at startup,
     *        immutable — and therefore safely shared across io threads —
     *        after freeze() (spec §8.1/§8.5).
     *
     * Parametric syntax: a whole segment of the form "{name}" captures the
     * incoming segment ("/users/{id}/posts/{post_id}"). Captures percent-
     * decode with plus_is_space=false into the request arena; literal
     * segments compare raw bytes, so '%' (and '{'/'}') are rejected inside
     * literal segments at registration (spec §8.6 split-before-decode).
     *
     * Lifecycle: add_route() during build → freeze() → find_route() only.
     */
    class RouteRegistry : gears::NonCopyable {
    public:
        explicit RouteRegistry(
            const PathNormalization norm = PathNormalization::collapse_trailing_slash) noexcept
            : norm_{norm} {
        }

        /// Throws std::logic_error after freeze(); std::invalid_argument on
        /// HttpMethod::unknown, a null handler, a path not starting with '/',
        /// '%'/'{'/'}' inside a literal segment, or an empty/duplicate
        /// parameter name. A duplicate (method, normalized path) does NOT
        /// throw — it is recorded and reported by freeze() so the caller sees
        /// every conflict at once (spec §8.7).
        void add_route(HttpMethod method, std::string_view path, ContextHandler handler);

        /// Freezes the registry; returns all recorded conflicts (empty = OK).
        [[nodiscard]] std::vector<RouteConflictError> freeze();

        [[nodiscard]] bool is_frozen() const noexcept {
            return frozen_;
        }

        /// Hot path: zero global-heap allocations on a match (gated). Exact
        /// match wins over parametric. 404 vs 405 per spec §8.5: a known path
        /// with no handler for `method` yields MethodNotAllowedError carrying
        /// the populated verb set.
        [[nodiscard]] gears::Outcome<ResolvedRoute, NotFoundError, MethodNotAllowedError>
        find_route(HttpMethod method, std::string_view path,
                   std::pmr::polymorphic_allocator<> arena_alloc) const;

    private:
        struct PathSegment {
            std::string text;  ///< literal text, or the parameter name
            bool is_param = false;
        };
        using MethodSlots = std::array<ContextHandler, kHttpMethodCount>;

        struct ParamTemplate {
            std::vector<PathSegment> segments;
            MethodSlots by_method;
        };

        /// Heterogeneous lookup: find(string_view) without a temp std::string.
        struct TransparentStringHash {
            using is_transparent = void;
            [[nodiscard]] std::size_t operator()(const std::string_view s) const noexcept {
                return std::hash<std::string_view>{}(s);
            }
        };

        [[nodiscard]] std::string normalize_owned(std::string_view path) const;
        [[nodiscard]] std::string_view normalize_lookup(std::string_view path,
                                                        std::pmr::polymorphic_allocator<> alloc) const;
        [[nodiscard]] static std::vector<PathSegment> parse_segments(std::string_view normalized);
        [[nodiscard]] static bool match_template(const ParamTemplate& tmpl, std::string_view path,
                                                 std::pmr::polymorphic_allocator<> alloc,
                                                 ResolvedRoute::ParamVec& out_params);
        [[nodiscard]] static std::vector<HttpMethod> allowed_methods(const MethodSlots& slots);

        bool frozen_ = false;
        PathNormalization norm_;
        boost::unordered::unordered_flat_map<std::string, MethodSlots, TransparentStringHash,
                                             std::equal_to<>>
            exact_;
        std::vector<ParamTemplate> parametric_;
        std::vector<RouteConflictError> conflicts_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 6: Create `components/http/routing/route_registry/route_registry.cpp`** — registration side + helpers.
  `find_route`, `normalize_lookup`, `match_template`, and `allowed_methods` get their real bodies in Tasks 6–7; this
  task defines them as compiling stubs so the TU is complete (they are not yet called by any test):

```cpp
#include "route_registry.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

#include <url_decode.hpp>

namespace demiplane::http {

    namespace {
        /// Calls fn(segment) for each '/'-separated segment of `path` (which
        /// must start with '/'). "/" yields one empty segment — harmless and,
        /// critically, CONSISTENT between registration and lookup. Stops and
        /// returns false the first time fn returns false.
        template <typename Fn>
        bool for_each_segment(const std::string_view path, Fn&& fn) {
            std::size_t pos = 1;  // skip the leading '/'
            while (pos <= path.size()) {
                const std::size_t next = path.find('/', pos);
                const std::string_view seg =
                    path.substr(pos, next == std::string_view::npos ? std::string_view::npos
                                                                    : next - pos);
                if (!fn(seg))
                    return false;
                if (next == std::string_view::npos)
                    break;
                pos = next + 1;
            }
            return true;
        }
    }  // namespace

    std::string join_path(const std::string_view prefix, const std::string_view path) {
        std::string out{prefix};
        while (!out.empty() && out.back() == '/')
            out.pop_back();
        if (path.empty() || path == "/")
            return out.empty() ? std::string{"/"} : out;
        if (path.front() != '/')
            out.push_back('/');
        out += path;
        return out;
    }

    std::string RouteRegistry::normalize_owned(const std::string_view path) const {
        std::string out;
        out.reserve(path.size());
        if (norm_ == PathNormalization::collapse_multi_slash) {
            bool prev_slash = false;
            for (const char c : path) {
                if (c == '/' && prev_slash)
                    continue;
                prev_slash = c == '/';
                out.push_back(c);
            }
        } else {
            out.assign(path);
        }
        if (norm_ != PathNormalization::none) {
            while (out.size() > 1 && out.back() == '/')
                out.pop_back();
        }
        return out;
    }

    std::vector<RouteRegistry::PathSegment> RouteRegistry::parse_segments(
        const std::string_view normalized) {
        std::vector<PathSegment> out;
        std::vector<std::string_view> seen_names;
        for_each_segment(normalized, [&](const std::string_view seg) {
            if (seg.size() >= 2 && seg.front() == '{' && seg.back() == '}') {
                const std::string_view name = seg.substr(1, seg.size() - 2);
                if (name.empty())
                    throw std::invalid_argument{"RouteRegistry: empty parameter name in route '"
                                                + std::string{normalized} + "'"};
                if (name.find_first_of("{}") != std::string_view::npos)
                    throw std::invalid_argument{"RouteRegistry: nested braces in route '"
                                                + std::string{normalized} + "'"};
                if (std::ranges::find(seen_names, name) != seen_names.end())
                    throw std::invalid_argument{"RouteRegistry: duplicate parameter name '"
                                                + std::string{name} + "' in route '"
                                                + std::string{normalized} + "'"};
                seen_names.push_back(name);
                out.push_back(PathSegment{std::string{name}, true});
            } else {
                if (seg.find_first_of("%{}") != std::string_view::npos)
                    throw std::invalid_argument{
                        "RouteRegistry: '%', '{' and '}' are not allowed in literal route "
                        "segments ('"
                        + std::string{normalized} + "', spec §8.6)"};
                out.push_back(PathSegment{std::string{seg}, false});
            }
            return true;
        });
        return out;
    }

    void RouteRegistry::add_route(const HttpMethod method, const std::string_view path,
                                  ContextHandler handler) {
        if (frozen_)
            throw std::logic_error{"RouteRegistry: registration after freeze()"};
        if (method == HttpMethod::unknown)
            throw std::invalid_argument{"RouteRegistry: cannot register HttpMethod::unknown"};
        if (!handler)
            throw std::invalid_argument{"RouteRegistry: null handler"};
        if (path.empty() || path.front() != '/')
            throw std::invalid_argument{"RouteRegistry: route path must start with '/': '"
                                        + std::string{path} + "'"};

        std::string normalized              = normalize_owned(path);
        std::vector<PathSegment> segments   = parse_segments(normalized);
        const bool parametric =
            std::ranges::any_of(segments, [](const PathSegment& s) { return s.is_param; });
        const auto idx = std::to_underlying(method);

        if (!parametric) {
            auto [it, inserted] = exact_.try_emplace(std::move(normalized));
            if (it->second[idx]) {
                conflicts_.push_back(RouteConflictError{method, it->first, "duplicate route"});
                return;
            }
            it->second[idx] = std::move(handler);
            return;
        }

        const auto same_shape = [](const std::vector<PathSegment>& a,
                                   const std::vector<PathSegment>& b) {
            return std::ranges::equal(a, b, [](const PathSegment& x, const PathSegment& y) {
                return x.is_param == y.is_param && (x.is_param || x.text == y.text);
            });
        };
        for (ParamTemplate& tmpl : parametric_) {
            if (!same_shape(tmpl.segments, segments))
                continue;
            const bool names_match =
                std::ranges::equal(tmpl.segments, segments,
                                   [](const PathSegment& x, const PathSegment& y) {
                                       return x.text == y.text;
                                   });
            if (!names_match) {
                // One template per shape (lookup decides 405 on first shape
                // match); divergent names across methods would make captures
                // ambiguous — surfaced as a conflict, not silently merged.
                conflicts_.push_back(RouteConflictError{
                    method, std::move(normalized),
                    "parametric route already registered with different parameter names"});
                return;
            }
            if (tmpl.by_method[idx]) {
                conflicts_.push_back(
                    RouteConflictError{method, std::move(normalized), "duplicate route"});
                return;
            }
            tmpl.by_method[idx] = std::move(handler);
            return;
        }
        ParamTemplate tmpl;
        tmpl.segments       = std::move(segments);
        tmpl.by_method[idx] = std::move(handler);
        parametric_.push_back(std::move(tmpl));
    }

    std::vector<RouteConflictError> RouteRegistry::freeze() {
        frozen_ = true;
        return std::exchange(conflicts_, {});
    }

    // ── Lookup side — real bodies land in Tasks 6–7 ─────────────────────────

    std::string_view RouteRegistry::normalize_lookup(
        const std::string_view path, std::pmr::polymorphic_allocator<> /*alloc*/) const {
        return path;  // Task 6
    }

    bool RouteRegistry::match_template(const ParamTemplate& /*tmpl*/,
                                       const std::string_view /*path*/,
                                       std::pmr::polymorphic_allocator<> /*alloc*/,
                                       ResolvedRoute::ParamVec& /*out_params*/) {
        return false;  // Task 7
    }

    std::vector<HttpMethod> RouteRegistry::allowed_methods(const MethodSlots& /*slots*/) {
        return {};  // Task 6
    }

    gears::Outcome<ResolvedRoute, NotFoundError, MethodNotAllowedError> RouteRegistry::find_route(
        const HttpMethod /*method*/, const std::string_view path,
        std::pmr::polymorphic_allocator<> /*arena_alloc*/) const {
        return gears::err(NotFoundError{"route", std::string{path}});  // Task 6
    }

}  // namespace demiplane::http
```

- [ ] **Step 7: Create `components/http/routing/route_registry/CMakeLists.txt`**

```cmake
##############################################################################
# Http Routing — RouteRegistry (exact + parametric route table)
##############################################################################
add_library(${DMP_HTTP}.Routing.RouteRegistry STATIC route_registry.cpp)

target_include_directories(${DMP_HTTP}.Routing.RouteRegistry PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Routing.RouteRegistry
        PUBLIC
        ${DMP_HTTP}.Routing.Middleware
        ${DMP_HTTP}.Types.Enums
        ${DMP_HTTP}.Types.Errors
        Demiplane::Common::Gears
        Boost::container
        Boost::unordered
        PRIVATE
        ${DMP_HTTP}.Types.UrlDecode
)
##############################################################################
```

- [ ] **Step 8: Wire into the aggregate** — in `components/http/routing/CMakeLists.txt` add
  `add_subdirectory(route_registry)` after `add_subdirectory(middleware)`, and `${DMP_HTTP}.Routing.RouteRegistry` to
  the aggregate's `target_link_libraries`.

- [ ] **Step 9: Configure + build + run — expect pass**

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Routing 2>&1 | tail -10
```

- [ ] **Step 10: Commit**

```bash
git add components/http/routing tests/unit_tests/http/routing tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/routing): RouteRegistry registration, validation, conflict aggregation

add_route validates (unknown verb, null handler, '%'/braces in literals,
param-name hygiene) and RECORDS duplicates instead of throwing — freeze()
returns the aggregated conflict set (spec §8.7). {name} segments are the
parametric syntax; one template per shape, divergent param names across
methods conflict. join_path handles group-prefix slash hygiene. Lookup
side stubbed until the next tasks."
```

---

## Task 6: find_route — exact matches, 404 vs 405, normalization modes

**Files:**
- Modify: `components/http/routing/route_registry/route_registry.cpp`
- Modify: `tests/unit_tests/http/routing/test_route_registry.cpp`

**Goal:** Exact lookup against the transparent flat map, the 404-vs-405 split with the populated `Allow` set, and all
three normalization policies on the lookup side. Trailing-slash collapse is a pure view shrink (zero alloc);
multi-slash collapse rewrites into the request arena (never the global heap).

- [ ] **Step 1: Write the failing tests** — append to `test_route_registry.cpp` (add `#include <algorithm>` and
  `#include <vector>` to the file's includes — `std::ranges::find` and the allowed-set comparisons need them):

```cpp
// ── find_route: exact + 404/405 + normalization ────────────────────────────

class RouteRegistryLookupTest : public http_routing_test::RoutingTestBase {};

TEST_F(RouteRegistryLookupTest, ExactHitInvokesStoredHandler) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users", tag_handler("users-get"));
    ASSERT_TRUE(reg.freeze().empty());

    auto resolved = reg.find_route(HttpMethod::get, "/users", alloc_);
    ASSERT_TRUE(resolved.is_success());
    EXPECT_TRUE(resolved.value().path_params.empty());

    Response r = run_awaitable(
        (*resolved.value().handler)(make_ctx(HttpMethod::get, "/users")));
    EXPECT_EQ(r.status, HttpStatus::ok);
    EXPECT_EQ(*r.body.buffered_view(), "users-get");
}

TEST_F(RouteRegistryLookupTest, UnknownPathIsNotFound) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users", tag_handler("u"));
    (void)reg.freeze();

    auto resolved = reg.find_route(HttpMethod::get, "/missing", alloc_);
    ASSERT_TRUE(resolved.is_error());
    EXPECT_TRUE(resolved.holds_error<NotFoundError>());
}

TEST_F(RouteRegistryLookupTest, KnownPathWrongVerbIs405WithAllowedSet) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users", tag_handler("g"));
    reg.add_route(HttpMethod::post, "/users", tag_handler("p"));
    (void)reg.freeze();

    auto resolved = reg.find_route(HttpMethod::del, "/users", alloc_);
    ASSERT_TRUE(resolved.holds_error<MethodNotAllowedError>());
    const auto& allowed = resolved.error<MethodNotAllowedError>().allowed;
    EXPECT_EQ(allowed.size(), 2u);
    EXPECT_NE(std::ranges::find(allowed, HttpMethod::get), allowed.end());
    EXPECT_NE(std::ranges::find(allowed, HttpMethod::post), allowed.end());
}

TEST_F(RouteRegistryLookupTest, UnknownIncomingVerbOnKnownPathIs405) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users", tag_handler("g"));
    (void)reg.freeze();
    // Beast's verb::unknown maps to HttpMethod::unknown — slot 0 is never
    // registered, so this falls out as 405 with the path's Allow set.
    auto resolved = reg.find_route(HttpMethod::unknown, "/users", alloc_);
    ASSERT_TRUE(resolved.holds_error<MethodNotAllowedError>());
}

TEST_F(RouteRegistryLookupTest, TrailingSlashCollapsedByDefault) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users", tag_handler("u"));
    reg.add_route(HttpMethod::get, "/", tag_handler("root"));
    (void)reg.freeze();

    EXPECT_TRUE(reg.find_route(HttpMethod::get, "/users/", alloc_).is_success());
    EXPECT_TRUE(reg.find_route(HttpMethod::get, "/", alloc_).is_success());  // "/" stays "/"
    // multi-slash NOT collapsed under the default policy:
    EXPECT_TRUE(reg.find_route(HttpMethod::get, "/users//", alloc_).is_error());
}

TEST_F(RouteRegistryLookupTest, MultiSlashPolicyCollapsesRuns) {
    RouteRegistry reg{PathNormalization::collapse_multi_slash};
    reg.add_route(HttpMethod::get, "/users/list", tag_handler("u"));
    (void)reg.freeze();

    EXPECT_TRUE(reg.find_route(HttpMethod::get, "/users//list/", alloc_).is_success());
    EXPECT_TRUE(reg.find_route(HttpMethod::get, "///users///list", alloc_).is_success());
}

TEST_F(RouteRegistryLookupTest, NonePolicyMatchesExactBytes) {
    RouteRegistry reg{PathNormalization::none};
    reg.add_route(HttpMethod::get, "/users", tag_handler("a"));
    reg.add_route(HttpMethod::get, "/users/", tag_handler("b"));  // distinct route, no conflict
    ASSERT_TRUE(reg.freeze().empty());

    auto a = reg.find_route(HttpMethod::get, "/users", alloc_);
    auto b = reg.find_route(HttpMethod::get, "/users/", alloc_);
    ASSERT_TRUE(a.is_success());
    ASSERT_TRUE(b.is_success());
    EXPECT_NE(a.value().handler, b.value().handler);
}
```

- [ ] **Step 2: Build + run — expect the new tests to FAIL** (stubbed lookup returns NotFound for everything):

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Routing 2>&1 | tail -15
```

- [ ] **Step 3: Implement.** In `route_registry.cpp`, replace the three Task-5 stubs `normalize_lookup`,
  `allowed_methods`, and `find_route` with:

```cpp
    std::string_view RouteRegistry::normalize_lookup(
        std::string_view path, std::pmr::polymorphic_allocator<> alloc) const {
        if (norm_ == PathNormalization::none)
            return path;
        if (norm_ == PathNormalization::collapse_multi_slash
            && path.find("//") != std::string_view::npos) {
            // Rare path: rewrite into the request arena — never the global heap.
            char* buf       = static_cast<char*>(alloc.allocate_bytes(path.size(), 1));
            std::size_t n   = 0;
            bool prev_slash = false;
            for (const char c : path) {
                if (c == '/' && prev_slash)
                    continue;
                prev_slash = c == '/';
                buf[n++]   = c;
            }
            path = std::string_view{buf, n};
        }
        while (path.size() > 1 && path.back() == '/')
            path.remove_suffix(1);  // view shrink — zero alloc
        return path;
    }

    std::vector<HttpMethod> RouteRegistry::allowed_methods(const MethodSlots& slots) {
        std::vector<HttpMethod> out;  // cold path (405) — heap is fine here
        for (std::size_t i = 1; i < kHttpMethodCount; ++i)  // slot 0 = unknown, never filled
            if (slots[i])
                out.push_back(static_cast<HttpMethod>(i));
        return out;
    }

    gears::Outcome<ResolvedRoute, NotFoundError, MethodNotAllowedError> RouteRegistry::find_route(
        const HttpMethod method, const std::string_view path,
        const std::pmr::polymorphic_allocator<> arena_alloc) const {
        assert(frozen_ && "RouteRegistry::find_route on an unfrozen registry");
        const std::string_view normalized = normalize_lookup(path, arena_alloc);
        const auto idx                    = std::to_underlying(method);

        if (const auto it = exact_.find(normalized); it != exact_.end()) {
            if (const ContextHandler& h = it->second[idx]) {
                return ResolvedRoute{&h, ResolvedRoute::ParamVec{arena_alloc}};
            }
            return gears::err(MethodNotAllowedError{allowed_methods(it->second)});
        }

        for (const ParamTemplate& tmpl : parametric_) {
            ResolvedRoute::ParamVec params{arena_alloc};
            if (!match_template(tmpl, normalized, arena_alloc, params))
                continue;
            if (const ContextHandler& h = tmpl.by_method[idx]) {
                return ResolvedRoute{&h, std::move(params)};
            }
            // First shape match decides 405 (spec §8.5) — no fall-through.
            return gears::err(MethodNotAllowedError{allowed_methods(tmpl.by_method)});
        }
        return gears::err(NotFoundError{"route", std::string{normalized}});
    }
```

(`match_template` keeps its `return false;` stub until Task 7 — exact tests don't reach it.)

- [ ] **Step 4: Build + run — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Routing 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add components/http/routing/route_registry tests/unit_tests/http/routing/test_route_registry.cpp
git commit -m "feat(http/routing): exact find_route with 404/405 split and normalization

Transparent (heterogeneous) flat-map lookup — no per-request std::string.
405 carries the populated Allow set; HttpMethod::unknown indexes the
never-filled slot 0 and falls out as 405/404 naturally. Trailing-slash
collapse is a view shrink; multi-slash collapse rewrites into the arena."
```

---

## Task 7: find_route — parametric matching with arena-decoded captures

**Files:**
- Modify: `components/http/routing/route_registry/route_registry.cpp`
- Modify: `tests/unit_tests/http/routing/test_route_registry.cpp`

**Goal:** Segment-walk matching (spec §8.5 — no `std::regex`): literals compare raw bytes, `{name}` captures
percent-decode via `url_decode_arena` (`plus_is_space=false`, spec §8.6). A malformed escape in a captured segment
means "this template does not match" (→ 404 if nothing else matches). Exact beats parametric.

- [ ] **Step 1: Write the failing tests** — append to `test_route_registry.cpp`:

```cpp
// ── find_route: parametric ──────────────────────────────────────────────────

TEST_F(RouteRegistryLookupTest, SingleParamCapture) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users/{id}", tag_handler("u"));
    (void)reg.freeze();

    auto resolved = reg.find_route(HttpMethod::get, "/users/42", alloc_);
    ASSERT_TRUE(resolved.is_success());
    const auto& params = resolved.value().path_params;
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0].first, "id");
    EXPECT_EQ(params[0].second, "42");
}

TEST_F(RouteRegistryLookupTest, MultiParamCapture) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users/{id}/posts/{post_id}", tag_handler("p"));
    (void)reg.freeze();

    auto resolved = reg.find_route(HttpMethod::get, "/users/7/posts/99", alloc_);
    ASSERT_TRUE(resolved.is_success());
    const auto& params = resolved.value().path_params;
    ASSERT_EQ(params.size(), 2u);
    EXPECT_EQ(params[0].first, "id");
    EXPECT_EQ(params[0].second, "7");
    EXPECT_EQ(params[1].first, "post_id");
    EXPECT_EQ(params[1].second, "99");
}

TEST_F(RouteRegistryLookupTest, CapturesArePercentDecodedPlusStaysLiteral) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/files/{name}", tag_handler("f"));
    (void)reg.freeze();

    auto decoded = reg.find_route(HttpMethod::get, "/files/report%202026", alloc_);
    ASSERT_TRUE(decoded.is_success());
    EXPECT_EQ(decoded.value().path_params[0].second, "report 2026");

    auto plus = reg.find_route(HttpMethod::get, "/files/a+b", alloc_);
    ASSERT_TRUE(plus.is_success());
    EXPECT_EQ(plus.value().path_params[0].second, "a+b");  // '+' literal in paths (§8.6)
}

TEST_F(RouteRegistryLookupTest, MalformedEscapeInCaptureIsNotFound) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/files/{name}", tag_handler("f"));
    (void)reg.freeze();
    auto resolved = reg.find_route(HttpMethod::get, "/files/bad%2", alloc_);
    ASSERT_TRUE(resolved.is_error());
    EXPECT_TRUE(resolved.holds_error<NotFoundError>());
}

TEST_F(RouteRegistryLookupTest, ExactBeatsParametric) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users/me", tag_handler("exact"));
    reg.add_route(HttpMethod::get, "/users/{id}", tag_handler("param"));
    (void)reg.freeze();

    auto resolved = reg.find_route(HttpMethod::get, "/users/me", alloc_);
    ASSERT_TRUE(resolved.is_success());
    EXPECT_TRUE(resolved.value().path_params.empty());
    Response r = run_awaitable(
        (*resolved.value().handler)(make_ctx(HttpMethod::get, "/users/me")));
    EXPECT_EQ(*r.body.buffered_view(), "exact");
}

TEST_F(RouteRegistryLookupTest, ParametricWrongVerbIs405) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users/{id}", tag_handler("g"));
    (void)reg.freeze();
    auto resolved = reg.find_route(HttpMethod::post, "/users/42", alloc_);
    ASSERT_TRUE(resolved.holds_error<MethodNotAllowedError>());
    EXPECT_EQ(resolved.error<MethodNotAllowedError>().allowed,
              std::vector{HttpMethod::get});
}

TEST_F(RouteRegistryLookupTest, ParamNeverCapturesEmptySegment) {
    RouteRegistry reg{PathNormalization::none};
    reg.add_route(HttpMethod::get, "/users/{id}", tag_handler("u"));
    (void)reg.freeze();
    // Under `none`, "/users/" keeps its trailing empty segment — a param must
    // not capture "".
    EXPECT_TRUE(reg.find_route(HttpMethod::get, "/users/", alloc_).is_error());
}

TEST_F(RouteRegistryLookupTest, SegmentCountMustMatch) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users/{id}", tag_handler("u"));
    (void)reg.freeze();
    EXPECT_TRUE(reg.find_route(HttpMethod::get, "/users", alloc_).is_error());
    EXPECT_TRUE(reg.find_route(HttpMethod::get, "/users/1/extra", alloc_).is_error());
}
```

- [ ] **Step 2: Build + run — expect the new tests to FAIL** (match_template stub returns false → everything 404s).

- [ ] **Step 3: Implement.** Replace the `match_template` stub in `route_registry.cpp`:

```cpp
    bool RouteRegistry::match_template(const ParamTemplate& tmpl, const std::string_view path,
                                       const std::pmr::polymorphic_allocator<> alloc,
                                       ResolvedRoute::ParamVec& out_params) {
        std::size_t i      = 0;
        const bool walked  = for_each_segment(path, [&](const std::string_view seg) {
            if (i >= tmpl.segments.size())
                return false;  // more incoming segments than the template has
            const PathSegment& ts = tmpl.segments[i++];
            if (!ts.is_param)
                return seg == ts.text;  // literal: raw byte compare (§8.6)
            if (seg.empty())
                return false;  // a param never captures an empty segment
            const auto decoded = url_decode_arena(seg, /*plus_is_space=*/false, alloc);
            if (!decoded)
                return false;  // malformed escape → this template does not match
            out_params.emplace_back(std::string_view{ts.text}, *decoded);
            return true;
        });
        return walked && i == tmpl.segments.size();
    }
```

- [ ] **Step 4: Build + run — expect pass (full Http.Routing suite)**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Routing 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add components/http/routing/route_registry tests/unit_tests/http/routing/test_route_registry.cpp
git commit -m "feat(http/routing): parametric matching with arena-decoded captures

Segment walk, no std::regex. Literals compare raw bytes; {name} captures
percent-decode via url_decode_arena (plus literal, §8.6) — zero-copy when
the segment has no escapes. Malformed escapes fail the template (404).
Exact match wins over parametric; params never capture empty segments."
```

---

## Task 8: HttpController — verb DSL, bake step, middleware composition machinery

**Files:**
- Modify: `common/nexus/core/include/nexus.hpp` (one-line macro fix)
- Create: `components/http/routing/controller/controller.hpp`
- Create: `components/http/routing/controller/controller.cpp`
- Create: `components/http/routing/controller/CMakeLists.txt`
- Create: `tests/unit_tests/http/routing/test_controller.cpp`
- Modify: `components/http/routing/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** The application-facing base class (spec §8.2) and the bake step (§8.3). The header lands complete —
including the Outcome-collapse machinery — because it is one cohesive template surface; this task's tests cover the
plain-handler shapes and bake mechanics, Task 9's tests cover the Outcome paths (same precedent as PR 1's Body task).
The spec's 12-overloads-per-verb DSL collapses to 3 per verb: member-`AsyncResponse`, member-`AsyncOutcome`, callable.

- [ ] **Step 1: Fix the NEXUS_REGISTER macro.** In `common/nexus/core/include/nexus.hpp` (line ~276):

```cpp
#define NEXUS_REGISTER(Policy) static constexpr Policy nexus_policy
```

becomes

```cpp
#define NEXUS_REGISTER(Policy) static constexpr Policy nexus_policy{}
```

A constexpr static data member requires an initializer; as written the macro cannot be used at all (it has zero
usages in the repo today — `HttpController` below is the first). `get_nexus_policy<T>()` reads the member unchanged.

- [ ] **Step 2: Write the failing tests** — `tests/unit_tests/http/routing/test_controller.cpp`:

```cpp
#include <memory>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include <controller.hpp>
#include <route_registry.hpp>

#include "routing_test_utils.hpp"

using namespace demiplane::http;
using http_routing_test::run_awaitable;

namespace {

    class PlainController final : public HttpController {
    public:
        int configure_calls = 0;

        void configure_routes() override {
            ++configure_calls;
            Get("/users", &PlainController::list);
            Post("/users", &PlainController::create);
        }

        // Public so a test can attempt late registration after bake.
        void late_register() {
            Get("/late", &PlainController::list);
        }

    private:
        AsyncResponse list(RequestContext ctx) {
            co_return ctx.ok("list");
        }
        AsyncResponse create(RequestContext ctx) {
            co_return ctx.created("made");
        }
    };

    AsyncResponse free_handler(RequestContext ctx) {
        co_return ctx.ok("free");
    }

    class KitchenSinkController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/k", &KitchenSinkController::h);
            Post("/k", &KitchenSinkController::h);
            Put("/k", &KitchenSinkController::h);
            Patch("/k", &KitchenSinkController::h);
            Delete("/k", &KitchenSinkController::h);
            Head("/k", &KitchenSinkController::h);
            Options("/k", &KitchenSinkController::h);
            Get("/lambda", [](RequestContext ctx) -> AsyncResponse { co_return ctx.ok("lambda"); });
            Get("/free", &free_handler);
        }

    private:
        AsyncResponse h(RequestContext ctx) {
            co_return ctx.ok("k");
        }
    };

    class OtherController final : public HttpController {
    public:
        void configure_routes() override {}
        AsyncResponse handler(RequestContext ctx) {
            co_return ctx.ok("other");
        }
    };

    class CrossRegisteringController final : public HttpController {
    public:
        void configure_routes() override {
            // Member of a DIFFERENT controller type — must throw at bake.
            Get("/cross", &OtherController::handler);
        }
    };

}  // namespace

class ControllerTest : public http_routing_test::RoutingTestBase {
protected:
    RouteRegistry registry_;

    void bake(const std::shared_ptr<HttpController>& ctrl, const std::string_view prefix = "") {
        detail::ControllerBaker::bake_into(registry_, ctrl, prefix);
    }

    Response invoke(const HttpMethod m, const std::string& path) {
        auto resolved = registry_.find_route(m, path, alloc_);
        EXPECT_TRUE(resolved.is_success()) << "no route for " << path;
        auto ctx = make_ctx(m, path);
        for (const auto& [n, v] : resolved.value().path_params)
            ctx.set_path_param(n, v);
        return run_awaitable((*resolved.value().handler)(std::move(ctx)));
    }
};

TEST_F(ControllerTest, MemberHandlersBakeAndDispatch) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl);
    ASSERT_TRUE(registry_.freeze().empty());

    EXPECT_EQ(*invoke(HttpMethod::get, "/users").body.buffered_view(), "list");
    EXPECT_EQ(invoke(HttpMethod::post, "/users").status, HttpStatus::created);
}

TEST_F(ControllerTest, ConfigureRoutesRunsExactlyOnce) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl);
    EXPECT_EQ(ctrl->configure_calls, 1);
}

TEST_F(ControllerTest, AllSevenVerbsPlusCallables) {
    auto ctrl = std::make_shared<KitchenSinkController>();
    bake(ctrl);
    ASSERT_TRUE(registry_.freeze().empty());

    for (const auto m : {HttpMethod::get, HttpMethod::post, HttpMethod::put, HttpMethod::patch,
                         HttpMethod::del, HttpMethod::head, HttpMethod::options}) {
        EXPECT_TRUE(registry_.find_route(m, "/k", alloc_).is_success());
    }
    EXPECT_EQ(*invoke(HttpMethod::get, "/lambda").body.buffered_view(), "lambda");
    EXPECT_EQ(*invoke(HttpMethod::get, "/free").body.buffered_view(), "free");
}

TEST_F(ControllerTest, PrefixAppliedAtBake) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl, "/api/v1");
    ASSERT_TRUE(registry_.freeze().empty());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/v1/users", alloc_).is_success());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/users", alloc_).is_error());
}

TEST_F(ControllerTest, DoubleBakeThrows) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl);
    EXPECT_THROW(bake(ctrl), std::logic_error);
}

TEST_F(ControllerTest, LateRegistrationThrows) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl);
    EXPECT_THROW(ctrl->late_register(), std::logic_error);
}

TEST_F(ControllerTest, AddMiddlewareAfterBakeThrows) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl);
    EXPECT_THROW(ctrl->add_middleware(
                     [](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
                         co_return co_await next(std::move(ctx));
                     }),
                 std::logic_error);
}

TEST_F(ControllerTest, CrossControllerMemberThrowsAtBake) {
    auto ctrl = std::make_shared<CrossRegisteringController>();
    EXPECT_THROW(bake(ctrl), std::logic_error);
}
```

- [ ] **Step 3: Register the test source** — add `routing/test_controller.cpp` to the `Http.Routing` source list in
  `tests/unit_tests/http/CMakeLists.txt`.

- [ ] **Step 4: Build — expect failure** (`controller.hpp` missing).

- [ ] **Step 5: Create `components/http/routing/controller/controller.hpp`**

```cpp
#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <demiplane/gears>
#include <demiplane/nexus>
#include <demiplane/scroll>

#include <async_outcome.hpp>
#include <errors.hpp>
#include <middleware.hpp>
#include <request_context.hpp>
#include <response.hpp>

namespace demiplane::http {

    class RouteRegistry;
    class HttpController;

    namespace detail {

        /// A typed error usable in a handler's Outcome: it must collapse to a
        /// Response via an ADL-found to_http_response(const E&). A missing
        /// overload fails this concept — the compile error names E (spec §8.3).
        template <typename E>
        concept HttpRenderableError = requires(const E& e) {
            { to_http_response(e) } -> std::same_as<Response>;
        };

        /// Maps a handler's return type onto the two supported shapes.
        template <typename R>
        struct RouteHandlerTraits {
            static constexpr bool valid = false;
        };
        template <>
        struct RouteHandlerTraits<boost::asio::awaitable<Response>> {
            static constexpr bool valid       = true;
            static constexpr bool has_outcome = false;
        };
        template <typename... Es>
        struct RouteHandlerTraits<boost::asio::awaitable<gears::Outcome<Response, Es...>>> {
            static constexpr bool valid       = (HttpRenderableError<Es> && ...);
            static constexpr bool has_outcome = true;
        };

        /// Collapse Outcome<Response, Es...> → Response via ADL (spec §8.3).
        /// The exact Response&& lambda beats the template in overload
        /// resolution, so errors land in the generic branch.
        template <typename OutcomeT>
        Response collapse_outcome(OutcomeT&& outcome) {
            return std::forward<OutcomeT>(outcome).visit(
                [](Response&& r) -> Response { return std::move(r); },
                []<typename E>(E&& e) -> Response { return to_http_response(e); });
        }

        /// Compose middlewares around `inner`, right-to-left, so the FIRST
        /// added middleware is the OUTERMOST (spec §8.3). Each layer is a
        /// plain (non-coroutine) lambda returning the middleware's awaitable
        /// directly — no wrapper frame; §11.1's budget stays at one frame per
        /// USER middleware. `next` lives in the layer closure inside the
        /// frozen chain, so the const& handed to the middleware (and held by
        /// its suspended frame) stays valid for the request's lifetime.
        ContextHandler wrap_with_middleware(ContextHandler inner,
                                            std::span<const Middleware> middlewares);

        /// The bake step (spec §8.1 phase 2): runs configure_routes() exactly
        /// once, then drains the controller's local routes into `registry`
        /// with `prefix` applied and the controller's middleware chain +
        /// Outcome collapse composed in. Called by GroupBinding (and by
        /// Server::add_controller in PR 5). Throws std::logic_error on a
        /// second bake of the same controller.
        struct ControllerBaker {
            static void bake_into(RouteRegistry& registry,
                                  const std::shared_ptr<HttpController>& ctrl,
                                  std::string_view prefix);
        };

    }  // namespace detail

    /// A callable route handler: invocable with a RequestContext (by value),
    /// returning AsyncResponse or AsyncOutcome<Response, Es...> where every E
    /// has an ADL to_http_response.
    template <typename F>
    concept RouteHandler =
        std::invocable<std::decay_t<F>&, RequestContext>
        && detail::RouteHandlerTraits<
            std::invoke_result_t<std::decay_t<F>&, RequestContext>>::valid;

    /**
     * @brief Application base class (spec §8.2). Subclasses register routes in
     *        configure_routes() via the protected verb DSL; GroupBinding bakes
     *        them into the server-wide registry with prefix + middleware +
     *        Outcome→Response conversion pre-composed.
     *
     * Lifecycle: construct → add_middleware()* → bake (via
     * GroupBinding::add_controller, which calls configure_routes() once) →
     * frozen. Registration or add_middleware after bake throws.
     */
    class HttpController : public std::enable_shared_from_this<HttpController> {
    public:
        NEXUS_REGISTER(nexus::Resettable);

        virtual ~HttpController() = default;

        /// Populate the local route table via the verb DSL. Called exactly
        /// once by the bake step — do not call it yourself.
        virtual void configure_routes() = 0;

        /// Lifecycle hooks; the Server wires them in PR 5.
        virtual void initialize() {
        }
        virtual void shutdown() {
        }

        /// Middlewares run in ADDITION ORDER (first added = outermost).
        template <typename Mw>
            requires std::constructible_from<Middleware, Mw&&>
        HttpController& add_middleware(Mw&& mw) {
            if (baked_)
                throw std::logic_error{"HttpController: add_middleware after bake"};
            middlewares_.push_back(Middleware{std::forward<Mw>(mw)});
            return *this;
        }

    protected:
        // ── Verb DSL: 3 shapes × 7 verbs (spec §8.2) ──────────────────────
        template <std::derived_from<HttpController> C>
        void Get(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::get, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Get(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::get, std::move(path), m);
        }
        template <RouteHandler F>
        void Get(std::string path, F&& f) {
            callable_route(HttpMethod::get, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Post(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::post, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Post(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::post, std::move(path), m);
        }
        template <RouteHandler F>
        void Post(std::string path, F&& f) {
            callable_route(HttpMethod::post, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Put(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::put, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Put(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::put, std::move(path), m);
        }
        template <RouteHandler F>
        void Put(std::string path, F&& f) {
            callable_route(HttpMethod::put, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Patch(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::patch, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Patch(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::patch, std::move(path), m);
        }
        template <RouteHandler F>
        void Patch(std::string path, F&& f) {
            callable_route(HttpMethod::patch, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Delete(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::del, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Delete(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::del, std::move(path), m);
        }
        template <RouteHandler F>
        void Delete(std::string path, F&& f) {
            callable_route(HttpMethod::del, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Head(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::head, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Head(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::head, std::move(path), m);
        }
        template <RouteHandler F>
        void Head(std::string path, F&& f) {
            callable_route(HttpMethod::head, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Options(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::options, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Options(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::options, std::move(path), m);
        }
        template <RouteHandler F>
        void Options(std::string path, F&& f) {
            callable_route(HttpMethod::options, std::move(path), std::forward<F>(f));
        }

    private:
        friend struct detail::ControllerBaker;

        using BakeFn = std::function<ContextHandler(const std::shared_ptr<HttpController>&,
                                                    std::span<const Middleware>)>;
        struct LocalRoute {
            HttpMethod method;
            std::string path;
            BakeFn bake;
        };

        void push_route(HttpMethod method, std::string path, BakeFn bake);

        template <std::derived_from<HttpController> C>
        static std::shared_ptr<C> typed_self(const std::shared_ptr<HttpController>& self) {
            // Bake-time only — never on the request path (spec §3 forbids
            // runtime dynamic_cast). Guards Get("/x", &OtherController::h)
            // cross-registration with a startup error instead of UB.
            auto typed = std::dynamic_pointer_cast<C>(self);
            if (!typed)
                throw std::logic_error{"HttpController: registered member function does not "
                                       "belong to the baked controller type"};
            return typed;
        }

        template <std::derived_from<HttpController> C>
        void member_route(const HttpMethod method, std::string path,
                          AsyncResponse (C::*m)(RequestContext)) {
            push_route(method, std::move(path),
                       [m](const std::shared_ptr<HttpController>& self,
                           const std::span<const Middleware> mws) -> ContextHandler {
                           // Plain lambda returning the member coroutine's
                           // awaitable directly — no wrapper frame. `typed`
                           // keeps the controller alive in the closure, which
                           // lives in the frozen registry.
                           ContextHandler inner = [typed = typed_self<C>(self),
                                                   m](RequestContext ctx) -> AsyncResponse {
                               return (typed.get()->*m)(std::move(ctx));
                           };
                           return detail::wrap_with_middleware(std::move(inner), mws);
                       });
        }

        template <std::derived_from<HttpController> C, typename... Es>
            requires(detail::HttpRenderableError<Es> && ...)
        void member_outcome_route(const HttpMethod method, std::string path,
                                  AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            push_route(method, std::move(path),
                       [m](const std::shared_ptr<HttpController>& self,
                           const std::span<const Middleware> mws) -> ContextHandler {
                           ContextHandler inner = [typed = typed_self<C>(self),
                                                   m](RequestContext ctx) -> AsyncResponse {
                               auto outcome = co_await (typed.get()->*m)(std::move(ctx));
                               co_return detail::collapse_outcome(std::move(outcome));
                           };
                           return detail::wrap_with_middleware(std::move(inner), mws);
                       });
        }

        template <RouteHandler F>
        void callable_route(const HttpMethod method, std::string path, F&& f) {
            using Fn     = std::decay_t<F>;
            using Traits = detail::RouteHandlerTraits<std::invoke_result_t<Fn&, RequestContext>>;
            push_route(
                method, std::move(path),
                [f = Fn{std::forward<F>(f)}](const std::shared_ptr<HttpController>&,
                                             const std::span<const Middleware> mws) -> ContextHandler {
                    ContextHandler inner;
                    if constexpr (Traits::has_outcome) {
                        inner = [f](RequestContext ctx) mutable -> AsyncResponse {
                            auto outcome = co_await f(std::move(ctx));
                            co_return detail::collapse_outcome(std::move(outcome));
                        };
                    } else {
                        inner = ContextHandler{f};  // signature matches exactly
                    }
                    return detail::wrap_with_middleware(std::move(inner), mws);
                });
        }

        bool configured_ = false;
        bool baked_      = false;
        std::vector<LocalRoute> local_routes_;
        std::vector<Middleware> middlewares_;

        SCROLL_COMPONENT_PREFIX("HttpController");
    };

}  // namespace demiplane::http
```

- [ ] **Step 6: Create `components/http/routing/controller/controller.cpp`**

```cpp
#include "controller.hpp"

#include <route_registry.hpp>

namespace demiplane::http {

    void HttpController::push_route(const HttpMethod method, std::string path, BakeFn bake) {
        if (baked_)
            throw std::logic_error{"HttpController: route registration after bake"};
        local_routes_.push_back(LocalRoute{method, std::move(path), std::move(bake)});
    }

    namespace detail {

        ContextHandler wrap_with_middleware(ContextHandler inner,
                                            const std::span<const Middleware> middlewares) {
            for (auto it = middlewares.rbegin(); it != middlewares.rend(); ++it) {
                NextHandler next = std::move(inner);
                // Plain lambda: returns the middleware coroutine's awaitable
                // directly (no wrapper frame). `mw` is copied once at BAKE
                // time; per request nothing is copied — `next` is handed down
                // by const& (spec §8.3).
                inner = [mw = *it, next = std::move(next)](RequestContext ctx) -> AsyncResponse {
                    return mw(std::move(ctx), next);
                };
            }
            return inner;
        }

        void ControllerBaker::bake_into(RouteRegistry& registry,
                                        const std::shared_ptr<HttpController>& ctrl,
                                        const std::string_view prefix) {
            if (!ctrl)
                throw std::invalid_argument{"ControllerBaker: null controller"};
            if (ctrl->baked_)
                throw std::logic_error{"HttpController: controller already baked"};
            if (!ctrl->configured_) {
                ctrl->configure_routes();
                ctrl->configured_ = true;
            }
            for (LocalRoute& route : ctrl->local_routes_) {
                registry.add_route(
                    route.method, join_path(prefix, route.path),
                    route.bake(ctrl, std::span<const Middleware>{ctrl->middlewares_}));
            }
            ctrl->baked_ = true;
            ctrl->local_routes_.clear();  // baked closures live in the registry now
        }

    }  // namespace detail

}  // namespace demiplane::http
```

- [ ] **Step 7: Create `components/http/routing/controller/CMakeLists.txt`**

```cmake
##############################################################################
# Http Routing — HttpController + verb DSL + bake step
##############################################################################
add_library(${DMP_HTTP}.Routing.Controller STATIC controller.cpp)

target_include_directories(${DMP_HTTP}.Routing.Controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Routing.Controller
        PUBLIC
        ${DMP_HTTP}.Routing.Middleware
        ${DMP_HTTP}.Types.Errors
        # collapse_outcome instantiates ADL to_http_response calls in consumer
        # TUs — the built-in definitions must travel with this target:
        ${DMP_HTTP}.Types.ErrorResponses
        Demiplane::Common::Gears
        Demiplane::Common::Nexus
        Demiplane::Common::Scroll
        PRIVATE
        ${DMP_HTTP}.Routing.RouteRegistry
)
##############################################################################
```

- [ ] **Step 8: Wire into the aggregate** — in `components/http/routing/CMakeLists.txt` add
  `add_subdirectory(controller)` and `${DMP_HTTP}.Routing.Controller` to the aggregate links.

- [ ] **Step 9: Configure + build + run — expect pass**

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Routing 2>&1 | tail -5
```

- [ ] **Step 10: Commit**

```bash
git add common/nexus/core/include/nexus.hpp components/http/routing tests/unit_tests/http/routing/test_controller.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/routing): HttpController verb DSL + bake step

Three handler shapes per verb (member AsyncResponse, member AsyncOutcome,
RouteHandler callable) instead of the old 12. Bake composes prefix +
middleware + Outcome collapse into one ContextHandler per route; plain
shapes return the handler's awaitable directly (no wrapper frame).
Cross-controller member registration throws at bake (dynamic_cast at
startup only). Fixes NEXUS_REGISTER (constexpr member lacked its
initializer; first real usage)."
```

---

## Task 9: Outcome→Response collapse — typed errors, user ADL overloads, concept gates

**Files:**
- Modify: `tests/unit_tests/http/routing/test_controller.cpp`

**Goal:** Behavioral coverage of the Outcome machinery that landed in Task 8: member and callable
`AsyncOutcome<Response, Es...>` handlers, success and every-error-alternative paths, a *user-defined* error type found
via ADL (spec §5.5/§8.3), and compile-time rejection of invalid handler shapes via `static_assert` (the spec's
"missing overload = compile error pointing at the offending type" property, asserted negatively through the concept).

- [ ] **Step 1: Write the failing tests** — append to `test_controller.cpp`. Add these includes at the top of the
  file: `#include <demiplane/gears>`, `#include <response_factory.hpp>`.

```cpp
// ── Outcome→Response collapse ───────────────────────────────────────────────

namespace myapp {

    struct TeapotError {
        std::string blend;
    };

    // User-defined ADL conversion — lives next to the error type (spec §5.5).
    inline demiplane::http::Response to_http_response(const TeapotError& e) {
        return demiplane::http::ResponseFactory::custom(
            demiplane::http::HttpStatus::unprocessable_entity, "teapot:" + e.blend);
    }

}  // namespace myapp

namespace {

    class OutcomeController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/users/{id}", &OutcomeController::get_user);
            Post("/users", &OutcomeController::create_user);
            Get("/tea", [](RequestContext ctx) -> AsyncOutcome<Response, myapp::TeapotError> {
                if (ctx.query<bool>("brew").value_or(false))
                    co_return ctx.ok("brewing");
                co_return demiplane::gears::err(myapp::TeapotError{"earl-grey"});
            });
        }

    private:
        AsyncOutcome<Response, NotFoundError> get_user(RequestContext ctx) {
            if (const auto id = ctx.path_param<int>("id"); id && *id == 42)
                co_return ctx.ok("user-42");
            co_return demiplane::gears::err(NotFoundError{"user", "?"});
        }

        AsyncOutcome<Response, BadRequestError, ForbiddenError> create_user(RequestContext ctx) {
            const auto mode = ctx.query<std::string>("mode");
            if (mode == "bad")
                co_return demiplane::gears::err(BadRequestError{"bad mode"});
            if (mode == "forbidden")
                co_return demiplane::gears::err(ForbiddenError{"no"});
            co_return ctx.created("ok");
        }
    };

}  // namespace

class OutcomeControllerTest : public ControllerTest {
protected:
    void SetUp() override {
        auto ctrl = std::make_shared<OutcomeController>();
        bake(ctrl);
        ASSERT_TRUE(registry_.freeze().empty());
    }
};

TEST_F(OutcomeControllerTest, SuccessAlternativePassesThrough) {
    EXPECT_EQ(*invoke(HttpMethod::get, "/users/42").body.buffered_view(), "user-42");
}

TEST_F(OutcomeControllerTest, TypedErrorCollapsesViaAdl) {
    EXPECT_EQ(invoke(HttpMethod::get, "/users/7").status, HttpStatus::not_found);
}

TEST_F(OutcomeControllerTest, MultiErrorPackEachAlternative) {
    // make_ctx targets carry the query string; invoke() routes on the path.
    auto resolved = registry_.find_route(HttpMethod::post, "/users", alloc_);
    ASSERT_TRUE(resolved.is_success());
    const auto run = [&](const std::string& target) {
        return run_awaitable((*resolved.value().handler)(make_ctx(HttpMethod::post, target)));
    };
    EXPECT_EQ(run("/users?mode=bad").status, HttpStatus::bad_request);
    EXPECT_EQ(run("/users?mode=forbidden").status, HttpStatus::forbidden);
    EXPECT_EQ(run("/users").status, HttpStatus::created);
}

TEST_F(OutcomeControllerTest, UserDefinedErrorTypeViaLambda) {
    auto resolved = registry_.find_route(HttpMethod::get, "/tea", alloc_);
    ASSERT_TRUE(resolved.is_success());
    const Response err = run_awaitable(
        (*resolved.value().handler)(make_ctx(HttpMethod::get, "/tea")));
    EXPECT_EQ(err.status, HttpStatus::unprocessable_entity);
    EXPECT_EQ(*err.body.buffered_view(), "teapot:earl-grey");

    const Response ok = run_awaitable(
        (*resolved.value().handler)(make_ctx(HttpMethod::get, "/tea?brew=true")));
    EXPECT_EQ(ok.status, HttpStatus::ok);
}

// ── Compile-time gates (spec §8.3: missing to_http_response = build break) ──

namespace {
    struct NotRenderable {};

    using GoodPlain   = decltype([](RequestContext) -> AsyncResponse { co_return Response{}; });
    using GoodOutcome = decltype([](RequestContext)
                                     -> AsyncOutcome<Response, NotFoundError> {
        co_return Response{};
    });
    using BadReturn   = decltype([](RequestContext) { return 42; });
    using BadError    = decltype([](RequestContext)
                                     -> AsyncOutcome<Response, NotRenderable> {
        co_return Response{};
    });

    static_assert(detail::HttpRenderableError<NotFoundError>);
    static_assert(detail::HttpRenderableError<myapp::TeapotError>);
    static_assert(!detail::HttpRenderableError<NotRenderable>);
    static_assert(RouteHandler<GoodPlain>);
    static_assert(RouteHandler<GoodOutcome>);
    static_assert(!RouteHandler<BadReturn>);
    static_assert(!RouteHandler<BadError>);  // error type without to_http_response
}  // namespace
```

- [ ] **Step 2: Build + run — expect pass** (machinery landed in Task 8; this is verification coverage — if anything
  fails, the bake/collapse implementation is wrong, fix it now):

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Routing 2>&1 | tail -5
```

- [ ] **Step 3: Commit**

```bash
git add tests/unit_tests/http/routing/test_controller.cpp
git commit -m "test(http/routing): Outcome collapse coverage — built-in, multi-error, user ADL

Covers success/error alternatives, a user-defined error type converted via
ADL to_http_response, and static_asserts that the RouteHandler concept
rejects wrong return types and non-renderable error packs."
```

---

## Task 10: Middleware behavior — order, short-circuit, post-mutation, ctx bag

**Files:**
- Create: `tests/unit_tests/http/routing/test_middleware.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** Behavioral coverage of `wrap_with_middleware` + `add_middleware` + `add_basic_middleware` through a real
controller bake: composition order (first added = outermost), short-circuiting (handler never runs), post-handler
response mutation (stays legal — Response's stored allocator), and the type-keyed bag flowing middleware → handler.

- [ ] **Step 1: Write the failing tests** — `tests/unit_tests/http/routing/test_middleware.cpp`:

```cpp
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <controller.hpp>
#include <response_factory.hpp>
#include <route_registry.hpp>

#include "routing_test_utils.hpp"

using namespace demiplane::http;
using http_routing_test::run_awaitable;

namespace {

    struct RequestId {
        int value = 0;
    };

    class MwController final : public HttpController {
    public:
        bool handler_ran = false;

        void configure_routes() override {
            Get("/traced", &MwController::traced);
            Get("/id", &MwController::with_id);
        }

    private:
        AsyncResponse traced(RequestContext ctx) {
            handler_ran = true;
            co_return ctx.ok("handled");
        }
        AsyncResponse with_id(RequestContext ctx) {
            const auto* id = ctx.get<RequestId>();
            co_return ctx.ok(id ? std::to_string(id->value) : "none");
        }
    };

    Middleware tracer(std::vector<std::string>& trace, std::string tag) {
        return [&trace, tag = std::move(tag)](RequestContext ctx,
                                              const NextHandler& next) -> AsyncResponse {
            trace.push_back(tag + ":before");
            auto r = co_await next(std::move(ctx));
            trace.push_back(tag + ":after");
            co_return r;
        };
    }

}  // namespace

class MiddlewareTest : public http_routing_test::RoutingTestBase {
protected:
    RouteRegistry registry_;
    std::shared_ptr<MwController> ctrl_ = std::make_shared<MwController>();

    void bake_and_freeze() {
        detail::ControllerBaker::bake_into(registry_, ctrl_, "");
        ASSERT_TRUE(registry_.freeze().empty());
    }

    Response invoke(const std::string& path) {
        auto resolved = registry_.find_route(HttpMethod::get, path, alloc_);
        EXPECT_TRUE(resolved.is_success());
        return run_awaitable((*resolved.value().handler)(make_ctx(HttpMethod::get, path)));
    }
};

TEST_F(MiddlewareTest, FirstAddedIsOutermost) {
    std::vector<std::string> trace;
    ctrl_->add_middleware(tracer(trace, "A"));
    ctrl_->add_middleware(tracer(trace, "B"));
    bake_and_freeze();

    EXPECT_EQ(*invoke("/traced").body.buffered_view(), "handled");
    const std::vector<std::string> expected{"A:before", "B:before", "B:after", "A:after"};
    EXPECT_EQ(trace, expected);
}

TEST_F(MiddlewareTest, ShortCircuitSkipsHandler) {
    ctrl_->add_middleware([](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        if (!ctx.header("Authorization"))
            co_return ResponseFactory::unauthorized();
        co_return co_await next(std::move(ctx));
    });
    bake_and_freeze();

    EXPECT_EQ(invoke("/traced").status, HttpStatus::unauthorized);
    EXPECT_FALSE(ctrl_->handler_ran);
}

TEST_F(MiddlewareTest, PostHandlerResponseMutation) {
    ctrl_->add_middleware([](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        auto r = co_await next(std::move(ctx));
        r.add_header("X-Traced", "1");
        co_return r;
    });
    bake_and_freeze();

    const Response r = invoke("/traced");
    ASSERT_TRUE(r.headers.get("X-Traced").has_value());
    EXPECT_EQ(*r.headers.get("X-Traced"), "1");
}

TEST_F(MiddlewareTest, BagFlowsFromMiddlewareToHandler) {
    ctrl_->add_middleware([](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        ctx.set(RequestId{42});
        co_return co_await next(std::move(ctx));
    });
    bake_and_freeze();

    EXPECT_EQ(*invoke("/id").body.buffered_view(), "42");
}

TEST_F(MiddlewareTest, AddBasicMiddlewareAppendsInOrder) {
    std::vector<std::string> trace;
    add_basic_middleware(*ctrl_, tracer(trace, "log"), tracer(trace, "auth"));
    bake_and_freeze();

    (void)invoke("/traced");
    const std::vector<std::string> expected{"log:before", "auth:before", "auth:after",
                                            "log:after"};
    EXPECT_EQ(trace, expected);
}
```

- [ ] **Step 2: Register the test source** — add `routing/test_middleware.cpp` to the `Http.Routing` source list.

- [ ] **Step 3: Build + run — expect pass** (implementation landed in Task 8 — this is the behavioral contract; any
  failure here is a real composition bug, fix `wrap_with_middleware`):

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Routing 2>&1 | tail -5
```

- [ ] **Step 4: Commit**

```bash
git add tests/unit_tests/http/routing/test_middleware.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "test(http/routing): middleware composition contract

Order (first added = outermost), short-circuit without invoking the
handler, post-handler response mutation, type-keyed bag flow, and
add_basic_middleware ordering."
```

---

## Task 11: GroupBinding — prefix-scoped controller mounting

**Files:**
- Create: `components/http/routing/group/group.hpp`
- Create: `components/http/routing/group/CMakeLists.txt`
- Create: `tests/unit_tests/http/routing/test_group.cpp`
- Modify: `components/http/routing/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** The public bake entry point (spec §8.4). Header-only: `add_controller` bakes + records the controller in a
caller-owned sink (the Server's controller list in PR 5); `in_group` nests prefixes via `join_path`.

- [ ] **Step 1: Write the failing tests** — `tests/unit_tests/http/routing/test_group.cpp`:

```cpp
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <controller.hpp>
#include <group.hpp>
#include <route_registry.hpp>

#include "routing_test_utils.hpp"

using namespace demiplane::http;

namespace {

    class UsersController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/users", &UsersController::list);
            Get("/users/{id}", &UsersController::one);
        }

    private:
        AsyncResponse list(RequestContext ctx) { co_return ctx.ok("users"); }
        AsyncResponse one(RequestContext ctx) { co_return ctx.ok("one"); }
    };

    class HealthController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/health", &HealthController::ping);
        }

    private:
        AsyncResponse ping(RequestContext ctx) { co_return ctx.ok("ok"); }
    };

    class ClashingController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/users", &ClashingController::also_users);
        }

    private:
        AsyncResponse also_users(RequestContext ctx) { co_return ctx.ok("clash"); }
    };

}  // namespace

class GroupTest : public http_routing_test::RoutingTestBase {
protected:
    RouteRegistry registry_;
    std::vector<std::shared_ptr<HttpController>> controllers_;

    GroupBinding root() { return GroupBinding{registry_, controllers_, ""}; }
};

TEST_F(GroupTest, PrefixAppliedToEveryRoute) {
    root().in_group("/api/v1").add_controller(std::make_shared<UsersController>());
    ASSERT_TRUE(registry_.freeze().empty());

    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/v1/users", alloc_).is_success());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/v1/users/7", alloc_).is_success());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/users", alloc_).is_error());
}

TEST_F(GroupTest, NestedGroupsConcatenatePrefixes) {
    root().in_group("/api").in_group("/v2").add_controller(std::make_shared<UsersController>());
    ASSERT_TRUE(registry_.freeze().empty());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/v2/users", alloc_).is_success());
}

TEST_F(GroupTest, MultipleControllersOneGroupAndChaining) {
    root()
        .in_group("/api")
        .add_controller(std::make_shared<UsersController>())
        .add_controller(std::make_shared<HealthController>());
    ASSERT_TRUE(registry_.freeze().empty());

    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/users", alloc_).is_success());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/health", alloc_).is_success());
    EXPECT_EQ(controllers_.size(), 2u);  // sink records ownership for PR 5 lifecycle
}

TEST_F(GroupTest, EmptyPrefixMountsAtRoot) {
    root().add_controller(std::make_shared<HealthController>());
    ASSERT_TRUE(registry_.freeze().empty());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/health", alloc_).is_success());
}

TEST_F(GroupTest, CrossControllerConflictSurfacesAtFreeze) {
    auto api = root().in_group("/api");
    api.add_controller(std::make_shared<UsersController>());
    api.add_controller(std::make_shared<ClashingController>());  // also GET /api/users

    const auto conflicts = registry_.freeze();
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0].method, HttpMethod::get);
    EXPECT_EQ(conflicts[0].path, "/api/users");
}

TEST_F(GroupTest, SameControllerInTwoGroupsThrows) {
    auto users = std::make_shared<UsersController>();
    root().in_group("/a").add_controller(users);
    EXPECT_THROW(root().in_group("/b").add_controller(users), std::logic_error);
}
```

- [ ] **Step 2: Register the test source** — add `routing/test_group.cpp` to the `Http.Routing` source list.

- [ ] **Step 3: Build — expect failure** (`group.hpp` missing).

- [ ] **Step 4: Create `components/http/routing/group/group.hpp`**

```cpp
#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <controller.hpp>
#include <route_registry.hpp>

namespace demiplane::http {

    /**
     * @brief Prefix-scoped controller mounting (spec §8.4).
     *
     * In PR 5 this is what Server::in_group(prefix) returns, constructed over
     * the Server's registry + controller list; until then callers construct it
     * directly. add_controller() runs the bake step: configure_routes() once,
     * prefix concat, middleware composition, Outcome→Response wiring, conflict
     * recording — and records the controller in the caller-owned sink (the
     * Server keeps them alive and drives initialize()/shutdown() in PR 5).
     */
    class GroupBinding {
    public:
        GroupBinding(RouteRegistry& registry,
                     std::vector<std::shared_ptr<HttpController>>& controller_sink,
                     std::string prefix)
            : registry_{&registry},
              controllers_{&controller_sink},
              prefix_{std::move(prefix)} {
        }

        /// Bakes + merges the controller's routes under this group's prefix.
        /// Throws std::logic_error if the controller was already baked.
        template <std::derived_from<HttpController> C>
        GroupBinding& add_controller(std::shared_ptr<C> ctrl) {
            std::shared_ptr<HttpController> base = std::move(ctrl);
            detail::ControllerBaker::bake_into(*registry_, base, prefix_);
            controllers_->push_back(std::move(base));
            return *this;
        }

        /// Nested group via combined prefix.
        [[nodiscard]] GroupBinding in_group(const std::string_view sub_prefix) const {
            return GroupBinding{*registry_, *controllers_, join_path(prefix_, sub_prefix)};
        }

        [[nodiscard]] const std::string& prefix() const noexcept {
            return prefix_;
        }

    private:
        RouteRegistry* registry_;
        std::vector<std::shared_ptr<HttpController>>* controllers_;
        std::string prefix_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/routing/group/CMakeLists.txt`**

```cmake
##############################################################################
# Http Routing — GroupBinding (header-only)
##############################################################################
add_library(${DMP_HTTP}.Routing.Group INTERFACE group.hpp)

target_include_directories(${DMP_HTTP}.Routing.Group INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Routing.Group INTERFACE
        ${DMP_HTTP}.Routing.Controller
        ${DMP_HTTP}.Routing.RouteRegistry
)
##############################################################################
```

- [ ] **Step 6: Wire into the aggregate** — `add_subdirectory(group)` + `${DMP_HTTP}.Routing.Group` in
  `components/http/routing/CMakeLists.txt`.

- [ ] **Step 7: Configure + build + run — expect pass**

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Routing 2>&1 | tail -5
```

- [ ] **Step 8: Commit**

```bash
git add components/http/routing/group components/http/routing/CMakeLists.txt tests/unit_tests/http/routing/test_group.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/routing): GroupBinding — prefix mounting, nesting, controller sink

server.in_group(...).add_controller(...) shape from spec §8.4, constructed
over a registry + caller-owned controller sink until the Server lands in
PR 5. Cross-controller conflicts aggregate at freeze()."
```

---

## Task 12: Router — the dispatch facade

**Files:**
- Create: `components/http/routing/router/router.hpp`
- Create: `components/http/routing/router/router.cpp`
- Create: `components/http/routing/router/CMakeLists.txt`
- Create: `tests/unit_tests/http/routing/test_router.cpp`
- Modify: `components/http/routing/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** Spec §8.8: `find_route` → routing misses collapse to Response via ADL → path params injected via the landed
`set_path_param` → handler invoked through the stored pointer. Handler exceptions escape (the h1 driver's catch-all →
500 is PR 3).

- [ ] **Step 1: Write the failing tests** — `tests/unit_tests/http/routing/test_router.cpp`:

```cpp
#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include <controller.hpp>
#include <group.hpp>
#include <route_registry.hpp>
#include <router.hpp>

#include "routing_test_utils.hpp"

using namespace demiplane::http;
using http_routing_test::run_awaitable;

namespace {

    class ApiController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/users/{id}", &ApiController::get_user);
            Post("/users", &ApiController::create);
            Get("/boom", &ApiController::boom);
        }

    private:
        AsyncResponse get_user(RequestContext ctx) {
            co_return ctx.ok("user:" + std::to_string(ctx.path_param<int>("id").value_or(-1)));
        }
        AsyncResponse create(RequestContext ctx) {
            co_return ctx.created("made");
        }
        AsyncResponse boom(RequestContext) {
            throw std::runtime_error{"handler exploded"};
        }
    };

}  // namespace

class RouterTest : public http_routing_test::RoutingTestBase {
protected:
    RouteRegistry registry_;
    std::vector<std::shared_ptr<HttpController>> controllers_;

    void SetUp() override {
        GroupBinding{registry_, controllers_, ""}.add_controller(
            std::make_shared<ApiController>());
        ASSERT_TRUE(registry_.freeze().empty());
    }
};

TEST_F(RouterTest, DispatchInvokesHandlerWithPathParams) {
    const Router router{registry_};
    const Response r = run_awaitable(router.dispatch(make_ctx(HttpMethod::get, "/users/42")));
    EXPECT_EQ(r.status, HttpStatus::ok);
    EXPECT_EQ(*r.body.buffered_view(), "user:42");
}

TEST_F(RouterTest, UnknownPathDispatchesTo404Response) {
    const Router router{registry_};
    const Response r = run_awaitable(router.dispatch(make_ctx(HttpMethod::get, "/missing")));
    EXPECT_EQ(r.status, HttpStatus::not_found);
}

TEST_F(RouterTest, WrongVerbDispatchesTo405WithAllowHeader) {
    const Router router{registry_};
    const Response r = run_awaitable(router.dispatch(make_ctx(HttpMethod::del, "/users")));
    EXPECT_EQ(r.status, HttpStatus::method_not_allowed);
    const auto allow = r.headers.get("Allow");
    ASSERT_TRUE(allow.has_value());
    EXPECT_NE(allow->find("POST"), std::string_view::npos);
}

TEST_F(RouterTest, QueryStringDoesNotConfuseRouting) {
    const Router router{registry_};
    const Response r = run_awaitable(
        router.dispatch(make_ctx(HttpMethod::get, "/users/7?verbose=1&x=%20")));
    EXPECT_EQ(*r.body.buffered_view(), "user:7");
}

TEST_F(RouterTest, HandlerExceptionsPropagateToCaller) {
    // The exception catch-all → 500 belongs to the h1 driver (PR 3, spec §6.3).
    const Router router{registry_};
    EXPECT_THROW(run_awaitable(router.dispatch(make_ctx(HttpMethod::get, "/boom"))),
                 std::runtime_error);
}
```

- [ ] **Step 2: Register the test source** — add `routing/test_router.cpp` to the `Http.Routing` source list.

- [ ] **Step 3: Build — expect failure** (`router.hpp` missing).

- [ ] **Step 4: Create `components/http/routing/router/router.hpp`**

```cpp
#pragma once

#include <boost/asio/awaitable.hpp>

#include <request_context.hpp>
#include <response.hpp>
#include <route_registry.hpp>

namespace demiplane::http {

    /**
     * @brief Thin dispatch facade the protocol drivers call (spec §8.8).
     *
     * find_route + path-param injection + handler invocation. Routing misses
     * (404/405) and handler typed errors are already collapsed to Response by
     * the time dispatch returns; exceptions escape to the driver's catch-all
     * (PR 3). The registry must be frozen before the first dispatch; frozen
     * means immutable, so concurrent dispatch from N io threads is safe.
     */
    class Router {
    public:
        explicit Router(const RouteRegistry& registry) noexcept
            : registry_{&registry} {
        }

        [[nodiscard]] boost::asio::awaitable<Response> dispatch(RequestContext ctx) const;

    private:
        const RouteRegistry* registry_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/routing/router/router.cpp`**

```cpp
#include "router.hpp"

#include <utility>

#include <errors.hpp>

namespace demiplane::http {

    boost::asio::awaitable<Response> Router::dispatch(RequestContext ctx) const {
        auto resolved = registry_->find_route(ctx.method(), ctx.path(), ctx.arena_alloc());
        if (!resolved) {
            co_return std::move(resolved).visit(
                [](ResolvedRoute&&) -> Response { std::unreachable(); },
                []<typename E>(E&& e) -> Response { return to_http_response(e); });
        }
        ResolvedRoute& route = resolved.value();
        for (const auto& [name, value] : route.path_params)
            ctx.set_path_param(name, value);
        co_return co_await (*route.handler)(std::move(ctx));
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Create `components/http/routing/router/CMakeLists.txt`**

```cmake
##############################################################################
# Http Routing — Router (dispatch facade for drivers)
##############################################################################
add_library(${DMP_HTTP}.Routing.Router STATIC router.cpp)

target_include_directories(${DMP_HTTP}.Routing.Router PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Routing.Router
        PUBLIC
        ${DMP_HTTP}.Routing.RouteRegistry
        Boost::asio
        PRIVATE
        ${DMP_HTTP}.Types.ErrorResponses
)
##############################################################################
```

- [ ] **Step 7: Wire into the aggregate** — `add_subdirectory(router)` + `${DMP_HTTP}.Routing.Router`. The aggregate
  in `components/http/routing/CMakeLists.txt` is now complete:

```cmake
add_subdirectory(middleware)
add_subdirectory(route_registry)
add_subdirectory(controller)
add_subdirectory(group)
add_subdirectory(router)
```

with the aggregate linking all five leaves.

- [ ] **Step 8: Configure + build + run — expect pass**

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Routing 2>&1 | tail -5
```

- [ ] **Step 9: Commit**

```bash
git add components/http/routing/router components/http/routing/CMakeLists.txt tests/unit_tests/http/routing/test_router.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/routing): Router dispatch facade

find_route → 404/405 collapse via ADL to_http_response → set_path_param
injection → handler invocation through the stored pointer. Handler
exceptions deliberately escape — the catch-all → 500 is the h1 driver's
job (PR 3)."
```

---

## Task 13: Routing allocation gate — find_route is zero-global-heap

**Files:**
- Create: `tests/unit_tests/http/routing/test_routing_allocation_gate.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** Enforce spec §8.5's per-request hygiene the same way PR 1 gated the response side (§14): replaced global
`operator new`/`operator delete`, thread-locally armed around the measured region. `find_route` must perform **zero**
global-heap allocations on: an exact hit (transparent lookup, handler-pointer not copy), a parametric hit including
trailing-slash normalization (view shrink), and a percent-decoded capture (arena rewrite). The 404/405 paths are cold
and exempt (they build `std::string`/`std::vector` error payloads by design).

- [ ] **Step 1: Write the failing-or-passing test** — `tests/unit_tests/http/routing/test_routing_allocation_gate.cpp`
  (the gate either passes — proving the invariant — or fails and points at a real hot-path allocation that must be
  fixed before commit):

```cpp
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <memory_resource>
#include <new>
#include <string>

#include <gtest/gtest.h>

#include <route_registry.hpp>

using namespace demiplane::http;

// ── Global operator new/delete instrumentation ──────────────────────────────
// Same mechanism as the PR 1 types gate (test_allocation_gate.cpp): replacing
// these in one TU instruments this whole test binary; counting is armed only
// inside measured regions via a thread_local flag. This sees what a pmr
// counter cannot: plain std::string/std::function/coroutine-frame allocations.
namespace {
    std::atomic<std::size_t> g_armed_allocs{0};
    thread_local bool t_armed = false;

    struct ArmedRegion {
        std::size_t start = g_armed_allocs.load(std::memory_order_relaxed);
        ArmedRegion() {
            t_armed = true;
        }
        [[nodiscard]] std::size_t finish() const {
            t_armed = false;
            return g_armed_allocs.load(std::memory_order_relaxed) - start;
        }
    };

    ContextHandler noop_handler() {
        return [](RequestContext ctx) -> AsyncResponse { co_return ctx.ok(); };
    }
}  // namespace

void* operator new(std::size_t size) {
    if (t_armed)
        g_armed_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(size))
        return p;
    throw std::bad_alloc{};
}
void* operator new(std::size_t size, std::align_val_t align) {
    if (t_armed)
        g_armed_allocs.fetch_add(1, std::memory_order_relaxed);
    const auto a              = static_cast<std::size_t>(align);
    const std::size_t rounded = (size + a - 1) / a * a;
    if (void* p = std::aligned_alloc(a, rounded))
        return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete(void* p, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    std::free(p);
}

namespace {
    // Stack-backed arena — mirrors the real RequestArena and never reaches
    // operator new. A size-only monotonic_buffer_resource would pull its first
    // block from new_delete_resource INSIDE the armed region. Do NOT
    // "simplify" to the size-only ctor (same trap as the PR 1 gate).
    struct StackArena {
        std::array<std::byte, 8192> buf{};
        std::pmr::monotonic_buffer_resource res{buf.data(), buf.size()};
        std::pmr::polymorphic_allocator<> alloc{&res};
    };
}  // namespace

TEST(RoutingAllocationGateTest, ExactMatchIsAllocationFree) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users/list", noop_handler());
    ASSERT_TRUE(reg.freeze().empty());
    StackArena arena;

    ArmedRegion region;
    auto resolved            = reg.find_route(HttpMethod::get, "/users/list", arena.alloc);
    const std::size_t allocs = region.finish();

    EXPECT_EQ(allocs, 0u) << "exact find_route touched the global heap";
    ASSERT_TRUE(resolved.is_success());
}

TEST(RoutingAllocationGateTest, ParametricMatchWithTrailingSlashIsAllocationFree) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users/{id}/posts/{post_id}", noop_handler());
    ASSERT_TRUE(reg.freeze().empty());
    StackArena arena;

    ArmedRegion region;
    auto resolved = reg.find_route(HttpMethod::get, "/users/12345/posts/678/", arena.alloc);
    const std::size_t allocs = region.finish();

    EXPECT_EQ(allocs, 0u) << "parametric find_route touched the global heap";
    ASSERT_TRUE(resolved.is_success());
    ASSERT_EQ(resolved.value().path_params.size(), 2u);
    EXPECT_EQ(resolved.value().path_params[0].second, "12345");  // zero-copy capture
}

TEST(RoutingAllocationGateTest, PercentDecodedCaptureStaysInTheArena) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/files/{name}", noop_handler());
    ASSERT_TRUE(reg.freeze().empty());
    StackArena arena;

    ArmedRegion region;
    auto resolved            = reg.find_route(HttpMethod::get, "/files/report%202026", arena.alloc);
    const std::size_t allocs = region.finish();

    EXPECT_EQ(allocs, 0u) << "capture decode escaped the arena";
    ASSERT_TRUE(resolved.is_success());
    EXPECT_EQ(resolved.value().path_params[0].second, "report 2026");
}
```

- [ ] **Step 2: Register the test source** — add `routing/test_routing_allocation_gate.cpp` to the `Http.Routing`
  source list. The final list:

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Routing
        routing/test_route_registry.cpp
        routing/test_controller.cpp
        routing/test_middleware.cpp
        routing/test_group.cpp
        routing/test_router.cpp
        routing/test_routing_allocation_gate.cpp
)
```

- [ ] **Step 3: Build + run — expect pass.** If a gate test fails, the count names a real hot-path allocation
  (a `std::string` key build, a copied `std::function`, an eager error construction) — fix `find_route`, do not relax
  the gate:

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Routing 2>&1 | tail -5
```

- [ ] **Step 4: Commit**

```bash
git add tests/unit_tests/http/routing/test_routing_allocation_gate.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "test(http/routing): allocation gate — find_route is zero-global-heap

Armed global operator new counter (PR1 gate mechanism) proves exact and
parametric lookups — including trailing-slash normalization and %-decoded
captures — never touch the global heap. 404/405 are cold and exempt."
```

---

## Task 14: Full verification — whole suite, ASan, self-review

**Files:** none (verification only; fix-forward if anything fails).

- [ ] **Step 1: Full release build + full unit suite**

```bash
cmake --build build/release -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit 2>&1 | tail -15
```

Expected: build clean, all unit tests green (Http.Types grew by ~10 tests in Tasks 2–4; Http.Routing ~45 tests;
everything else untouched — any non-http failure is pre-existing, investigate before assuming).

- [ ] **Step 2: ASan/UBSan pass** (the riskiest code: coroutine-lambda capture lifetimes through the baked chain and
  arena-view lifetimes through find_route)

```bash
cmake --preset asan 2>&1 | tail -3
cmake --build build/asan --target Demiplane.Tests.Unit.Http.Routing Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -5
ctest --test-dir build/asan --output-on-failure -R "Http.Types|Http.Routing" 2>&1 | tail -10
```

Expected: zero ASan/UBSan reports. (The allocation-gate tests run under ASan too — ASan intercepts
`operator new`/`malloc`, the armed counter still counts because our replaced operators are what the TU links; if ASan
interposition makes the gate counts unreliable on this toolchain, gate tests may be skipped under sanitizers via
`GTEST_SKIP() when __has_feature(address_sanitizer)` — only do this if actually observed, and note it in the test.)

- [ ] **Step 3: Self-review against the spec** (run the checklist, fix inline, re-run affected tests):

1. **Spec coverage** — §8.1 three-phase lifecycle (build/bake/frozen) ✔ Tasks 5+8; §8.2 controller DSL ✔ Task 8;
   §8.3 bake + middleware composition + `next` by const& ✔ Tasks 8–10; §8.4 groups + nesting ✔ Task 11;
   §8.5 registry storage, transparent lookup, handler-pointer, 404-vs-405, no-regex parametric, `unknown` slot ✔
   Tasks 5–7+13; §8.6 normalization + split-before-decode + `%`-rejection + `+`-literal ✔ Tasks 4–7;
   §8.7 conflict aggregation ✔ Tasks 5+11; §8.8 Router ✔ Task 12. Deferred (documented): firewall data types,
   `RouteConflictAggregateError` throw (PR 5, needs Server::setup), Nexus registration of controllers into a live
   Nexus instance (PR 5).
2. **No placeholders** — Tasks 5–7 share `route_registry.cpp` via explicit compiling stubs that later tasks replace;
   every stub's replacement body is spelled out in its task. No TBDs anywhere.
3. **Type consistency spot-checks** — `ContextHandler`/`NextHandler` (middleware.hpp) used by registry/controller/
   router; `ResolvedRoute::path_params` is `small_vector<pair<string_view, string_view>>` consumed by
   `Router::dispatch` via `set_path_param(name, value)`; `kHttpMethodCount` sizes `MethodSlots`;
   `detail::ControllerBaker::bake_into(registry, ctrl, prefix)` called by both `ControllerTest::bake` and
   `GroupBinding::add_controller`.

- [ ] **Step 4: Status report.** Summarize: tests added/passing, ASan result, any deviations taken during execution.
  Do NOT push or open a PR unless explicitly asked.

---

## Out of scope (deferred, with owners)

| Item | Where it lands |
|---|---|
| `RouteConflictAggregateError` thrown from setup | PR 5 (`Server::setup()` aggregates `freeze()`'s return) |
| Controllers registered into a live Nexus | PR 5 (Server lifecycle; `NEXUS_REGISTER` policy is in place now) |
| Driver catch-all → 500, `Date`/`Server` stamping | PR 3 (`Http11Driver::serve`, spec §6.3) |
| `routing/firewall/` data types (`rate_limit`, `ip_rule`) | When rate-limit middleware exists (spec non-goal 5) |
| Auto-HEAD fallback to GET handlers | Driver-level decision, PR 3+ (registry stays verb-exact) |
| `PathNormalization` sourced from `ServerConfig` | PR 6 (config enum maps onto the routing enum) |
