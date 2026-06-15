# HTTP Redesign — PR 3: Connection + Driver Layer (`Http11Driver`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the connection layer (`RequestArena`, `Connection`/`StreamConnection` concepts, `TcpConnection`, QUIC
scaffold) and the driver layer (`HttpDriver` concept, a real `Http11Driver`, h2/h3 scaffolds) as per-leaf
static/interface libraries, plus the request-side `Body` payload, with full unit-test coverage against in-memory fake
connections. After this PR the framework can turn raw HTTP/1.1 wire bytes into a `RequestContext`, dispatch through the
PR-2 `Router`, and write a `Response` back — the bug-fix battery (URL decode, body limits, per-phase timeouts,
handler-exception → 500) lands here. Spec: §6 + §12.2 PR 3 of
`docs/superpowers/specs/2026-05-07-http-redesign-design.md`.

**Architecture:** A thin protocol layer on top of the landed Types (PR 1) and Routing (PR 2) layers — no listeners, no
`Server`, no real sockets in the test path. A *connection* is a value type satisfying a duck-typed concept (its own byte
stream + a per-connection `RequestArena`); a *driver* is templated on the connection type, so `serve()` picks the stream
type at compile time — no virtual base, no `dynamic_cast`. `Http11Driver::serve()` runs the keep-alive session loop over
Boost.Beast: parse with the request parser's body allocator pointed at the request arena, build a `RequestContext`,
`co_await router.dispatch(...)`, stamp `Date`/`Server`, write the `Response` zero-copy through `buffer_body`. h2/h3
drivers and the QUIC connection ship as compiling scaffolds; their vcpkg deps are wired so the future impl PRs touch no
build files. Drivers are tested against a `TestConnection` wrapping `boost::beast::test::stream` (in-memory, no kernel
sockets).

**Tech Stack:** C++23 (concepts, coroutines, `std::pmr`, deducing-this consumers), Boost.Beast (`request_parser`,
`response<buffer_body>`, `test::stream`), Boost.Asio (`awaitable`, `bind_cancellation_slot`, `co_spawn`), the landed
PR1/PR2 leaf targets, GoogleTest. Links `Demiplane.Component.HTTP.Routing` + `.Types` (dotted names; there are no `::`
aliases — see Reconciliation).

---

## Reconciliation against the landed code + spec (read before executing)

The spec §6.3 sketch predates the landed Types/Routing layers. These are the facts on the ground and the deviations
this plan deliberately takes; each was verified against the repo and, where load-bearing, by a compiled spike.

### Landed facts (authoritative over the spec)

1. **Per-leaf CMake convention.** Each "thing" is its own leaf target owning its include dir (headers cross-reference as
   `<router.hpp>`, `<response.hpp>`, never `../`), aggregated by an INTERFACE target per layer. Dotted names only
   (`Demiplane.Component.HTTP.Drivers`); **no `::` aliases.** `${DMP_HTTP}` = `${DMP_COMPONENT}.HTTP`.
2. **`Router(const RouteRegistry&)`** with `boost::asio::awaitable<Response> dispatch(RequestContext) const` (
   router.hpp).
   It already collapses 404/405 + typed handler errors to a `Response`; **handler exceptions escape** — the catch-all →
   500 is this PR's job.
3. **`RequestContext(Request, std::pmr::polymorphic_allocator<>)`** (request_context.hpp). Move-construct only.
   `method()/target()/version()/headers()/body()`, `set_path_param`, arena factories `ok/json/created/...`.
4. **`Request : gears::NonCopyable`** — `explicit Request(Headers)`, public
   `method/version/target(string_view)/headers/body`
   (request.hpp).
5. **`Response : gears::NonCopyable`** — `explicit Response(alloc={})`, sticky-allocator move-assign, fluent setters,
   `headers` (owned), `body` (value SBO) (response.hpp).
6. **`Headers::view_of_beast(const boost::beast::http::fields&)`** is the *only* incoming-view factory, and its
   parameter
   type is fixed to `http::fields` (i.e. `basic_fields<std::allocator<char>>`) (headers.hpp). This constrains the parser
   (see deviation D1).
7. **`Body`** is a value SBO type (48-byte budget) with a private `emplace_t` constructor + `vtable_for<T>()`; payloads
   are added *inside* the Types.Body leaf (body.cpp). The header comment already reserves `BeastRequestBody` for "PR3".
8. **`ResponseFactory`** (cold-path, global heap) has `internal_error()`, `payload_too_large()`, `bad_request()`, etc.
9. **`COMPONENT_LOG_WRN()`** is the warn macro (common/scroll/provider/include/log_macros.hpp), paired with a
   class-scope
   `SCROLL_COMPONENT_PREFIX("Name")`; gated behind `DMP_COMPONENT_LOGGING` (no-op otherwise).
10. **Test harness.** `add_unit_test(${UNIT_TESTING_TARGET}.Http.<Layer> <sources>)` + link the dotted layer target(s)
    + `${TEST_LIBS}`, labelled `unit`. `boost/beast/_experimental/test/stream.hpp` ships with `Boost::beast` (verified
      present in vcpkg). Integration tests (`tests/integration_tests/http/`, real `127.0.0.1:0` sockets) are **PR 4** —
      PR 3
      is unit-only.

### Deviations taken (each documented in the relevant task)

- **D1 — Split-allocator parse (supersedes the §6.3 `buffer_body` sketch).** The driver parses with
  `request_parser<BeastBody, std::allocator<char>>` where
  `BeastBody = basic_string_body<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>`, constructed
  `parser{std::piecewise_construct, std::forward_as_tuple(arena_char), std::forward_as_tuple()}`. **Verified by a
  compiled spike:** the request **body** bytes land in the request arena (large bodies) or inline-SSO in the message
  (small bodies) — **zero global-heap allocation for the body** — while `req.base()` stays a `http::fields`, so the
  landed `Headers::view_of_beast` is used **unchanged**. This is simpler and cleaner than `buffer_body`'s manual read
  loop while honoring the body half of the zero-alloc invariant.
- **D2 — Header field storage is global-heap in v1.** Because the fields allocator is `std::allocator` (forced by D1's
  compatibility with `Headers::view_of_beast`), Beast's header *parsing* allocates on the global heap (small, few). This
  deviates from spec §11.1's "Beast parsing into fields → 0". Honoring it requires generalizing `Headers::BeastBacking`
  over the fields allocator (a Types.Headers change). **Deferred, documented.** It is outside this PR's gated region
  anyway (header parsing happens inside Beast's `async_read_header`, i.e. Beast I/O — see D5).
- **D3 — `TlsConnection` deferred to PR 4.** PR 3 ships `TcpConnection` + the QUIC scaffold. A `TlsConnection` value
  type
  is only meaningfully testable once the TLS listener drives the handshake + ALPN (PR 4); landing it here would add
  OpenSSL surface with no PR-3 test to exercise it. (Same spirit as PR 2's reconciliation notes.)
- **D4 — h2/h3 vcpkg deps added to the manifest but NOT linked.** The scaffolds reference zero symbols from
  nghttp2/ngtcp2/nghttp3, so linking them only risks target-name fragility and a slow/failing port build. Manifest entry
  satisfies the spec's "deps wired so future PRs touch no build files"; `find_package`+link lands with the real impl.
  Fallback if ngtcp2/nghttp3 fail to build in-env: nghttp2-only, defer the other two to the h3 PR (Task 2).
- **D5 — Allocation gate scoped to the framework translation path (differential).** A compiled spike showed Beast's
  `async_read`/`async_write` each do ~1 global allocation (composed-op state) irrespective of our allocators, so a full
  `serve()` exact-count gate is impossible. The PR-3 gate measures only **`build_request_context` → `router.dispatch`**
  (the framework path that builds the arena-backed `Response`) under an armed global-`operator new` counter.
  **`make_beast_response` and the async I/O are the Beast-translation boundary and are excluded** — Beast's
  `fields::insert` allocates one node per response header, by construction. It asserts **differentials** (no magic frame
  budget): arena response-header mutation adds **0**; one user-body string adds exactly **1**. This deviates from spec
  §11.1's "on the wire / exact budget" language with the rationale above.
- **D6 — `HEAD` and `Expect: 100-continue` deferred.** The registry is verb-exact (PR 2 deferred auto-HEAD→GET); the
  driver treats all methods uniformly and does not suppress bodies for HEAD or honor `100-continue` in v1. Documented;
  revisit when a HEAD route is actually registered.
- **D7 — Timeout *behavior* is PR 4.** `beast::test::stream` has no timer, so `TestConnection::expires_after` is a
  no-op.
  The driver wires header/body timeouts (`conn.expires_after(...)`); a real socket exercises them firing in PR 4. PR 3
  asserts the wiring compiles + the happy path; it does **not** write a "timeout fires → close" test it cannot satisfy.

### Validated mechanisms (spike results the executor can trust)

| Mechanism                                                                                               | Result                                                                                  |
|---------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------|
| `request_parser<basic_string_body<…,pmr>, std::allocator>` + `piecewise_construct{arena,{}}`            | compiles; **body bytes in arena** (large) / SSO (small); `req.base()` is `http::fields` |
| `response<buffer_body>` with `body().data` = external `std::string` + `more=false` + `content_length()` | `async_write` succeeds; **zero-copy**; correct on the wire                              |
| Multi-value response headers via `msg.insert(name, value)`                                              | round-trips (e.g. two `Set-Cookie`)                                                     |
| `msg.result(unsigned)`                                                                                  | Beast auto-fills the reason phrase — no `to_string(HttpStatus)` needed                  |
| Beast `async_read`/`async_write`                                                                        | ~1 global alloc each (gate excludes Beast I/O — D5)                                     |

**Build prerequisites (execution note, 2026-06-14):** Use the **`debug`** preset (`build/debug`) for all per-task
verification — the `release` preset currently fails to configure with a pre-existing, PR3-unrelated
`find_package(Threads)` error under its project-local vcpkg toolchain (`vcpkg/`, Boost 1.91, `x64-linux-clang`); `debug`
reconfigures and builds cleanly under the same strict flags (`-Werror -Wconversion -Wshadow -Wthread-safety` …).
Substitute `--preset debug` / `build/debug` wherever a task says `release`. Sanitizer steps (Task 17) use `asan`/`tsan`
*if they configure*; otherwise fall back to debug. **Task 2 (vcpkg deps) is deferred to run after Task 16** — nothing in
PR3 *code* links nghttp2/ngtcp2/nghttp3 (D4), so adding the ports (which triggers a vcpkg port build that may fail for
ngtcp2) must not block the implementation. Work happens on the current branch `component/http-1.1/v1.3`; **no git
commits** in this execution — the user manages git.

---

## File Structure

```
components/http/connection/
├─ CMakeLists.txt                       aggregate INTERFACE ${DMP_HTTP}.Connection (grows per task)
├─ request_arena/   {request_arena.hpp, request_arena.cpp, CMakeLists.txt}   STATIC  ${DMP_HTTP}.Connection.RequestArena
├─ concepts/        {connection_concepts.hpp, CMakeLists.txt}                INTERFACE ${DMP_HTTP}.Connection.Concepts
├─ tcp_connection/  {tcp_connection.hpp, tcp_connection.cpp, CMakeLists.txt} STATIC  ${DMP_HTTP}.Connection.Tcp
└─ quic_connection/ {quic_connection.hpp, CMakeLists.txt}                    INTERFACE ${DMP_HTTP}.Connection.Quic  (scaffold)
   (tls_connection/ deferred to PR 4 — D3)

components/http/drivers/
├─ CMakeLists.txt                       aggregate INTERFACE ${DMP_HTTP}.Drivers (grows per task)
├─ http_driver/  {http_driver.hpp, CMakeLists.txt}                          INTERFACE ${DMP_HTTP}.Drivers.Concept
├─ http11/       {http11_config.hpp, http11_driver.hpp, http11_driver.cpp, CMakeLists.txt}  STATIC ${DMP_HTTP}.Drivers.Http11
├─ http2/        {http2_driver.hpp, CMakeLists.txt}                         INTERFACE ${DMP_HTTP}.Drivers.Http2  (scaffold)
└─ http3/        {http3_driver.hpp, CMakeLists.txt}                         INTERFACE ${DMP_HTTP}.Drivers.Http3  (scaffold)

tests/unit_tests/http/connection/
├─ test_request_arena.cpp
├─ test_connection_concepts.cpp
└─ test_tcp_connection.cpp

tests/unit_tests/http/drivers/
├─ driver_test_utils.hpp               TestConnection (over beast::test::stream) + DriverFixture exchange harness
├─ test_driver_helpers.cpp             build_request_context / make_beast_response / stamp_common_headers (no stream)
├─ test_http11_driver.cpp             serve(): verbs, params, body, keep-alive, 404/405, exception→500, 413, stamping
├─ test_driver_scaffolds.cpp          HttpDriver concept conformance + h2/h3 id()/accepted_alpns()/serve logs+closes
└─ test_driver_allocation_gate.cpp    framework-translation differential gate (D5)

Modified:
├─ vcpkg.json                                              + nghttp2, ngtcp2, nghttp3  (Task 2)
├─ components/http/types/body/body.hpp                     + Body::beast_view declaration (Task 5)
├─ components/http/types/body/body.cpp                     + BeastRequestBody payload + factory (Task 5)
├─ components/http/CMakeLists.txt                          + add_subdirectory(connection) + add_subdirectory(drivers)
└─ tests/unit_tests/http/CMakeLists.txt                    + Http.Connection + Http.Drivers test targets
```

Per-task verification: `cmake --build build/release --target <target> -- -j4` clean;
`ctest --test-dir build/release --output-on-failure -R <pattern>` passes.

---

## Task 1: Bootstrap connection + drivers layer skeletons

**Files:**

- Create: `components/http/connection/CMakeLists.txt`
- Create: `components/http/drivers/CMakeLists.txt`
- Modify: `components/http/CMakeLists.txt`

**Goal:** Both layer aggregates registered in the build as empty INTERFACE targets. No leaves yet (first leaf lands in
Task 3); no test (same precedent as PR 1/PR 2 Task 1).

- [ ] **Step 1: Create the directory trees**

```bash
cd /home/grivin/Workspace/Demiplane
mkdir -p components/http/connection/{request_arena,concepts,tcp_connection,quic_connection}
mkdir -p components/http/drivers/{http_driver,http11,http2,http3}
mkdir -p tests/unit_tests/http/connection tests/unit_tests/http/drivers
```

- [ ] **Step 2: Create `components/http/connection/CMakeLists.txt`**

```cmake
##############################################################################
# Http Connection — per-connection arena, the Connection concept, concrete
# connections. Per-leaf convention (each thing owns its include dir); the
# dotted ${DMP_HTTP}.Connection target is an INTERFACE aggregate. Leaves are
# added by subsequent tasks.
##############################################################################

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Connection INTERFACE)
##############################################################################
```

- [ ] **Step 3: Create `components/http/drivers/CMakeLists.txt`**

```cmake
##############################################################################
# Http Drivers — the HttpDriver concept + protocol drivers (h1 real, h2/h3
# scaffolds). Per-leaf convention; the dotted ${DMP_HTTP}.Drivers target is an
# INTERFACE aggregate. Leaves are added by subsequent tasks.
##############################################################################

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Drivers INTERFACE)
##############################################################################
```

- [ ] **Step 4: Register both layers.** In `components/http/CMakeLists.txt`, after the routing block:

```cmake
##############################################################################
# Http Routing layer (PR 2 of redesign)
##############################################################################
add_subdirectory(routing)
##############################################################################
```

add:

```cmake
##############################################################################
# Http Connection layer (PR 3 of redesign)
##############################################################################
add_subdirectory(connection)
##############################################################################


##############################################################################
# Http Drivers layer (PR 3 of redesign)
##############################################################################
add_subdirectory(drivers)
##############################################################################
```

- [ ] **Step 5: Configure + sanity build**

```bash
cmake --preset release 2>&1 | tail -5
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
```

Expected: configure succeeds; existing routing tests still build. The empty INTERFACE aggregates validate at configure
time.

- [ ] **Step 6: Commit**

```bash
git add components/http/connection/CMakeLists.txt components/http/drivers/CMakeLists.txt components/http/CMakeLists.txt
git commit -m "feat(http): bootstrap connection + drivers layer skeletons

Empty INTERFACE aggregates Demiplane.Component.HTTP.Connection and
.Drivers registered in the http component. Leaves land per task."
```

---

## Task 2: vcpkg deps for h2/h3 scaffolds (nghttp2 / ngtcp2 / nghttp3)

**Files:**

- Modify: `vcpkg.json`

**Goal:** Wire the h2/h3 transitive libraries into the manifest so the future impl PRs touch no build files (spec §13).
Per **D4** they are NOT linked in PR 3 (the scaffolds reference no symbols). **This task is isolated because adding the
ports triggers a vcpkg build on the next configure** — potentially slow, and ngtcp2 needs a crypto backend that may not
build cleanly in every environment.

- [ ] **Step 1: Add the three ports.** In `vcpkg.json`, the dependencies array currently starts:

```json
  "dependencies": [
    "boost-program-options",
    "boost-unordered",
```

Change to:

```json
  "dependencies": [
    "boost-program-options",
    "boost-unordered",
    "nghttp2",
    "ngtcp2",
    "nghttp3",
```

- [ ] **Step 2: Configure and let vcpkg build the ports (slow — first time only)**

```bash
cmake --preset release 2>&1 | tail -30
```

Expected: vcpkg resolves and builds nghttp2, ngtcp2, nghttp3, then the normal configure completes. Confirm all three
installed:

```bash
ls -d /opt/vcpkg/installed/x64-linux/include/nghttp2 \
      /opt/vcpkg/installed/x64-linux/include/ngtcp2 \
      /opt/vcpkg/installed/x64-linux/include/nghttp3 2>&1
```

- [ ] **Step 3: FALLBACK if ngtcp2 or nghttp3 fail to build (D4).** If the configure errors on ngtcp2/nghttp3 build,
  revert them and keep nghttp2 only:

```json
  "dependencies": [
    "boost-program-options",
    "boost-unordered",
    "nghttp2",
```

Re-run `cmake --preset release`. Note in the commit message that ngtcp2/nghttp3 are deferred to the h3 PR. The h2/h3
*scaffolds* (Task 15) compile regardless — they include none of these headers.

- [ ] **Step 4: Sanity build (nothing links the new ports yet)**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add vcpkg.json
git commit -m "build(http): add nghttp2/ngtcp2/nghttp3 to the manifest for h2/h3 scaffolds

Headers available for the future h2/h3 impl PRs (spec §13). Not linked in
PR 3 — the scaffolds reference no symbols (D4). [If fallback taken: ngtcp2/
nghttp3 deferred to the h3 PR; nghttp2 only.]"
```

---

## Task 3: `RequestArena` — one heap block per connection, reused across requests

**Files:**

- Create: `components/http/connection/request_arena/request_arena.hpp`
- Create: `components/http/connection/request_arena/request_arena.cpp`
- Create: `components/http/connection/request_arena/CMakeLists.txt`
- Create: `tests/unit_tests/http/connection/test_request_arena.cpp`
- Modify: `components/http/connection/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** Spec §6.1: one heap block of `request_arena_size` (default 8 KB) allocated per connection, reused across every
keep-alive request via `reset()` (rewinds to the initial block). Non-movable (holds a `monotonic_buffer_resource`, which
is neither copyable nor movable) — concrete connections compose it by value and are therefore non-movable too
(constructed in place).

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/connection/test_request_arena.cpp`

```cpp
#include <cstddef>
#include <memory_resource>

#include <gtest/gtest.h>

#include <request_arena.hpp>

using demiplane::http::RequestArena;

TEST(RequestArenaTest, AllocatesFromTheInitialBlock) {
    RequestArena arena{1024};
    auto alloc = arena.allocator();
    void* p = alloc.allocate_bytes(64, alignof(std::max_align_t));
    ASSERT_NE(p, nullptr);
}

TEST(RequestArenaTest, ResetRewindsAndReusesTheSameBlock) {
    RequestArena arena{1024};
    void* first = arena.allocator().allocate_bytes(128, 1);
    arena.reset();
    void* second = arena.allocator().allocate_bytes(128, 1);
    // After reset the monotonic resource hands back the start of the initial
    // block again — same address, no new heap block.
    EXPECT_EQ(first, second);
}

TEST(RequestArenaTest, AllocatorPointsAtThisArenasResource) {
    RequestArena a{512};
    RequestArena b{512};
    EXPECT_NE(a.allocator().resource(), b.allocator().resource());
    EXPECT_EQ(a.allocator().resource(), a.allocator().resource());
}
```

- [ ] **Step 2: Wire the test target** — append to `tests/unit_tests/http/CMakeLists.txt`:

```cmake
##############################################################################
# Test HTTP Connection layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Connection
        connection/test_request_arena.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Connection
        PRIVATE
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Types
        ${TEST_LIBS}
)
##############################################################################
```

(The source list grows in Tasks 4 + 14; check before appending.)

- [ ] **Step 3: Configure + build — expect failure** (`request_arena.hpp` missing):

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Connection -- -j4 2>&1 | tail -10
```

- [ ] **Step 4: Create `components/http/connection/request_arena/request_arena.hpp`**

```cpp
#pragma once

#include <cstddef>
#include <memory>
#include <memory_resource>

namespace demiplane::http {

    /**
     * @brief One heap block per CONNECTION, reused across every keep-alive
     *        request on it (spec §6.1).
     *
     * Allocated once at `size` bytes (ServerConfig::request_arena_size, default
     * 8 KB); reset() rewinds the monotonic resource to the initial block, so
     * the next request reuses the same memory — amortized, not per-request.
     * Requests that exceed the block grow via upstream new_delete blocks.
     *
     * Non-copyable AND non-movable: monotonic_buffer_resource is immovable, so
     * connections compose this by value and are constructed in place.
     */
    class RequestArena {
    public:
        explicit RequestArena(const std::size_t size = 8192)
            : initial_block_{std::make_unique<std::byte[]>(size)},
              resource_{initial_block_.get(), size} {
        }

        RequestArena(const RequestArena&)            = delete;
        RequestArena& operator=(const RequestArena&) = delete;
        RequestArena(RequestArena&&)                 = delete;
        RequestArena& operator=(RequestArena&&)      = delete;

        [[nodiscard]] std::pmr::polymorphic_allocator<> allocator() noexcept {
            return std::pmr::polymorphic_allocator<>{&resource_};
        }

        void reset() {
            resource_.release();  // rewinds to the initial block
        }

    private:
        std::unique_ptr<std::byte[]> initial_block_;
        std::pmr::monotonic_buffer_resource resource_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/connection/request_arena/request_arena.cpp`**

```cpp
#include "request_arena.hpp"

// Header-only logic; this TU exists so the leaf is a STATIC archive with a
// stable object, matching the per-leaf convention.
namespace demiplane::http {
    namespace {
        [[maybe_unused]] constexpr int request_arena_tu_anchor = 0;
    }
}  // namespace demiplane::http
```

- [ ] **Step 6: Create `components/http/connection/request_arena/CMakeLists.txt`**

```cmake
##############################################################################
# Http Connection — RequestArena (per-connection monotonic arena)
##############################################################################
add_library(${DMP_HTTP}.Connection.RequestArena STATIC request_arena.cpp)

target_include_directories(${DMP_HTTP}.Connection.RequestArena PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
```

- [ ] **Step 7: Wire into the aggregate** — in `components/http/connection/CMakeLists.txt`, add
  `add_subdirectory(request_arena)` above the aggregate, and link it:

```cmake
add_subdirectory(request_arena)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Connection INTERFACE)

target_link_libraries(${DMP_HTTP}.Connection INTERFACE
        ${DMP_HTTP}.Connection.RequestArena
)
##############################################################################
```

- [ ] **Step 8: Configure + build + run — expect pass**

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Connection -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Connection 2>&1 | tail -10
```

- [ ] **Step 9: Commit**

```bash
git add components/http/connection/request_arena components/http/connection/CMakeLists.txt \
        tests/unit_tests/http/connection/test_request_arena.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/connection): RequestArena — per-connection monotonic block

One heap block per connection, reused across keep-alive requests via
reset() (spec §6.1). Non-movable (holds monotonic_buffer_resource)."
```

---

## Task 4: `Connection` / `StreamConnection` concepts

**Files:**

- Create: `components/http/connection/concepts/connection_concepts.hpp`
- Create: `components/http/connection/concepts/CMakeLists.txt`
- Create: `tests/unit_tests/http/connection/test_connection_concepts.cpp`
- Modify: `components/http/connection/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** Spec §6.1: the duck-typed contract every connection satisfies. `Connection` covers the arena + lifecycle +
metadata surface; `StreamConnection` refines it with a Beast-compatible `stream()`. No virtual base — drivers template
on the connection type.

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/connection/test_connection_concepts.cpp`

```cpp
#include <chrono>
#include <memory_resource>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <gtest/gtest.h>

#include <connection_concepts.hpp>
#include <http_enums.hpp>

using namespace demiplane::http;

namespace {
    // Minimal in-line model that satisfies StreamConnection. Proves the concept
    // is satisfiable with a plausible shape (TcpConnection is checked in Task 14,
    // TestConnection in Task 9).
    struct FakeStream {};

    struct ModelConn {
        using stream_type = FakeStream;
        FakeStream s;

        std::pmr::polymorphic_allocator<> arena_alloc() { return {}; }
        void reset_request_arena() {}
        void expires_after(std::chrono::milliseconds) {}
        boost::asio::awaitable<void> async_close() { co_return; }
        boost::asio::cancellation_slot cancel_slot() { return {}; }
        boost::asio::ip::address remote_address() const { return {}; }
        static Protocol negotiated_protocol() { return Protocol::http1; }
        static bool is_secure() { return false; }
        stream_type& stream() { return s; }
    };

    struct NoStream {  // satisfies Connection but not StreamConnection
        std::pmr::polymorphic_allocator<> arena_alloc() { return {}; }
        void reset_request_arena() {}
        void expires_after(std::chrono::milliseconds) {}
        boost::asio::awaitable<void> async_close() { co_return; }
        boost::asio::cancellation_slot cancel_slot() { return {}; }
        boost::asio::ip::address remote_address() const { return {}; }
        static Protocol negotiated_protocol() { return Protocol::http1; }
        static bool is_secure() { return false; }
    };
}  // namespace

static_assert(Connection<ModelConn>);
static_assert(StreamConnection<ModelConn>);
static_assert(Connection<NoStream>);
static_assert(!StreamConnection<NoStream>);

TEST(ConnectionConceptsTest, ModelsAreCheckedAtCompileTime) {
    SUCCEED();  // the static_asserts above are the test
}
```

- [ ] **Step 2: Register the test source** — add `connection/test_connection_concepts.cpp` to the `Http.Connection`
  source list in `tests/unit_tests/http/CMakeLists.txt`.

- [ ] **Step 3: Build — expect failure** (`connection_concepts.hpp` missing).

- [ ] **Step 4: Create `components/http/connection/concepts/connection_concepts.hpp`**

```cpp
#pragma once

#include <chrono>
#include <concepts>
#include <memory_resource>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>

#include <http_enums.hpp>

namespace demiplane::http {

    /**
     * @brief One peer's byte stream + lifecycle, duck-typed (spec §6.1).
     *
     * Concrete connections (TcpConnection, the test fake, QuicConnection later)
     * are value types composing a RequestArena + per-transport state. No virtual
     * base: drivers template their serve() on the connection type, so the stream
     * type is chosen at compile time — no dynamic_cast, no per-byte vcall.
     */
    template <typename T>
    concept Connection = requires(T& t, std::chrono::milliseconds ms) {
        { t.arena_alloc() } -> std::same_as<std::pmr::polymorphic_allocator<>>;
        { t.reset_request_arena() } -> std::same_as<void>;
        { t.expires_after(ms) } -> std::same_as<void>;
        { t.async_close() } -> std::same_as<boost::asio::awaitable<void>>;
        { t.cancel_slot() } -> std::same_as<boost::asio::cancellation_slot>;
        { t.remote_address() } -> std::same_as<boost::asio::ip::address>;
        { t.negotiated_protocol() } -> std::same_as<Protocol>;
        { t.is_secure() } -> std::same_as<bool>;
    };

    /// A Connection that exposes a Beast-compatible byte stream. Http11Driver
    /// requires this (it drives async_read/async_write on stream()).
    template <typename T>
    concept StreamConnection = Connection<T> && requires(T& t) {
        typename T::stream_type;
        { t.stream() } -> std::same_as<typename T::stream_type&>;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/connection/concepts/CMakeLists.txt`**

```cmake
##############################################################################
# Http Connection — Connection / StreamConnection concepts (header-only)
##############################################################################
add_library(${DMP_HTTP}.Connection.Concepts INTERFACE connection_concepts.hpp)

target_include_directories(${DMP_HTTP}.Connection.Concepts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Connection.Concepts INTERFACE
        ${DMP_HTTP}.Types.Enums
        Boost::asio
)
##############################################################################
```

- [ ] **Step 6: Wire into the aggregate** — `add_subdirectory(concepts)` + `${DMP_HTTP}.Connection.Concepts` in the
  `components/http/connection/CMakeLists.txt` aggregate link list.

- [ ] **Step 7: Build + run — expect pass**

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Connection -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Connection 2>&1 | tail -5
```

- [ ] **Step 8: Commit**

```bash
git add components/http/connection/concepts components/http/connection/CMakeLists.txt \
        tests/unit_tests/http/connection/test_connection_concepts.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/connection): Connection / StreamConnection concepts

Duck-typed connection contract (spec §6.1) — arena + lifecycle + metadata,
refined by StreamConnection's Beast-compatible stream(). No virtual base."
```

---

## Task 5: `Body::beast_view` — the zero-copy request-body payload

**Files:**

- Modify: `components/http/types/body/body.hpp`
- Modify: `components/http/types/body/body.cpp`
- Modify: `tests/unit_tests/http/types/test_body.cpp`

**Goal:** Spec §5.2's `BeastRequestBody` — a **non-owning** `Body` payload viewing bytes the connection/parser owns
(the request arena, per **D1**). Lives in the Types.Body leaf because `Body`'s payload constructor is private. Mirrors
`OwnedBufferPayload` but borrows instead of owning; `sizeof` is well under the 48-byte SBO budget (`std::span` + bool).

- [ ] **Step 1: Write the failing test** — append to `tests/unit_tests/http/types/test_body.cpp` (add
  `#include <cstddef>` and `#include <span>` to its includes if absent):

```cpp
TEST(BodyTest, BeastViewBorrowsExternalBytes) {
    const std::string owner = "borrowed payload";
    Body b = Body::beast_view(
        std::span<const std::byte>{reinterpret_cast<const std::byte*>(owner.data()), owner.size()});

    ASSERT_TRUE(b.buffered_view().has_value());
    EXPECT_EQ(*b.buffered_view(), "borrowed payload");
    EXPECT_EQ(b.buffered_view()->data(), owner.data());  // zero-copy: same address
    EXPECT_EQ(b.size_hint().value_or(0), owner.size());
}

TEST(BodyTest, BeastViewYieldsOneChunkThenEnd) {
    const std::string owner = "abc";
    Body b = Body::beast_view(
        std::span<const std::byte>{reinterpret_cast<const std::byte*>(owner.data()), owner.size()});
    auto first = run_awaitable(b.read_chunk());
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->size(), 3u);
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
}

TEST(BodyTest, BeastViewEmptyYieldsNoChunks) {
    Body b = Body::beast_view(std::span<const std::byte>{});
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
    ASSERT_TRUE(b.buffered_view().has_value());
    EXPECT_EQ(*b.buffered_view(), "");
}
```

- [ ] **Step 2: Build — expect failure** (`Body::beast_view` undeclared):

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -10
```

- [ ] **Step 3: Declare the factory.** In `components/http/types/body/body.hpp`, add `#include <cstddef>` and
  `#include <span>` to the includes if not present, and add this static factory next to `owned`:

```cpp
        static Body empty() noexcept {
            return Body{};
        }
        static Body owned(std::string bytes);  // OwnedBufferBody

        /// Non-owning view over bytes the caller keeps alive (the request arena
        /// owns the parsed h1 body — spec §5.2 BeastRequestBody / PR3 D1). The
        /// span must outlive this Body (the driver keeps the parser/message
        /// alive across dispatch + write, then resets the arena).
        static Body beast_view(std::span<const std::byte> bytes);
```

- [ ] **Step 4: Define the payload + factory.** In `components/http/types/body/body.cpp`, add the payload to the
  anonymous namespace alongside `OwnedBufferPayload`:

```cpp
        struct BeastRequestBody {
            std::span<const std::byte> bytes;
            bool consumed = false;
            boost::asio::awaitable<std::optional<std::span<const std::byte>>> read_chunk() {
                if (consumed || bytes.empty()) {
                    consumed = true;
                    co_return std::nullopt;
                }
                consumed = true;
                co_return bytes;
            }
            [[nodiscard]] std::optional<std::size_t> size_hint() const {
                return bytes.size();
            }
            [[nodiscard]] std::optional<std::string_view> buffered_view() const {
                return std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
            }
        };
```

and the factory next to `Body::owned`:

```cpp
    Body Body::beast_view(std::span<const std::byte> bytes) {
        return Body{
            emplace_t{},
            std::in_place_type<BeastRequestBody>, BeastRequestBody{bytes, false}
        };
    }
```

> `buffered_view()` on an empty span yields `string_view{nullptr, 0}` which compares equal to `""` — matching the
> EmptyBody contract the test asserts. `sizeof(BeastRequestBody)` ≈ 24 ≤ 48 (the `static_assert` in the `emplace_t`
> ctor enforces the budget).

- [ ] **Step 5: Build + run — expect pass** (old Body tests must still pass):

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Types 2>&1 | tail -5
```

- [ ] **Step 6: Commit**

```bash
git add components/http/types/body/body.hpp components/http/types/body/body.cpp \
        tests/unit_tests/http/types/test_body.cpp
git commit -m "feat(http/types): Body::beast_view — zero-copy request-body payload

Non-owning BeastRequestBody payload viewing arena-owned parsed bytes (spec
§5.2). The h1 driver (PR3) wraps the request parser's pmr body in it."
```

---

## Task 6: `HttpDriver` concept

**Files:**

- Create: `components/http/drivers/http_driver/http_driver.hpp`
- Create: `components/http/drivers/http_driver/CMakeLists.txt`
- Create: `tests/unit_tests/http/drivers/test_driver_scaffolds.cpp` (concept section only; scaffolds added in Task 15)
- Modify: `components/http/drivers/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** Spec §6.2: a driver advertises a `Protocol` id and its ALPN strings statically; `serve()` is templated on the
connection type and checked at the listener call site (PR 4), so it is *not* part of the concept.

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/drivers/test_driver_scaffolds.cpp`

```cpp
#include <span>
#include <string_view>

#include <gtest/gtest.h>

#include <http_driver.hpp>
#include <http_enums.hpp>

using namespace demiplane::http;

namespace {
    struct GoodDriver {
        static constexpr Protocol id() {
            return Protocol::http1;
        }
        static constexpr std::span<const std::string_view> accepted_alpns() {
            static constexpr std::string_view kAlpns[] = {"http/1.1"};
            return kAlpns;
        }
    };
    struct MissingAlpns {
        static constexpr Protocol id() {
            return Protocol::http1;
        }
    };
}  // namespace

static_assert(HttpDriver<GoodDriver>);
static_assert(!HttpDriver<MissingAlpns>);

TEST(HttpDriverConceptTest, AdvertisesIdAndAlpns) {
    EXPECT_EQ(GoodDriver::id(), Protocol::http1);
    ASSERT_EQ(GoodDriver::accepted_alpns().size(), 1u);
    EXPECT_EQ(GoodDriver::accepted_alpns()[0], "http/1.1");
}
```

- [ ] **Step 2: Wire the test target** — append to `tests/unit_tests/http/CMakeLists.txt`:

```cmake
##############################################################################
# Test HTTP Drivers layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Drivers
        drivers/test_driver_scaffolds.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Drivers
        PRIVATE
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Types
        Boost::beast
        ${TEST_LIBS}
)
##############################################################################
```

(The source list grows in Tasks 8, 9, 15, 16; check before appending.)

- [ ] **Step 3: Build — expect failure** (`http_driver.hpp` missing).

- [ ] **Step 4: Create `components/http/drivers/http_driver/http_driver.hpp`**

```cpp
#pragma once

#include <concepts>
#include <span>
#include <string_view>

#include <http_enums.hpp>

namespace demiplane::http {

    /**
     * @brief What every protocol driver advertises statically (spec §6.2).
     *
     * serve(Connection&, Router&) is intentionally NOT in the concept: it is
     * templated on the connection type and checked where the listener pairs a
     * driver with a connection (PR 4). The build/buy line is inside serve().
     */
    template <typename T>
    concept HttpDriver = requires {
        { T::id() } -> std::same_as<Protocol>;
        { T::accepted_alpns() } -> std::same_as<std::span<const std::string_view>>;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/drivers/http_driver/CMakeLists.txt`**

```cmake
##############################################################################
# Http Drivers — HttpDriver concept (header-only)
##############################################################################
add_library(${DMP_HTTP}.Drivers.Concept INTERFACE http_driver.hpp)

target_include_directories(${DMP_HTTP}.Drivers.Concept INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Drivers.Concept INTERFACE
        ${DMP_HTTP}.Types.Enums
)
##############################################################################
```

- [ ] **Step 6: Wire into the aggregate** — `add_subdirectory(http_driver)` + `${DMP_HTTP}.Drivers.Concept` in the
  `components/http/drivers/CMakeLists.txt` aggregate.

- [ ] **Step 7: Build + run — expect pass**

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Drivers -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Drivers 2>&1 | tail -5
```

- [ ] **Step 8: Commit**

```bash
git add components/http/drivers/http_driver components/http/drivers/CMakeLists.txt \
        tests/unit_tests/http/drivers/test_driver_scaffolds.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/drivers): HttpDriver concept (static id + accepted_alpns)

serve() is deliberately out of the concept — templated on the connection
type, checked at the listener call site (PR 4)."
```

---

## Task 7: `Http11Config`

**Files:**

- Create: `components/http/drivers/http11/http11_config.hpp`

**Goal:** Spec §6.3's config struct — limits + per-phase timeouts. Plain struct (the `ConfigInterface`-backed
`ServerConfig` that *feeds* these values is PR 6); no test of its own (defaults exercised by the driver tests).

- [ ] **Step 1: Create `components/http/drivers/http11/http11_config.hpp`**

```cpp
#pragma once

#include <chrono>
#include <cstddef>

namespace demiplane::http {

    /// Per-driver HTTP/1.1 limits + phase timeouts (spec §6.3). ServerConfig
    /// (PR 6) constructs these from loaded config; for now callers build them
    /// directly.
    struct Http11Config {
        std::size_t max_header_bytes = 16 * 1024;
        std::size_t max_body_bytes   = 16 * 1024 * 1024;

        std::chrono::milliseconds header_timeout = std::chrono::seconds{10};
        std::chrono::milliseconds body_timeout   = std::chrono::seconds{30};
        std::chrono::milliseconds idle_timeout   = std::chrono::seconds{60};
    };

}  // namespace demiplane::http
```

- [ ] **Step 2: Commit** (the leaf CMakeLists + driver land in Task 8; this header is included from there)

```bash
git add components/http/drivers/http11/http11_config.hpp
git commit -m "feat(http/drivers): Http11Config — h1 limits + per-phase timeouts"
```

---

## Task 8: Driver helpers — `build_request_context` / `make_beast_response` / `stamp_common_headers`

**Files:**

- Create: `components/http/drivers/http11/http11_driver.hpp` (type aliases + helper decls + class skeleton)
- Create: `components/http/drivers/http11/http11_driver.cpp` (the three non-template helpers)
- Create: `components/http/drivers/http11/CMakeLists.txt`
- Create: `tests/unit_tests/http/drivers/test_driver_helpers.cpp`
- Modify: `components/http/drivers/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** The three pure-ish, non-template translation helpers, unit-tested **without a stream** (true TDD).
`build_request_context` turns a parsed Beast request into a `RequestContext` (zero-copy body view, per **D1**);
`make_beast_response` turns our `Response` into a zero-copy `response<buffer_body>` (per the validated write spike);
`stamp_common_headers` adds `Date`/`Server` if absent. The template `serve()` is added in Task 9.

> **Implementer note — the split-allocator parse is load-bearing and was spike-validated.** `BeastBody` uses a pmr
> *body* allocator (arena) but `std::allocator` *fields* (so `req.base()` stays `http::fields`, compatible with the
> landed `Headers::view_of_beast`). Do not "unify" the allocators — a pmr fields allocator changes `req.base()`'s type
> and breaks the Headers view (D1/D2).

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/drivers/test_driver_helpers.cpp`

```cpp
#include <array>
#include <memory_resource>
#include <string>
#include <tuple>

#include <boost/beast/http.hpp>
#include <gtest/gtest.h>

#include <http11_driver.hpp>
#include <response.hpp>
#include <response_factory.hpp>

using namespace demiplane::http;
namespace http = boost::beast::http;

namespace {
    // Build a parsed Beast request of the driver's exact type, body in `arena`.
    detail::Http11Request make_parsed(std::pmr::polymorphic_allocator<> arena, http::verb verb,
                                      std::string target, std::string body) {
        std::pmr::polymorphic_allocator<char> body_alloc{arena.resource()};
        detail::Http11Request req{std::piecewise_construct, std::forward_as_tuple(body_alloc),
                                  std::forward_as_tuple()};
        req.method(verb);
        req.target(target);
        req.version(11);
        req.body().assign(body.begin(), body.end());
        req.set(http::field::content_type, "application/json");
        return req;
    }
}  // namespace

TEST(DriverHelpersTest, BuildRequestContextMapsMethodTargetVersionBody) {
    std::array<std::byte, 8192> block{};
    std::pmr::monotonic_buffer_resource res{block.data(), block.size()};
    std::pmr::polymorphic_allocator<> arena{&res};

    detail::Http11Request req = make_parsed(arena, http::verb::post, "/users/42?q=foo", "hello");
    RequestContext ctx = detail::build_request_context(req, arena);

    EXPECT_EQ(ctx.method(), HttpMethod::post);
    EXPECT_EQ(ctx.target(), "/users/42?q=foo");
    EXPECT_EQ(ctx.path(), "/users/42");
    EXPECT_EQ(ctx.version(), HttpVersion::http_1_1);
    ASSERT_TRUE(ctx.body().buffered_view().has_value());
    EXPECT_EQ(*ctx.body().buffered_view(), "hello");
    EXPECT_TRUE(ctx.is_json());  // Content-Type viewed through Headers::view_of_beast
}

TEST(DriverHelpersTest, MakeBeastResponseTranslatesStatusHeadersBodyZeroCopy) {
    Response r;  // default new_delete (cold-path ok for this pure test)
    r.status     = HttpStatus::created;
    r.keep_alive = false;
    r.set_header("Content-Type", "application/json");
    r.add_header("Set-Cookie", "a=1");
    r.add_header("Set-Cookie", "b=2");
    r = std::move(r).with_body("{\"id\":1}");

    auto msg = detail::make_beast_response(r);
    EXPECT_EQ(msg.result_int(), 201u);
    EXPECT_EQ(msg.version(), 11u);
    EXPECT_EQ(std::string(msg[http::field::content_type]), "application/json");
    auto cookies = msg.equal_range("Set-Cookie");
    EXPECT_EQ(std::distance(cookies.first, cookies.second), 2);
    // buffer_body points AT the Response's body bytes — no copy.
    EXPECT_EQ(static_cast<const char*>(msg.body().data), r.body.buffered_view()->data());
    EXPECT_EQ(msg.body().size, 8u);
    EXPECT_FALSE(msg.keep_alive());
}

TEST(DriverHelpersTest, MakeBeastResponseEmptyBody) {
    Response r;
    r.status = HttpStatus::no_content;
    auto msg = detail::make_beast_response(r);
    EXPECT_EQ(msg.result_int(), 204u);
    EXPECT_EQ(msg.body().size, 0u);
}
```

- [ ] **Step 2: Register the test source** — add `drivers/test_driver_helpers.cpp` to the `Http.Drivers` source list.

- [ ] **Step 3: Build — expect failure** (`http11_driver.hpp` missing).

- [ ] **Step 4: Create `components/http/drivers/http11/http11_driver.hpp`** (aliases + helper decls + class skeleton;
  the template `serve()` body is filled in Task 9 — here it is declared and defined as a minimal stub so the TU
  compiles and the helper tests link)

```cpp
#pragma once

#include <memory>
#include <memory_resource>
#include <span>
#include <string_view>
#include <tuple>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>

#include <body.hpp>
#include <connection_concepts.hpp>
#include <headers.hpp>
#include <http_enums.hpp>
#include <request.hpp>
#include <request_context.hpp>
#include <response.hpp>
#include <response_factory.hpp>
#include <router.hpp>

#include "http11_config.hpp"

namespace demiplane::http {

    namespace detail {
        // Split allocators (PR3 D1, spike-validated): pmr BODY allocator (arena),
        // std::allocator FIELDS (so req.base() is http::fields, compatible with
        // the landed Headers::view_of_beast).
        using Http11Body = boost::beast::http::basic_string_body<
            char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>;
        using Http11Request = boost::beast::http::request<Http11Body>;
        using Http11Parser  = boost::beast::http::request_parser<Http11Body, std::allocator<char>>;

        /// Wrap a parsed Beast request as a RequestContext. The request body is
        /// a non-owning view over the parser's arena-backed bytes — the parser
        /// MUST outlive the context (the driver keeps it alive across dispatch +
        /// write, then resets the arena).
        RequestContext build_request_context(Http11Request& req,
                                              std::pmr::polymorphic_allocator<> arena);

        /// Translate our Response into a zero-copy buffer_body message whose
        /// body bytes point at `resp.body` — `resp` MUST outlive the returned
        /// message and its write.
        boost::beast::http::response<boost::beast::http::buffer_body>
        make_beast_response(Response& resp);
    }  // namespace detail

    /**
     * @brief HTTP/1.1 driver over Boost.Beast (spec §6.3).
     *
     * One keep-alive session loop per connection: parse (body into the request
     * arena), build a RequestContext, dispatch through the Router, stamp
     * Date/Server, write the Response zero-copy. The bug-fix battery lands here
     * (body/header limits, per-phase timeouts, handler-exception → 500,
     * cancellation-aware I/O).
     */
    class Http11Driver {
    public:
        explicit Http11Driver(Http11Config cfg) noexcept
            : cfg_{cfg} {
        }

        static constexpr Protocol id() noexcept {
            return Protocol::http1;
        }
        static constexpr std::span<const std::string_view> accepted_alpns() noexcept {
            static constexpr std::string_view kAlpns[] = {"http/1.1"};
            return kAlpns;
        }

        template <StreamConnection ConnT>
        boost::asio::awaitable<void> serve(ConnT& conn, Router& router);

    private:
        static void stamp_common_headers(Response& resp);

        template <typename Stream>
        static boost::asio::awaitable<boost::beast::error_code>
        write_response(Stream& stream, Response& resp, boost::asio::cancellation_slot slot);

        Http11Config cfg_;
        // No SCROLL_COMPONENT_PREFIX / COMPONENT_LOG_* here: the h1 driver does
        // not log in v1, so it carries no Scroll dependency in its public header
        // (avoids a PUBLIC Scroll link just to satisfy the macro in consumers).
        // The h2/h3 scaffolds DO log, and link Scroll INTERFACE accordingly.
    };

    // ── serve() / write_response() definitions land in Task 9 (see the
    //    include of "http11_serve.inl" appended there) ─────────────────────

}  // namespace demiplane::http
```

> Note: `serve()` and `write_response()` are *declared* here; their definitions are added in Task 9 via an appended
> `#include "http11_serve.inl"` at the bottom of this header. Until then the helper tests link because they call only
> the non-template helpers (defined next). The class is still usable as an `HttpDriver` (its static `id()`/
> `accepted_alpns()` satisfy the concept).

- [ ] **Step 5: Create `components/http/drivers/http11/http11_driver.cpp`**

```cpp
#include "http11_driver.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string_view>
#include <utility>

namespace demiplane::http {

    RequestContext detail::build_request_context(Http11Request& req,
                                                 std::pmr::polymorphic_allocator<> arena) {
        Request request{Headers::view_of_beast(req.base())};
        request.method  = method_from_beast(req.method());
        request.version = version_from_beast(req.version());
        request.target  = std::string_view{req.target().data(), req.target().size()};

        auto& body   = req.body();
        request.body = Body::beast_view(std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(body.data()), body.size()});

        return RequestContext{std::move(request), arena};
    }

    boost::beast::http::response<boost::beast::http::buffer_body>
    detail::make_beast_response(Response& resp) {
        namespace http = boost::beast::http;

        http::response<http::buffer_body> msg;
        msg.result(static_cast<unsigned>(resp.status));
        msg.version(version_to_beast(resp.version));
        for (const auto& [name, value] : resp.headers)
            msg.insert(name, value);

        // v1 writes only non-streaming responses, so buffered_view() is always
        // present (Empty/Owned/BeastView). A future streaming body returns
        // nullopt here — assert rather than silently truncate to an empty body.
        assert(resp.body.buffered_view().has_value()
               && "make_beast_response: streaming response bodies not supported yet");
        const std::string_view bytes = resp.body.buffered_view().value_or(std::string_view{});
        msg.body().data = const_cast<char*>(bytes.data());
        msg.body().size = bytes.size();
        msg.body().more = false;
        msg.content_length(bytes.size());  // set() semantics — overrides any Content-Length header
        msg.keep_alive(resp.keep_alive);   // sets the Connection header
        return msg;
    }

    void Http11Driver::stamp_common_headers(Response& resp) {
        if (!resp.headers.contains("Date")) {
            // IMF-fixdate (RFC 9110 §5.6.7) uses FIXED English day/month names.
            // Build them from tables rather than strftime("%a"/"%b"), which honor
            // the process's global LC_TIME locale — a reusable server must not
            // emit a locale-dependent (non-compliant) Date header.
            static constexpr const char* days[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            static constexpr const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            const std::time_t t =
                std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm tm{};
            ::gmtime_r(&t, &tm);
            char buf[32];
            const int n = std::snprintf(buf, sizeof buf, "%s, %02d %s %04d %02d:%02d:%02d GMT",
                                        days[tm.tm_wday], tm.tm_mday, months[tm.tm_mon],
                                        tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
            if (n > 0)
                resp.set_header("Date", std::string_view{buf, static_cast<std::size_t>(n)});
        }
        if (!resp.headers.contains("Server"))
            resp.set_header("Server", "Demiplane");
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Create `components/http/drivers/http11/CMakeLists.txt`**

```cmake
##############################################################################
# Http Drivers — Http11Driver (real HTTP/1.1 over Boost.Beast)
##############################################################################
add_library(${DMP_HTTP}.Drivers.Http11 STATIC http11_driver.cpp)

target_include_directories(${DMP_HTTP}.Drivers.Http11 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Drivers.Http11
        PUBLIC
        ${DMP_HTTP}.Types
        ${DMP_HTTP}.Routing.Router
        ${DMP_HTTP}.Connection.Concepts
        Boost::beast
        Boost::asio
)
##############################################################################
```

- [ ] **Step 7: Wire into the aggregate** — `add_subdirectory(http11)` + `${DMP_HTTP}.Drivers.Http11` in
  `components/http/drivers/CMakeLists.txt`.

- [ ] **Step 8: Build + run — expect pass**

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Drivers -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Drivers 2>&1 | tail -5
```

- [ ] **Step 9: Commit**

```bash
git add components/http/drivers/http11 components/http/drivers/CMakeLists.txt \
        tests/unit_tests/http/drivers/test_driver_helpers.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/drivers): h1 translation helpers (request_context, response, stamp)

build_request_context wraps the parser's arena body zero-copy (D1);
make_beast_response writes our Response through buffer_body zero-copy;
stamp_common_headers adds Date/Server. serve() lands next."
```

---

## Task 9: `Http11Driver::serve()` + the `TestConnection` harness + happy-path GET

**Files:**

- Create: `components/http/drivers/http11/http11_serve.inl`
- Modify: `components/http/drivers/http11/http11_driver.hpp` (append the `#include`)
- Create: `tests/unit_tests/http/drivers/driver_test_utils.hpp`
- Create: `tests/unit_tests/http/drivers/test_http11_driver.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** The complete keep-alive session loop (spec §6.3) — written **once, in full**, including the error branches
(413/400/500) and stamping, because `serve()` is one cohesive coroutine that does not bisect cleanly. Tasks 10–13 then
*expand wire-test coverage* against it (a passing test there confirms correctness; a failure points at a real bug to
fix). This task lands the harness (`TestConnection` over `beast::test::stream`) and the first wire test: a GET round
trip.

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/drivers/driver_test_utils.hpp` first (the harness)

```cpp
#pragma once

#include <chrono>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>

#include <request_arena.hpp>
#include <router.hpp>

namespace http_driver_test {

    /// StreamConnection over an in-memory beast::test::stream — no kernel
    /// sockets. expires_after is a no-op (test::stream has no timer; timeout
    /// BEHAVIOR is PR 4, D7).
    class TestConnection {
    public:
        using stream_type = boost::beast::test::stream;

        explicit TestConnection(boost::asio::io_context& ioc, std::size_t arena_size = 8192)
            : stream_{ioc.get_executor()}, arena_{arena_size} {
        }

        stream_type& stream() noexcept {
            return stream_;
        }
        std::pmr::polymorphic_allocator<> arena_alloc() noexcept {
            return arena_.allocator();
        }
        void reset_request_arena() {
            arena_.reset();
        }
        void expires_after(std::chrono::milliseconds) noexcept {}
        boost::asio::awaitable<void> async_close() {
            stream_.close();
            co_return;
        }
        boost::asio::cancellation_slot cancel_slot() noexcept {
            return signal_.slot();
        }
        boost::asio::ip::address remote_address() const {
            return boost::asio::ip::make_address("127.0.0.1");
        }
        static Protocol negotiated_protocol() noexcept {
            return demiplane::http::Protocol::http1;
        }
        static bool is_secure() noexcept {
            return false;
        }

    private:
        stream_type stream_;
        demiplane::http::RequestArena arena_;
        boost::asio::cancellation_signal signal_;
    };

    static_assert(demiplane::http::StreamConnection<TestConnection>);

    using ParsedResponse = boost::beast::http::response<boost::beast::http::string_body>;

    /// Drive `driver.serve(conn, router)` against an in-memory peer that writes
    /// `requests` then reads `expected` responses. Returns the parsed responses.
    template <typename Driver>
    std::vector<ParsedResponse> exchange(Driver& driver, demiplane::http::Router& router,
                                         std::vector<std::string> requests, int expected) {
        namespace asio  = boost::asio;
        namespace beast = boost::beast;
        namespace http  = boost::beast::http;

        asio::io_context ioc;
        TestConnection conn{ioc};
        beast::test::stream client{ioc.get_executor()};
        conn.stream().connect(client);

        std::vector<ParsedResponse> responses;

        asio::co_spawn(ioc, driver.serve(conn, router), asio::detached);
        asio::co_spawn(
            ioc,
            [&]() -> asio::awaitable<void> {
                for (const auto& req : requests)
                    co_await asio::async_write(client, asio::buffer(req), asio::use_awaitable);
                beast::flat_buffer buffer;
                beast::error_code ec;
                for (int i = 0; i < expected; ++i) {
                    ParsedResponse res;
                    co_await http::async_read(client, buffer, res,
                                              asio::redirect_error(asio::use_awaitable, ec));
                    if (ec)
                        break;
                    responses.push_back(std::move(res));
                }
                client.close();
                co_return;
            },
            asio::detached);

        ioc.run();
        return responses;
    }

}  // namespace http_driver_test
```

then `tests/unit_tests/http/drivers/test_http11_driver.cpp`:

```cpp
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <controller.hpp>
#include <group.hpp>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <route_registry.hpp>
#include <router.hpp>

#include "driver_test_utils.hpp"

using namespace demiplane::http;
using http_driver_test::exchange;
using http_driver_test::ParsedResponse;

namespace {

    class ApiController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &ApiController::hello);
            Get("/users/{id}", &ApiController::user);
            Post("/echo", &ApiController::echo);
            Get("/boom", &ApiController::boom);
        }

    private:
        AsyncResponse hello(RequestContext ctx) {
            co_return ctx.ok("hello world");
        }
        AsyncResponse user(RequestContext ctx) {
            co_return ctx.ok("user:" + std::to_string(ctx.path_param<int>("id").value_or(-1))
                             + " v=" + ctx.query_or<std::string>("v", "none"));
        }
        AsyncResponse echo(RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(1 << 20);
            if (!body)  // gears::Outcome: explicit operator bool (same as Router's `if (!resolved)`)
                co_return ctx.status(HttpStatus::payload_too_large, "too big");
            co_return ctx.json(std::move(body).value());  // .value(), not operator* — the landed API
        }
        AsyncResponse boom(RequestContext) {
            throw std::runtime_error{"handler exploded"};
        }
    };

    Http11Driver make_driver() {
        return Http11Driver{Http11Config{}};
    }

}  // namespace

class Http11DriverTest : public ::testing::Test {
protected:
    RouteRegistry registry_;
    std::vector<std::shared_ptr<HttpController>> controllers_;

    void SetUp() override {
        GroupBinding{registry_, controllers_, ""}.add_controller(std::make_shared<ApiController>());
        ASSERT_TRUE(registry_.freeze().empty());
    }

    Router router() {
        return Router{registry_};
    }
};

TEST_F(Http11DriverTest, GetRoundTrip) {
    auto driver = make_driver();
    auto r      = router();
    auto out = exchange(driver, r,
                        {"GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 200u);
    EXPECT_EQ(out[0].body(), "hello world");
}
```

- [ ] **Step 2: Register the test source** — add `drivers/test_http11_driver.cpp` to the `Http.Drivers` source list.

- [ ] **Step 3: Build — expect failure** (`serve()` is only declared; linker error / unimplemented).

- [ ] **Step 4: Create `components/http/drivers/http11/http11_serve.inl`** (the complete session loop)

```cpp
// Included at the bottom of http11_driver.hpp — definitions of the templated
// serve() and write_response(). Kept in a .inl so the header stays readable.

namespace demiplane::http {

    namespace {
        /// Beast "malformed but framed" parse errors: the bytes arrived and were
        /// structurally rejected, so the stream is still writable and the right
        /// answer is 400 — not a silently dropped connection. Transport errors
        /// (reset/timeout, cancellation, partial_message/EOF) are NOT here; those
        /// close. (Found by the final review — the original blanket `if (ec) break`
        /// dropped these, inconsistent with the header_limit→400 mapping.)
        inline bool is_malformed_request(boost::beast::error_code ec) noexcept {
            namespace he = boost::beast::http;
            return ec == he::error::bad_line_ending || ec == he::error::bad_method
                   || ec == he::error::bad_target || ec == he::error::bad_version
                   || ec == he::error::bad_field || ec == he::error::bad_value
                   || ec == he::error::bad_content_length
                   || ec == he::error::bad_transfer_encoding || ec == he::error::bad_chunk
                   || ec == he::error::bad_chunk_extension || ec == he::error::bad_obs_fold;
        }
    }  // namespace

    template <typename Stream>
    boost::asio::awaitable<boost::beast::error_code> Http11Driver::write_response(
        Stream& stream, Response& resp, boost::asio::cancellation_slot slot) {
        namespace asio = boost::asio;
        namespace http = boost::beast::http;

        auto msg = detail::make_beast_response(resp);
        boost::beast::error_code ec;
        co_await http::async_write(
            stream, msg,
            asio::bind_cancellation_slot(slot, asio::redirect_error(asio::use_awaitable, ec)));
        co_return ec;  // checked by serve()'s normal path; error paths break regardless
    }

    template <StreamConnection ConnT>
    boost::asio::awaitable<void> Http11Driver::serve(ConnT& conn, Router& router) {
        namespace asio = boost::asio;
        namespace http = boost::beast::http;

        auto& stream = conn.stream();
        boost::beast::flat_buffer buffer;

        while (true) {
            conn.reset_request_arena();
            std::pmr::polymorphic_allocator<char> body_alloc{conn.arena_alloc().resource()};

            detail::Http11Parser parser{std::piecewise_construct, std::forward_as_tuple(body_alloc),
                                        std::forward_as_tuple()};
            parser.header_limit(cfg_.max_header_bytes);
            parser.body_limit(cfg_.max_body_bytes);

            boost::beast::error_code ec;

            // ── Phase 1: header (also the idle wait between keep-alive requests)
            conn.expires_after(cfg_.header_timeout);
            co_await http::async_read_header(
                stream, buffer, parser,
                asio::bind_cancellation_slot(conn.cancel_slot(),
                                             asio::redirect_error(asio::use_awaitable, ec)));
            if (ec == http::error::end_of_stream)
                break;  // client closed cleanly
            if (ec == http::error::header_limit) {
                Response r = ResponseFactory::bad_request("Request Header Fields Too Large");
                r.keep_alive = false;
                stamp_common_headers(r);
                co_await write_response(stream, r, conn.cancel_slot());
                break;
            }
            // Beast checks Content-Length against body_limit eagerly at header-parse
            // time, so an oversize declared body surfaces HERE, not in phase 2.
            if (ec == http::error::body_limit) {
                Response r = ResponseFactory::payload_too_large();
                r.keep_alive = false;
                stamp_common_headers(r);
                co_await write_response(stream, r, conn.cancel_slot());
                break;
            }
            if (ec) {
                if (is_malformed_request(ec)) {  // malformed-but-framed → 400
                    Response r   = ResponseFactory::bad_request("Bad Request");
                    r.keep_alive = false;
                    stamp_common_headers(r);
                    co_await write_response(stream, r, conn.cancel_slot());
                }
                break;  // transport error → just close
            }

            // ── Phase 2: body
            conn.expires_after(cfg_.body_timeout);
            co_await http::async_read(
                stream, buffer, parser,
                asio::bind_cancellation_slot(conn.cancel_slot(),
                                             asio::redirect_error(asio::use_awaitable, ec)));
            if (ec == http::error::body_limit) {
                Response r = ResponseFactory::payload_too_large();
                r.keep_alive = false;
                stamp_common_headers(r);
                co_await write_response(stream, r, conn.cancel_slot());
                break;
            }
            if (ec) {
                if (is_malformed_request(ec)) {  // malformed-but-framed → 400
                    Response r   = ResponseFactory::bad_request("Bad Request");
                    r.keep_alive = false;
                    stamp_common_headers(r);
                    co_await write_response(stream, r, conn.cancel_slot());
                }
                break;
            }

            // ── Dispatch
            auto& req                    = parser.get();
            const bool client_keep_alive = req.keep_alive();
            RequestContext ctx           = detail::build_request_context(req, conn.arena_alloc());

            Response response{conn.arena_alloc()};
            try {
                response = co_await router.dispatch(std::move(ctx));
            } catch (...) {
                response            = ResponseFactory::internal_error();
                response.keep_alive = false;
            }

            const bool keep_alive = response.keep_alive && client_keep_alive;
            response.keep_alive   = keep_alive;
            response.version      = HttpVersion::http_1_1;
            stamp_common_headers(response);

            const boost::beast::error_code write_ec =
                co_await write_response(stream, response, conn.cancel_slot());
            if (write_ec || !keep_alive)
                break;
        }
        co_await conn.async_close();
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Append the include** at the very bottom of `components/http/drivers/http11/http11_driver.hpp`,
  *after* the closing `}  // namespace demiplane::http`:

```cpp
#include "http11_serve.inl"
```

- [ ] **Step 6: Build + run — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Drivers -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -R Http.Drivers 2>&1 | tail -10
```

- [ ] **Step 7: Commit**

```bash
git add components/http/drivers/http11/http11_serve.inl components/http/drivers/http11/http11_driver.hpp \
        tests/unit_tests/http/drivers/driver_test_utils.hpp tests/unit_tests/http/drivers/test_http11_driver.cpp \
        tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/drivers): Http11Driver::serve() session loop + TestConnection harness

Complete keep-alive loop (spec §6.3): two-phase read, arena body, dispatch,
catch-all → 500, body/header-limit → 413/400, Date/Server stamp, zero-copy
write. Tested in-memory via beast::test::stream. Coverage expands next."
```

---

## Task 10: Wire tests — request body + path/query params

**Files:**

- Modify: `tests/unit_tests/http/drivers/test_http11_driver.cpp`

**Goal:** Prove the body arena integration (D1) and param decoding end-to-end against the complete driver. No driver
change expected (these pass against Task 9's serve); a failure indicates a real bug to fix in `serve`/helpers.

- [ ] **Step 1: Append tests** to `test_http11_driver.cpp`:

```cpp
TEST_F(Http11DriverTest, PathAndQueryParams) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(
        driver, r,
        {"GET /users/42?v=hi HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 200u);
    EXPECT_EQ(out[0].body(), "user:42 v=hi");
}

TEST_F(Http11DriverTest, PostBodyEchoedThroughArena) {
    auto driver = make_driver();
    auto r      = router();
    const std::string payload = R"({"name":"demiplane"})";
    std::string req = "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
                      "Content-Length: " + std::to_string(payload.size()) +
                      "\r\nConnection: close\r\n\r\n" + payload;
    auto out = exchange(driver, r, {req}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 200u);
    EXPECT_EQ(std::string(out[0][boost::beast::http::field::content_type]), "application/json");
    EXPECT_EQ(out[0].body(), payload);
}

TEST_F(Http11DriverTest, LargeBodyRoundTrips) {
    auto driver = make_driver();
    auto r      = router();
    const std::string payload(4096, 'Z');  // well past SSO — exercises the arena body block
    std::string req = "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: " +
                      std::to_string(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload;
    auto out = exchange(driver, r, {req}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].body(), payload);
}
```

- [ ] **Step 2: Build + run — expect pass**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Drivers -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Drivers 2>&1 | tail -5
```

- [ ] **Step 3: Commit**

```bash
git add tests/unit_tests/http/drivers/test_http11_driver.cpp
git commit -m "test(http/drivers): body arena integration + path/query params on the wire"
```

---

## Task 11: Wire tests — keep-alive loop + connection close

**Files:**

- Modify: `tests/unit_tests/http/drivers/test_http11_driver.cpp`

**Goal:** Two requests on one connection answered in order (arena reset between them, per §6.2); the no-keep-alive path
closes after one response.

- [ ] **Step 1: Append tests:**

```cpp
TEST_F(Http11DriverTest, KeepAliveServesTwoRequests) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(
        driver, r,
        {"GET /hello HTTP/1.1\r\nHost: x\r\n\r\n",                       // keep-alive (HTTP/1.1 default)
         "GET /users/7?v=q HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"},
        2);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].body(), "hello world");
    EXPECT_EQ(out[1].body(), "user:7 v=q");
}

TEST_F(Http11DriverTest, ConnectionCloseStopsAfterOne) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(
        driver, r, {"GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 2);
    // Only one response arrives; the driver closed after it (Connection: close).
    ASSERT_EQ(out.size(), 1u);
    EXPECT_FALSE(out[0].keep_alive());
}
```

- [ ] **Step 2: Build + run — expect pass; Step 3: Commit**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Drivers -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Drivers 2>&1 | tail -5
git add tests/unit_tests/http/drivers/test_http11_driver.cpp
git commit -m "test(http/drivers): keep-alive serves multiple requests; close stops the loop"
```

---

## Task 12: Wire tests — 404/405, handler exception → 500, body limit → 413, header limit → 400

**Files:**

- Modify: `tests/unit_tests/http/drivers/test_http11_driver.cpp`

**Goal:** The bug-fix battery on the wire (spec §6.3): routing misses already collapsed by the Router surface as
4xx; an escaping handler exception becomes a 500 (not a dropped TCP connection — the original bug); oversize body →
413; oversize headers → 400. These exercise serve()'s error branches written in Task 9.

- [ ] **Step 1: Append tests:**

```cpp
TEST_F(Http11DriverTest, UnknownPathIs404) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r,
                           {"GET /nope HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 404u);
}

TEST_F(Http11DriverTest, WrongVerbIs405WithAllow) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r,
                           {"DELETE /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 405u);
    EXPECT_NE(std::string(out[0]["Allow"]).find("GET"), std::string::npos);
}

TEST_F(Http11DriverTest, HandlerExceptionBecomes500) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r,
                           {"GET /boom HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);  // a RESPONSE, not a dropped connection (the original bug)
    EXPECT_EQ(out[0].result_int(), 500u);
}

TEST_F(Http11DriverTest, OversizeBodyIs413) {
    Http11Config cfg;
    cfg.max_body_bytes = 16;  // tiny limit
    Http11Driver driver{cfg};
    auto r = router();
    const std::string payload(64, 'A');  // exceeds 16
    std::string req = "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: " +
                      std::to_string(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload;
    auto out = exchange(driver, r, {req}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 413u);
}

TEST_F(Http11DriverTest, OversizeHeadersIs400) {
    Http11Config cfg;
    cfg.max_header_bytes = 64;  // tiny limit
    Http11Driver driver{cfg};
    auto r = router();
    std::string req = "GET /hello HTTP/1.1\r\nHost: x\r\nX-Big: " + std::string(256, 'h') +
                      "\r\nConnection: close\r\n\r\n";
    auto out = exchange(driver, r, {req}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 400u);
}
```

- [ ] **Step 2: Build + run — expect pass.** If a case fails, the driver's error branch is the bug — fix `serve()`,
  not the test:

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Drivers -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Drivers 2>&1 | tail -5
```

- [ ] **Step 3: Commit**

```bash
git add tests/unit_tests/http/drivers/test_http11_driver.cpp
git commit -m "test(http/drivers): 404/405, handler-exception → 500, 413, 400 on the wire

The exception case asserts a 500 RESPONSE — not the dropped connection the
original module produced."
```

---

## Task 13: Wire test — Date / Server stamping

**Files:**

- Modify: `tests/unit_tests/http/drivers/test_http11_driver.cpp`

**Goal:** Every response carries `Date` (IMF-fixdate) and `Server` exactly once, stamped uniformly by the driver
(spec §6.3) — not by the response factories.

- [ ] **Step 1: Append test:**

```cpp
TEST_F(Http11DriverTest, DateAndServerStamped) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r,
                           {"GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(std::string(out[0][boost::beast::http::field::server]), "Demiplane");
    const std::string date{out[0][boost::beast::http::field::date]};
    EXPECT_NE(date.find("GMT"), std::string::npos);
    // exactly one of each
    EXPECT_EQ(std::distance(out[0].equal_range("Date").first, out[0].equal_range("Date").second), 1);
    EXPECT_EQ(std::distance(out[0].equal_range("Server").first, out[0].equal_range("Server").second), 1);
}
```

- [ ] **Step 2: Build + run — expect pass; Step 3: Commit**

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Drivers -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Drivers 2>&1 | tail -5
git add tests/unit_tests/http/drivers/test_http11_driver.cpp
git commit -m "test(http/drivers): Date/Server stamped uniformly, exactly once per response"
```

---

## Task 14: `TcpConnection` — the real-socket connection

**Files:**

- Create: `components/http/connection/tcp_connection/tcp_connection.hpp`
- Create: `components/http/connection/tcp_connection/tcp_connection.cpp`
- Create: `components/http/connection/tcp_connection/CMakeLists.txt`
- Create: `tests/unit_tests/http/connection/test_tcp_connection.cpp`
- Modify: `components/http/connection/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** Spec §6.1's `TcpConnection` — `beast::tcp_stream` + `RequestArena` + a per-connection cancellation signal. It
satisfies `StreamConnection` and is the connection the `TcpListener` will spawn in PR 4. Here it is verified by concept
conformance + a construction smoke (its full wire exercise is PR 4 integration; the *driver* is already wire-tested via
`TestConnection`).

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/connection/test_tcp_connection.cpp`

```cpp
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <gtest/gtest.h>

#include <connection_concepts.hpp>
#include <tcp_connection.hpp>

using namespace demiplane::http;

static_assert(StreamConnection<TcpConnection>);

TEST(TcpConnectionTest, MetadataDefaults) {
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::socket sock{ioc};
    TcpConnection conn{std::move(sock)};
    EXPECT_EQ(conn.negotiated_protocol(), Protocol::http1);
    EXPECT_FALSE(conn.is_secure());
    EXPECT_NE(conn.arena_alloc().resource(), nullptr);
    conn.reset_request_arena();  // does not throw on a fresh arena
}
```

- [ ] **Step 2: Register the test source** — add `connection/test_tcp_connection.cpp` to the `Http.Connection` list,
  and add `Boost::beast` to that target's link libraries (tcp_stream lives in Beast):

```cmake
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Connection
        PRIVATE
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Types
        Boost::beast
        ${TEST_LIBS}
)
```

- [ ] **Step 3: Build — expect failure** (`tcp_connection.hpp` missing).

- [ ] **Step 4: Create `components/http/connection/tcp_connection/tcp_connection.hpp`**

```cpp
#pragma once

#include <chrono>
#include <cstddef>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <http_enums.hpp>
#include <request_arena.hpp>

namespace demiplane::http {

    /**
     * @brief Plain-TCP connection: beast::tcp_stream + per-connection arena +
     *        cancel signal (spec §6.1).
     *
     * Non-movable (composes the immovable RequestArena + cancellation_signal);
     * the TcpListener (PR 4) constructs it in place / on the heap per accept.
     */
    class TcpConnection {
    public:
        using stream_type = boost::beast::tcp_stream;

        explicit TcpConnection(boost::asio::ip::tcp::socket socket, std::size_t arena_size = 8192)
            : stream_{std::move(socket)}, arena_{arena_size} {
        }

        TcpConnection(const TcpConnection&)            = delete;
        TcpConnection& operator=(const TcpConnection&) = delete;
        TcpConnection(TcpConnection&&)                 = delete;
        TcpConnection& operator=(TcpConnection&&)      = delete;

        stream_type& stream() noexcept {
            return stream_;
        }
        std::pmr::polymorphic_allocator<> arena_alloc() noexcept {
            return arena_.allocator();
        }
        void reset_request_arena() {
            arena_.reset();
        }
        void expires_after(std::chrono::milliseconds ms) {
            stream_.expires_after(ms);
        }
        boost::asio::awaitable<void> async_close();
        boost::asio::cancellation_slot cancel_slot() noexcept {
            return signal_.slot();
        }
        boost::asio::ip::address remote_address() const {
            return stream_.socket().remote_endpoint().address();
        }
        static Protocol negotiated_protocol() noexcept {
            return Protocol::http1;
        }
        static bool is_secure() noexcept {
            return false;
        }

    private:
        stream_type stream_;
        RequestArena arena_;
        boost::asio::cancellation_signal signal_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/connection/tcp_connection/tcp_connection.cpp`**

```cpp
#include "tcp_connection.hpp"

#include <boost/beast/core/error.hpp>

namespace demiplane::http {

    boost::asio::awaitable<void> TcpConnection::async_close() {
        boost::beast::error_code ec;
        stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
        // Best-effort half-close; the socket closes when the stream is destroyed.
        co_return;
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Create `components/http/connection/tcp_connection/CMakeLists.txt`**

```cmake
##############################################################################
# Http Connection — TcpConnection (plain TCP over beast::tcp_stream)
##############################################################################
add_library(${DMP_HTTP}.Connection.Tcp STATIC tcp_connection.cpp)

target_include_directories(${DMP_HTTP}.Connection.Tcp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Connection.Tcp
        PUBLIC
        ${DMP_HTTP}.Connection.RequestArena
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Types.Enums
        Boost::beast
        Boost::asio
)
##############################################################################
```

- [ ] **Step 7: Wire into the aggregate** — `add_subdirectory(tcp_connection)` + `${DMP_HTTP}.Connection.Tcp` in the
  connection aggregate.

- [ ] **Step 8: Build + run — expect pass**

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Connection -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Connection 2>&1 | tail -5
```

- [ ] **Step 9: Commit**

```bash
git add components/http/connection/tcp_connection components/http/connection/CMakeLists.txt \
        tests/unit_tests/http/connection/test_tcp_connection.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/connection): TcpConnection (beast::tcp_stream + arena + cancel)

Satisfies StreamConnection; the plain-TCP connection the TcpListener spawns
(PR 4). Wire exercise is PR 4 integration — the driver is already wire-tested
via TestConnection."
```

---

## Task 15: h2/h3 driver scaffolds + QUIC connection scaffold

**Files:**

- Create: `components/http/drivers/http2/http2_driver.hpp`, `components/http/drivers/http2/CMakeLists.txt`
- Create: `components/http/drivers/http3/http3_driver.hpp`, `components/http/drivers/http3/CMakeLists.txt`
- Create: `components/http/connection/quic_connection/quic_connection.hpp`,
  `components/http/connection/quic_connection/CMakeLists.txt`
- Modify: `components/http/drivers/CMakeLists.txt`, `components/http/connection/CMakeLists.txt`
- Modify: `tests/unit_tests/http/drivers/test_driver_scaffolds.cpp`

**Goal:** Spec §6.4 / §7.3: compiling scaffolds that satisfy `HttpDriver`, advertise the right ALPN, and (for the
drivers) `serve()` by logging a warning and closing. They link no nghttp2/ngtcp2/nghttp3 symbols (D4).

- [ ] **Step 1: Append failing tests** to `tests/unit_tests/http/drivers/test_driver_scaffolds.cpp`:

```cpp
#include <http2_driver.hpp>
#include <http3_driver.hpp>

static_assert(HttpDriver<Http2Driver>);
static_assert(HttpDriver<Http3Driver>);

TEST(DriverScaffoldsTest, Http2AdvertisesH2) {
    EXPECT_EQ(Http2Driver::id(), Protocol::http2);
    ASSERT_EQ(Http2Driver::accepted_alpns().size(), 1u);
    EXPECT_EQ(Http2Driver::accepted_alpns()[0], "h2");
}

TEST(DriverScaffoldsTest, Http3AdvertisesH3) {
    EXPECT_EQ(Http3Driver::id(), Protocol::http3);
    ASSERT_EQ(Http3Driver::accepted_alpns().size(), 1u);
    EXPECT_EQ(Http3Driver::accepted_alpns()[0], "h3");
}
```

- [ ] **Step 2: Build — expect failure** (`http2_driver.hpp` missing).

- [ ] **Step 3: Create `components/http/drivers/http2/http2_driver.hpp`**

```cpp
#pragma once

#include <span>
#include <string_view>

#include <boost/asio/awaitable.hpp>
#include <demiplane/scroll>

#include <connection_concepts.hpp>
#include <http_enums.hpp>
#include <router.hpp>

namespace demiplane::http {

    /// HTTP/2 driver — SCAFFOLD (spec §6.4). serve() logs and closes; fill in
    /// with nghttp2 in a future PR (its vcpkg dep is already in the manifest,
    /// D4). Satisfies HttpDriver so the TLS listener can carry it via ALPN.
    class Http2Driver {
    public:
        static constexpr Protocol id() noexcept {
            return Protocol::http2;
        }
        static constexpr std::span<const std::string_view> accepted_alpns() noexcept {
            static constexpr std::string_view kAlpns[] = {"h2"};
            return kAlpns;
        }

        template <StreamConnection ConnT>
        boost::asio::awaitable<void> serve(ConnT& conn, Router& /*router*/) {
            COMPONENT_LOG_WRN() << "Http2Driver::serve() not implemented (scaffold)";
            co_await conn.async_close();
        }

    private:
        SCROLL_COMPONENT_PREFIX("Http2Driver");
    };

}  // namespace demiplane::http
```

- [ ] **Step 4: Create `components/http/drivers/http2/CMakeLists.txt`**

```cmake
##############################################################################
# Http Drivers — Http2Driver (SCAFFOLD; nghttp2 not linked yet, D4)
##############################################################################
add_library(${DMP_HTTP}.Drivers.Http2 INTERFACE http2_driver.hpp)

target_include_directories(${DMP_HTTP}.Drivers.Http2 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Drivers.Http2 INTERFACE
        ${DMP_HTTP}.Drivers.Concept
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Routing.Router
        Demiplane::Common::Scroll
)
##############################################################################
```

- [ ] **Step 5: Create `components/http/drivers/http3/http3_driver.hpp`** (mirror of h2 with `Protocol::http3` /
  `"h3"` / `"Http3Driver"`)

```cpp
#pragma once

#include <span>
#include <string_view>

#include <boost/asio/awaitable.hpp>
#include <demiplane/scroll>

#include <connection_concepts.hpp>
#include <http_enums.hpp>
#include <router.hpp>

namespace demiplane::http {

    /// HTTP/3 driver — SCAFFOLD (spec §6.4). serve() logs and closes; fill in
    /// with ngtcp2 + nghttp3 in a future PR (vcpkg deps already in the manifest,
    /// D4). Pairs with QuicConnection (QUIC transport) when implemented.
    class Http3Driver {
    public:
        static constexpr Protocol id() noexcept {
            return Protocol::http3;
        }
        static constexpr std::span<const std::string_view> accepted_alpns() noexcept {
            static constexpr std::string_view kAlpns[] = {"h3"};
            return kAlpns;
        }

        template <Connection ConnT>
        boost::asio::awaitable<void> serve(ConnT& conn, Router& /*router*/) {
            COMPONENT_LOG_WRN() << "Http3Driver::serve() not implemented (scaffold)";
            co_await conn.async_close();
        }

    private:
        SCROLL_COMPONENT_PREFIX("Http3Driver");
    };

}  // namespace demiplane::http
```

- [ ] **Step 6: Create `components/http/drivers/http3/CMakeLists.txt`**

```cmake
##############################################################################
# Http Drivers — Http3Driver (SCAFFOLD; ngtcp2 + nghttp3 not linked yet, D4)
##############################################################################
add_library(${DMP_HTTP}.Drivers.Http3 INTERFACE http3_driver.hpp)

target_include_directories(${DMP_HTTP}.Drivers.Http3 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Drivers.Http3 INTERFACE
        ${DMP_HTTP}.Drivers.Concept
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Routing.Router
        Demiplane::Common::Scroll
)
##############################################################################
```

- [ ] **Step 7: Create `components/http/connection/quic_connection/quic_connection.hpp`** (the QUIC connection
  scaffold — satisfies `Connection` but not `StreamConnection`; it has no byte-stream `stream()`)

```cpp
#pragma once

#include <chrono>
#include <cstddef>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>

#include <http_enums.hpp>
#include <request_arena.hpp>

namespace demiplane::http {

    /// QUIC connection — SCAFFOLD (spec §6.1). ngtcp2 state lands when h3 is
    /// implemented. Satisfies Connection (arena + lifecycle) but NOT
    /// StreamConnection (QUIC is not a single byte stream).
    class QuicConnection {
    public:
        explicit QuicConnection(std::size_t arena_size = 8192)
            : arena_{arena_size} {
        }

        QuicConnection(const QuicConnection&)            = delete;
        QuicConnection& operator=(const QuicConnection&) = delete;
        QuicConnection(QuicConnection&&)                 = delete;
        QuicConnection& operator=(QuicConnection&&)      = delete;

        std::pmr::polymorphic_allocator<> arena_alloc() noexcept {
            return arena_.allocator();
        }
        void reset_request_arena() {
            arena_.reset();
        }
        void expires_after(std::chrono::milliseconds) noexcept {}
        boost::asio::awaitable<void> async_close() {
            co_return;
        }
        boost::asio::cancellation_slot cancel_slot() noexcept {
            return signal_.slot();
        }
        boost::asio::ip::address remote_address() const {
            return {};
        }
        static Protocol negotiated_protocol() noexcept {
            return Protocol::http3;
        }
        static bool is_secure() noexcept {
            return true;
        }

    private:
        RequestArena arena_;
        boost::asio::cancellation_signal signal_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 8: Create `components/http/connection/quic_connection/CMakeLists.txt`**

```cmake
##############################################################################
# Http Connection — QuicConnection (SCAFFOLD; ngtcp2 state lands with h3)
##############################################################################
add_library(${DMP_HTTP}.Connection.Quic INTERFACE quic_connection.hpp)

target_include_directories(${DMP_HTTP}.Connection.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Connection.Quic INTERFACE
        ${DMP_HTTP}.Connection.RequestArena
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Types.Enums
        Boost::asio
)
##############################################################################
```

- [ ] **Step 9: Wire all three into their aggregates** — `add_subdirectory(http2)` + `add_subdirectory(http3)` +
  the two targets in `components/http/drivers/CMakeLists.txt`; `add_subdirectory(quic_connection)` +
  `${DMP_HTTP}.Connection.Quic` in `components/http/connection/CMakeLists.txt`.

- [ ] **Step 10: Build + run — expect pass**

```bash
cmake --preset release 2>&1 | tail -3
cmake --build build/release --target Demiplane.Tests.Unit.Http.Drivers Demiplane.Tests.Unit.Http.Connection -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R "Http.Drivers|Http.Connection" 2>&1 | tail -10
```

- [ ] **Step 11: Commit**

```bash
git add components/http/drivers/http2 components/http/drivers/http3 \
        components/http/connection/quic_connection components/http/drivers/CMakeLists.txt \
        components/http/connection/CMakeLists.txt tests/unit_tests/http/drivers/test_driver_scaffolds.cpp
git commit -m "feat(http): h2/h3 driver + QUIC connection scaffolds

Satisfy HttpDriver / Connection, advertise h2/h3 ALPN, serve() logs+closes.
No nghttp2/ngtcp2/nghttp3 symbols linked (D4) — future impl PRs fill serve()."
```

---

## Task 16: Driver allocation gate — framework translation path is zero-extra-heap

**Files:**

- Create: `tests/unit_tests/http/drivers/test_driver_allocation_gate.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** Enforce the §11 invariant for the part of the path the framework controls. Per **D5** (spike-confirmed: Beast
async I/O allocates ~1/op irreducibly), the gate measures **`build_request_context` → `router.dispatch`** (the framework
path that builds the arena-backed `Response`) under an armed global-`operator new` counter — **excluding
`make_beast_response` and async I/O**, the Beast-translation boundary (Beast's `fields::insert` allocates one node per
response header, by construction). It asserts **differentials**, not an absolute frame budget: arena response-header
mutation adds **0**; one >SSO user-body string adds exactly **1**. This catches an accidental framework allocation (a
copied `std::function`, a non-arena container) as a regression while staying maintainable across compiler changes.

- [ ] **Step 1: Write the test** — `tests/unit_tests/http/drivers/test_driver_allocation_gate.cpp`

```cpp
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <memory_resource>
#include <new>
#include <string>
#include <tuple>
#include <vector>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/beast/http.hpp>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <group.hpp>
#include <http11_driver.hpp>
#include <middleware.hpp>
#include <route_registry.hpp>
#include <router.hpp>

using namespace demiplane::http;
namespace http = boost::beast::http;

// ── Armed global operator new/delete (same mechanism as the PR1/PR2 gates) ──
namespace {
    std::atomic<std::size_t> g_allocs{0};
    thread_local bool t_armed = false;

    struct ArmedRegion {
        std::size_t start = g_allocs.load(std::memory_order_relaxed);
        ArmedRegion() {
            t_armed = true;
        }
        [[nodiscard]] std::size_t finish() {
            t_armed = false;
            return g_allocs.load(std::memory_order_relaxed) - start;
        }
    };
}  // namespace

void* operator new(std::size_t n) {
    if (t_armed)
        g_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n))
        return p;
    throw std::bad_alloc{};
}
void* operator new(std::size_t n, std::align_val_t a) {
    if (t_armed)
        g_allocs.fetch_add(1, std::memory_order_relaxed);
    const auto al = static_cast<std::size_t>(a);
    if (void* p = std::aligned_alloc(al, (n + al - 1) / al * al))
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
    // Stack arena — never reaches operator new (same trap-avoidance as PR1/PR2).
    struct StackArena {
        std::array<std::byte, 16384> buf{};
        std::pmr::monotonic_buffer_resource res{buf.data(), buf.size()};
        std::pmr::polymorphic_allocator<> alloc{&res};
    };

    // A controller whose handlers exercise the three measured shapes.
    class GateController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/empty", &GateController::empty);     // no body  -> baseline
            Get("/withbody", &GateController::body);    // 1 user string
        }

    private:
        AsyncResponse empty(RequestContext ctx) {
            co_return ctx.no_content();
        }
        AsyncResponse body(RequestContext ctx) {
            co_return ctx.ok(std::string(64, 'B'));  // >SSO: exactly one heap string
        }
    };

    // Build a parsed GET request of the driver's exact type, with `target`,
    // outside any armed region.
    detail::Http11Request make_get(std::pmr::polymorphic_allocator<> arena, std::string target) {
        std::pmr::polymorphic_allocator<char> body_alloc{arena.resource()};
        detail::Http11Request req{std::piecewise_construct, std::forward_as_tuple(body_alloc),
                                  std::forward_as_tuple()};
        req.method(http::verb::get);
        req.target(target);
        req.version(11);
        return req;
    }

    // Measure global allocs across build_request_context -> dispatch -> make_beast_response.
    std::size_t measure(Router& router, std::string target, StackArena& arena) {
        boost::asio::io_context ioc;
        auto fut = boost::asio::co_spawn(
            ioc,
            [&]() -> boost::asio::awaitable<std::size_t> {
                detail::Http11Request req = make_get(arena.alloc, target);  // before arming
                ArmedRegion region;
                RequestContext ctx  = detail::build_request_context(req, arena.alloc);
                Response resp       = co_await router.dispatch(std::move(ctx));
                const std::size_t n = region.finish();  // STOP before Beast translation (D5)
                // make_beast_response is the Beast-fields translation boundary:
                // each msg.insert() allocates a fields node (global heap, by
                // construction — like async I/O). Exercised for realism, NOT
                // counted. If it were inside the region, the middleware test
                // below would see +2 (its two added headers) and fail spuriously.
                (void)detail::make_beast_response(resp);
                co_return n;
            },
            boost::asio::use_future);
        ioc.run();
        return fut.get();
    }

    Router freeze_router(RouteRegistry& reg, std::vector<std::shared_ptr<HttpController>>& sink,
                         const std::vector<Middleware>& mws = {}) {
        auto ctrl = std::make_shared<GateController>();
        for (const auto& mw : mws)
            ctrl->add_middleware(mw);
        GroupBinding{reg, sink, ""}.add_controller(ctrl);
        (void)reg.freeze();
        return Router{reg};
    }
}  // namespace

TEST(DriverAllocationGateTest, ArenaHeaderMutationAddsNoGlobalHeap) {
    // Compare TWO middleware variants with the SAME coroutine-frame structure
    // (each user middleware that co_awaits next is itself one heap frame). The
    // ONLY difference is whether the post-handler middleware mutates two
    // response headers. A bare-vs-middleware comparison would be confounded by
    // the middleware's own frame; this isolates the arena-header-mutation cost.
    Middleware passthrough = [](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        co_return co_await next(std::move(ctx));
    };
    Middleware add_headers = [](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        Response r = co_await next(std::move(ctx));
        r.add_header("X-A", "1");
        r.add_header("X-B", "2");  // arena-backed — must not touch the global heap
        co_return r;
    };

    RouteRegistry reg_pass;
    std::vector<std::shared_ptr<HttpController>> sink_pass;
    Router r_pass = freeze_router(reg_pass, sink_pass, {passthrough});
    StackArena a_pass;
    const std::size_t passthrough_allocs = measure(r_pass, "/empty", a_pass);

    RouteRegistry reg_add;
    std::vector<std::shared_ptr<HttpController>> sink_add;
    Router r_add = freeze_router(reg_add, sink_add, {add_headers});
    StackArena a_add;
    const std::size_t with_headers = measure(r_add, "/empty", a_add);

    EXPECT_EQ(with_headers, passthrough_allocs)
        << "post-handler arena header mutation hit the global heap (" << with_headers << " vs "
        << passthrough_allocs << ")";
}

TEST(DriverAllocationGateTest, OneUserBodyStringIsExactlyOneAlloc) {
    RouteRegistry reg_base;
    std::vector<std::shared_ptr<HttpController>> sink_base;
    Router r_base = freeze_router(reg_base, sink_base);
    StackArena a_base;
    const std::size_t baseline = measure(r_base, "/empty", a_base);

    RouteRegistry reg_body;
    std::vector<std::shared_ptr<HttpController>> sink_body;
    Router r_body = freeze_router(reg_body, sink_body);
    StackArena a_body;
    const std::size_t with_body = measure(r_body, "/withbody", a_body);

    EXPECT_EQ(with_body, baseline + 1)
        << "framework added allocations beyond the single user-body string (" << with_body << " vs "
        << baseline << "+1)";
}
```

- [ ] **Step 2: Register the test source** — add `drivers/test_driver_allocation_gate.cpp` to the `Http.Drivers` list.
  The final list:

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Drivers
        drivers/test_driver_scaffolds.cpp
        drivers/test_driver_helpers.cpp
        drivers/test_http11_driver.cpp
        drivers/test_driver_allocation_gate.cpp
)
```

- [ ] **Step 3: Build + run — expect pass.** A differential other than `0` / `+1` names a real framework allocation
  (a copied `std::function`, a non-arena container) — fix the framework, do not relax the gate:

```bash
cmake --build build/release --target Demiplane.Tests.Unit.Http.Drivers -- -j4 2>&1 | tail -5
ctest --test-dir build/release --output-on-failure -R Http.Drivers 2>&1 | tail -5
```

- [ ] **Step 4: Commit**

```bash
git add tests/unit_tests/http/drivers/test_driver_allocation_gate.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "test(http/drivers): allocation gate — framework path is zero-extra-heap

Differential gate (D5): arena header mutation adds 0; one user-body string
adds exactly 1. Excludes Beast async I/O (allocates ~1/op irreducibly)."
```

---

## Task 17: Full verification — whole suite, ASan, TSan, self-review

**Files:** none (verification only; fix-forward if anything fails).

- [ ] **Step 1: Full release build + full unit suite**

```bash
cmake --build build/release -- -j4 2>&1 | tail -10
ctest --test-dir build/release --output-on-failure -L unit 2>&1 | tail -20
```

Expected: build clean, all unit tests green. New targets: `Http.Connection` (~5 tests), `Http.Drivers` (~18 tests);
`Http.Types` grew by 3 (Body::beast_view). Any non-http failure is pre-existing — investigate before assuming.

- [ ] **Step 2: ASan/UBSan pass** (the riskiest code: arena/body view lifetimes across dispatch + write, and the
  coroutine session loop holding the parser alive)

```bash
cmake --preset asan 2>&1 | tail -3
cmake --build build/asan --target Demiplane.Tests.Unit.Http.Drivers Demiplane.Tests.Unit.Http.Connection Demiplane.Tests.Unit.Http.Types -- -j4 2>&1 | tail -5
ctest --test-dir build/asan --output-on-failure -R "Http.Drivers|Http.Connection|Http.Types" 2>&1 | tail -15
```

Expected: zero ASan/UBSan reports. If the allocation-gate counts become unreliable under ASan's `operator new`
interposition on this toolchain (only if actually observed), guard those two tests with
`GTEST_SKIP()` under `__has_feature(address_sanitizer)` and note it in the test — do not weaken the release-build gate.

- [ ] **Step 3: TSan pass** (the driver session loop + cancellation slot under the scheduler)

```bash
cmake --preset tsan 2>&1 | tail -3
cmake --build build/tsan --target Demiplane.Tests.Unit.Http.Drivers -- -j4 2>&1 | tail -5
ctest --test-dir build/tsan --output-on-failure -R Http.Drivers 2>&1 | tail -10
```

Expected: zero TSan reports (the tests are single-threaded io_contexts; this guards the spawned-coroutine plumbing).

- [ ] **Step 4: Self-review against the spec** (run the checklist, fix inline, re-run affected tests):

1. **Spec coverage** — §6.1 RequestArena ✔ Task 3; Connection/StreamConnection concepts ✔ Task 4; TcpConnection ✔
   Task 14; QuicConnection scaffold ✔ Task 15. §6.2 HttpDriver concept ✔ Task 6. §6.3 Http11Driver: config ✔ Task 7;
   helpers ✔ Task 8; serve() session loop + body limit + per-phase timeouts (wired, D7) + URL decode (via
   RequestContext, PR 2) + handler-exception → 500 + Date/Server stamping + cancellation-aware I/O ✔ Tasks 9–13. §6.4
   h2/h3 scaffolds ✔ Task 15. §11 allocation invariant (framework path, D5) ✔ Task 16. §13 vcpkg deps ✔ Task 2.
   **Deferred (documented):** D2 arena header parsing (Types.Headers change), D3 TlsConnection (PR 4), D6 HEAD /
   100-continue, D7 timeout *behavior* (PR 4 real socket), listeners/TLS/ALPN (PR 4), Server lifecycle (PR 5).
2. **No placeholders** — `request_arena.cpp` is an intentional TU anchor (documented); `serve()`/`write_response()`
   are split into `http11_serve.inl` (a real definition, not a stub) included from the header. No TBDs.
3. **Type consistency spot-checks** — `detail::Http11Request`/`Http11Parser`/`Http11Body` used by the helper tests,
   the gate, and `serve()`; `Body::beast_view(std::span<const std::byte>)` consumed by `build_request_context`;
   `Headers::view_of_beast(req.base())` compiles because the parser's fields allocator is `std::allocator` (D1);
   `make_beast_response(Response&)` returns `response<buffer_body>` pointing at `resp.body`; `StreamConnection` modelled
   by `TestConnection` (Task 9) + `TcpConnection` (Task 14); `Connection` (not Stream) modelled by `QuicConnection`.

- [ ] **Step 5: Status report.** Summarize: tests added/passing, ASan/TSan results, any deviations taken during
  execution (especially the Task 2 vcpkg fallback if used). Do NOT push or open a PR unless explicitly asked.

---

## Out of scope (deferred, with owners)

| Item                                                                                             | Where it lands                               |
|--------------------------------------------------------------------------------------------------|----------------------------------------------|
| Listeners (`TcpListener`/`TlsListener`), `ConnectionTracker`, `build_ssl_context`, ALPN          | PR 4                                         |
| `TlsConnection` value type (D3)                                                                  | PR 4 (produced by `TlsListener` + handshake) |
| Timeout *behavior* tests (real socket fires the timer, D7)                                       | PR 4 integration                             |
| End-to-end integration tests on `127.0.0.1:0` via real Beast clients                             | PR 4                                         |
| Arena-backed header *parsing* (generalize `Headers::BeastBacking` over the fields allocator, D2) | Types.Headers follow-up                      |
| `HEAD` auto-fallback + body suppression; `Expect: 100-continue` (D6)                             | Driver follow-up                             |
| `Server` orchestration, graceful shutdown, observers, `RouteConflictAggregateError` throw        | PR 5                                         |
| `ServerConfig` feeding `Http11Config` / driver instances                                         | PR 6                                         |
| nghttp2/ngtcp2/nghttp3 `find_package` + link + real `serve()`                                    | future h2/h3 impl PRs                        |
| `StreamingProducerBody` / `ctx.stream(...)` (chunked responses)                                  | future (needs `Body::Writer`)                |