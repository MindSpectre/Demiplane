# HTTP Redesign — PR 7: Umbrella + Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish the 7-PR HTTP redesign: make `Demiplane::Component::Http` + `#include <demiplane/http>` actually work
(the umbrella header is empty and the combined library links nothing), restore the HTTP benchmark deleted with the old
server (plus a new-API bench server to bomb), create the `examples/http/` tree the spec promised, and reconcile the
spec so it stops describing a future that has now landed. Spec: §12.1/§12.2 PR 7 + §13 of
`docs/superpowers/specs/2026-05-07-http-redesign-design.md`.

**Architecture:** No new runtime code paths — this PR is aggregation, restoration, and documentation. The umbrella is
an INTERFACE combined library (`add_combined_library`) that links the seven landed layer aggregates and ships one
header including every public leaf header; a dedicated smoke test links ONLY the umbrella to prove include-dir and
library propagation. The bomber is restored verbatim from git history (it is a pure Beast client — spec §12.1
"benchmark logic intact"); a small `bench_server` built on the new `run_standalone` API gives it something to hit.
The example is the spec §10.3 standalone path as living documentation: controller + middleware + typed-error handler,
wired either programmatically or from `server.json` via `load_server_config` + `attach_default_listeners`.

**Tech Stack:** CMake (`add_combined_library`, `add_unit_test`), the landed PR 1–6 dotted layer aggregates
(`Demiplane.Component.HTTP.{Types,Routing,Connection,Drivers,Listeners,Config,Server}`), GoogleTest, Boost.Beast
(bomber client), `demiplane::http` public API (`run_standalone`, `HttpController`, `RequestContext`, `AsyncOutcome`,
`load_server_config`).

## Global Constraints

- Verify every task with the **`debug`** preset: `cmake --build build/debug --target <target> -- -j4` clean, then
  `ctest --test-dir build/debug --output-on-failure -R <pattern>`. The final sweep builds the full tree.
- **No git commits by the executing agent** — the user manages git. Per-task `git` blocks below are the recommended
  grouping for when the user commits. **Never add a Claude/Co-Authored-By signature.**
- Dotted CMake target names (`Demiplane.Component.HTTP.*`, `Demiplane.Tests.Unit.*`, `Demiplane.Benchmarks.*`,
  `Demiplane.Examples.*`); **no new `::` aliases**. The one pre-existing alias `Demiplane::Component::Http`
  (`components/http/CMakeLists.txt:64`) is the public name executables link.
- This PR adds **zero per-request code** — the §11 zero-additional-allocation invariant is untouched. Do not edit
  anything under `components/http/` except the umbrella header and the root `components/http/CMakeLists.txt`.
- New executables (bomber, bench server, example) compile under the global `-Werror -Wall -Wextra -Wpedantic
  -Wconversion -Wshadow ...` set AND the global `CMAKE_CXX_CLANG_TIDY` — same bar as everything else in the tree.
- 4-space indent, `/** */` doc comments, `[[nodiscard]]` where applicable — match the landed HTTP sources. C++23
  **headers**, not modules. The umbrella and consumers use `#include <demiplane/http>`; cross-leaf includes stay
  rooted (`<server.hpp>`), never relative.
- Tests keep **per-layer dotted links** (user decision D4, see Reconciliation) — do NOT rewrite existing test link
  lists to use the umbrella.
- Do not run TSan by default; 3 pre-existing TSan races live in Nexus ctor/dtor and fire on any component's first
  `COMPONENT_LOG_*` — they are NOT HTTP regressions.

---

## Reconciliation against the landed code + spec (read before executing)

Spec §12.2 defines PR 7 as "the deletions PR" — delete `aliases.hpp`, old `server.hpp`/`controller.hpp`, port the
manual test, update benchmarks. Most of that never survived to PR 7:

### Landed facts (authoritative over the spec)

1. **The old code is already gone.** Commit `150b666` ("Http revision: types (#92)", the PR 1 squash; staged by
   `d3c40ef`) deleted the entire legacy `http_server/` (controller, server, route_registry, request_context,
   response_factory, `aliases.hpp`), the old config headers (`router_config.hpp`, `tls_config.hpp`,
   `firewall_config.hpp`), `tests/manual_tests/http/` (whole tree — `manual_tests/` no longer exists at all), and
   `benchmarks/http/` (bomber + CMake + the `BUILD_HTTP` block in `benchmarks/CMakeLists.txt`). There is nothing
   left to delete; `grep` finds zero references to any old-API symbol outside `docs/`.
2. **The umbrella is a stub.** `components/http/export/demiplane/http` contains only `#pragma once`;
   `add_combined_library(${DMP_HTTP} ... LIBRARIES )` at `components/http/CMakeLists.txt:56-62` links **nothing**.
   Nothing in the repo consumes `<demiplane/http>` or `Demiplane::Component::Http` today. Every PR 1–6 plan deferred
   populating it to PR 7.
3. **Layer aggregates (all INTERFACE, all landed):** `Demiplane.Component.HTTP.{Types,Routing,Connection,Drivers,
   Listeners,Config,Server}` (`${DMP_HTTP}` = `${DMP_COMPONENT}.HTTP` = `Demiplane.Component.HTTP`). Public leaf
   headers per layer (each leaf target owns its include dir; includes are rooted/flat):
    - types: `http_enums.hpp async_outcome.hpp url_decode.hpp headers.hpp errors.hpp body.hpp request.hpp
     response.hpp response_factory.hpp request_context.hpp`
    - routing: `route_registry.hpp controller.hpp middleware.hpp group.hpp router.hpp`
    - connection: `connection_concepts.hpp request_arena.hpp tcp_connection.hpp tls_connection.hpp
     quic_connection.hpp`
    - drivers: `http_driver_concept.hpp http11_config.hpp http11_driver.hpp http2_driver.hpp http3_driver.hpp`
    - listeners: `listener_base.hpp connection_tracker.hpp tcp_listener.hpp tls_listener.hpp build_ssl_context.hpp
     quic_listener.hpp`
    - config: `timeouts.hpp tls_config.hpp listener_config.hpp server_config.hpp load_server_config.hpp`
    - server: `server_observer.hpp server.hpp run_standalone.hpp attach_default_listeners.hpp`
4. **Manual-test porting is already done.** The integration suite (`tests/integration_tests/http/`, PR 4/5) covers
   everything the manual handler demonstrated, against real sockets. The §12.1 disposition "example pattern stays
   available under `examples/http/`" is the only unfulfilled half — no `examples/` tree exists anywhere in the repo.
5. **The old bomber CMake linked `Demiplane::Component::Http`** — restoring it unchanged is also the first
   non-test consumer of the umbrella. The bomber source itself uses only Boost.Beast/Asio (zero demiplane types),
   so "benchmark logic intact" (§12.1) is a verbatim restore.
6. **Key public API shapes** (verified against landed headers — used by the new code in Tasks 1/3/4):
    - `ResponseFactory::ok(std::string body = "", std::string_view ct = "text/plain")` → `Response{.status, .headers,
     .body}`; `Body::buffered_view()` → `std::optional<std::string_view>`.
    - `RouteRegistry{PathNormalization = collapse_trailing_slash}`; `add_route(HttpMethod, std::string_view path,
     ContextHandler)`; `[[nodiscard]] freeze()` → `std::vector<RouteConflictError>`; `find_route(HttpMethod,
     std::string_view, std::pmr::polymorphic_allocator<>)` → `gears::Outcome<ResolvedRoute, NotFoundError,
     MethodNotAllowedError>`.
    - `RequestArena{std::size_t = 8192}` → `.allocator()`, `.reset()`.
    - `Http11Driver{const Http11Config&}` (explicit noexcept); `Http11Driver::id()`/`Http2Driver::id()`/
      `Http3Driver::id()` all `static constexpr Protocol`.
    - `ConnectionTracker` default-constructible; `.in_flight()` → `std::size_t`.
    - `ServerConfig::Builder{}.finalize()` (canonical empty config); accessors `threads() listeners() timeouts()
     body_limit() request_arena_size() drain_timeout()`.
    - `Server{ServerConfig, boost::asio::any_io_executor}`; `add_tcp_listener(std::string host, std::uint16_t port,
     Driver)`; `add_controller(std::shared_ptr<C>)`; `in_group(std::string)`; `listeners()` →
      `std::span<const std::unique_ptr<ListenerBase>>`; `is_running()`.
    - `run_standalone(ServerConfig, std::size_t threads, const std::function<void(Server&)>& configure)` — installs
      SIGINT/SIGTERM → graceful stop, blocks until shutdown, throws `std::invalid_argument` on `threads == 0`.
    - `attach_default_listeners(Server&)` — walks `server.config().listeners()`; empty array = no-op.
    - `load_server_config(std::string_view)` → `gears::Outcome<ServerConfig, ConfigFileError{path,reason},
     ConfigParseError{path,line,detail}, ConfigSchemaError{path,field_path,detail}>`; Outcome API:
      `is_error()`, `holds_error<E>()`, `error<E>()`, `value()`, `gears::err(E)`.
    - Middleware: `using NextHandler = ContextHandler = std::function<AsyncResponse(RequestContext)>`;
      `using Middleware = std::function<AsyncResponse(RequestContext, const NextHandler&)>`;
      `HttpController::add_middleware(Mw&&)`.
    - `Body::read_to_string(std::size_t limit)` → `AsyncOutcome<std::string, BodyLimitExceeded>`;
      `RequestContext::body()`, `path_param_or<T>(name, fallback)`.
7. **Build wiring:** `BUILD_HTTP` (default ON under `BUILD_COMPONENTS`) gates `components/http` and the HTTP test
   dirs; `DO_BENCHMARKS` (default ON) gates `benchmarks/`; options live in `cmake/features.cmake`. Test macros:
   `add_unit_test(<target> <sources...>)` + separate `target_link_libraries(... ${TEST_LIBS})`. Benchmark target
   prefix: `${DMP_BENCHMARKS}` = `Demiplane.Benchmarks` (set in `benchmarks/CMakeLists.txt`).

### Decisions taken (user-confirmed 2026-07-09, each documented in the relevant task)

- **D1 — `routing/firewall/` data types are DROPPED from v1.** Spec §4/§12.1 wanted `rate_limit.hpp`/`ip_rule.hpp`
  moved there as data-only inputs to user-written middleware; they were deleted with the old config and nothing
  consumes them. Recreating dead types is YAGNI — they return with the actual rate-limit middleware. Spec updated
  (Task 5).
- **D2 — benchmarks are restored, not just recorded.** `http_bomber.cpp` comes back verbatim from
  `150b666~1` (logic intact per §12.1) plus a new `bench_server` executable on the new API so the pair is usable
  out of the box.
    - **D2 SUPERSEDED (2026-07-09, throughput comparison work).** The verbatim bomber cannot generate saturating
      load: it opened a fresh TCP connection per request (`Connection: close`), blocked on one request at a time
      per thread, and slept `interval_ms` between requests — it measured connection setup and its own sleep, never
      HTTP serving throughput. `http_bomber.cpp` was rewritten in place as an async keep-alive generator
      (io_context per thread, N connections, optional pipelining, per-thread counters, client-side clock). The
      smoke test in Task 3 Step 4 below uses the old positional CLI and no longer applies; the new CLI is
      `--host/--port/--path/--threads/--conns/--pipeline/--requests/--warmup [--strict] [--json]`.
      "Benchmark logic intact" was preserved for `bench_server.cpp`, which is unchanged.
- **D3 — a top-level `examples/` tree is created** (`BUILD_EXAMPLES` option, default ON), first inhabitant
  `examples/http/minimal_http_server.cpp` + `server.json` — the §10.3 standalone path as living documentation.
- **D4 — existing tests keep per-layer dotted links.** Per-layer links catch under-linking regressions in each
  layer's CMake; umbrella propagation is guarded by the new dedicated smoke target instead. Only stale comments
  change.

---

## File Structure

```
components/http/export/demiplane/http                 MODIFY  umbrella header — include all public leaf headers
components/http/CMakeLists.txt                        MODIFY  populate add_combined_library LIBRARIES
tests/unit_tests/http/umbrella/test_umbrella.cpp      CREATE  smoke test linking ONLY the umbrella
tests/unit_tests/http/CMakeLists.txt                  MODIFY  add umbrella test target; fix stale PR-7 comment
benchmarks/http/http_bomber.cpp                       CREATE  verbatim restore from 150b666~1
benchmarks/http/bench_server.cpp                      CREATE  new-API server for the bomber to hit
benchmarks/http/CMakeLists.txt                        CREATE  both benchmark targets
benchmarks/CMakeLists.txt                             MODIFY  restore BUILD_HTTP-gated add_subdirectory(http)
cmake/features.cmake                                  MODIFY  add BUILD_EXAMPLES option
CMakeLists.txt                                        MODIFY  add BUILD_EXAMPLES-gated add_subdirectory(examples)
examples/CMakeLists.txt                               CREATE  examples root (DMP_EXAMPLES prefix, BUILD_HTTP gate)
examples/http/CMakeLists.txt                          CREATE  example target
examples/http/minimal_http_server.cpp                 CREATE  spec §10.3 standalone-path example
examples/http/server.json                             CREATE  sample config for the JSON wiring path
docs/superpowers/specs/2026-05-07-http-redesign-design.md  MODIFY  final reconciliation (Task 5)
```

---

### Task 1: Umbrella aggregation + smoke test

**Files:**

- Create: `tests/unit_tests/http/umbrella/test_umbrella.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt` (append target; fix stale comment at the Types section)
- Modify: `components/http/export/demiplane/http`
- Modify: `components/http/CMakeLists.txt:56-62`

**Interfaces:**

- Consumes: the seven landed layer aggregates `${DMP_HTTP}.{Types,Routing,Connection,Drivers,Listeners,Config,Server}`.
- Produces: a working `Demiplane.Component.HTTP` combined INTERFACE lib (alias `Demiplane::Component::Http`) that
  propagates every layer's include dirs + libs, and `#include <demiplane/http>` resolving to the full public API.
  Tasks 2–4 link it.

- [ ] **Step 1: Write the failing smoke test**

Create `tests/unit_tests/http/umbrella/test_umbrella.cpp`:

```cpp
/**
 * PR 7 umbrella smoke test: this translation unit includes ONLY
 * <demiplane/http> and its target links ONLY Demiplane.Component.HTTP (plus
 * gtest). One construct per layer proves the combined library propagates
 * every layer's include directories and libraries on its own — any
 * under-aggregation in the umbrella breaks this target and nothing else.
 */
#include <demiplane/http>

#include <memory_resource>
#include <string>

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

namespace {

    namespace http = demiplane::http;

    TEST(HttpUmbrella, TypesLayerIsReachable) {
        const auto response = http::ResponseFactory::ok("hello");
        EXPECT_EQ(response.status, http::HttpStatus::ok);
        ASSERT_TRUE(response.body.buffered_view().has_value());
        EXPECT_EQ(*response.body.buffered_view(), "hello");
    }

    TEST(HttpUmbrella, RoutingAndConnectionLayersAreReachable) {
        http::RouteRegistry registry;
        registry.add_route(http::HttpMethod::get, "/smoke",
                           [](http::RequestContext ctx) -> http::AsyncResponse { co_return ctx.ok("pong"); });
        EXPECT_TRUE(registry.freeze().empty());

        http::RequestArena arena{1024};
        EXPECT_TRUE(registry.find_route(http::HttpMethod::get, "/smoke", arena.allocator()).is_success());

        std::pmr::string arena_backed{"arena-backed", arena.allocator()};
        EXPECT_EQ(arena_backed, "arena-backed");
    }

    TEST(HttpUmbrella, DriversLayerIsReachable) {
        static_assert(http::Http11Driver::id() == http::Protocol::http1);
        static_assert(http::Http2Driver::id() == http::Protocol::http2);
        static_assert(http::Http3Driver::id() == http::Protocol::http3);
        const http::Http11Driver driver{http::Http11Config{}};
        static_cast<void>(driver);
    }

    TEST(HttpUmbrella, ListenersLayerIsReachable) {
        http::ConnectionTracker tracker;
        EXPECT_EQ(tracker.in_flight(), 0U);
    }

    TEST(HttpUmbrella, ConfigLayerIsReachable) {
        const auto cfg = http::ServerConfig::Builder{}.finalize();
        EXPECT_EQ(cfg.body_limit(), 16U * 1024U * 1024U);
        EXPECT_EQ(cfg.request_arena_size(), 8192U);
    }

    TEST(HttpUmbrella, ServerLayerIsReachable) {
        boost::asio::io_context ioc;
        const http::Server server{http::ServerConfig::Builder{}.finalize(), ioc.get_executor()};
        EXPECT_FALSE(server.is_running());
    }

}  // namespace
```

- [ ] **Step 2: Register the test target**

Append to `tests/unit_tests/http/CMakeLists.txt`:

```cmake
##############################################################################
# Test the public umbrella (PR 7): <demiplane/http> + the combined
# Demiplane.Component.HTTP library must expose every layer ON THEIR OWN —
# this target deliberately links nothing else (gtest aside), so any missing
# include-dir/library propagation in the umbrella breaks exactly this target.
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Umbrella
        umbrella/test_umbrella.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Umbrella
        PRIVATE
        Demiplane.Component.HTTP
        ${TEST_LIBS}
)
##############################################################################
```

- [ ] **Step 3: Build the test target — verify it FAILS**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Umbrella -- -j4`
Expected: FAIL — the umbrella header is empty, so every `demiplane::http` name in the test is undeclared
(`error: no member named 'ResponseFactory' in namespace 'demiplane::http'` or similar). If instead the *include*
itself fails, that is the same root cause (empty aggregation) — proceed.

- [ ] **Step 4: Fill the umbrella header**

Replace the entire contents of `components/http/export/demiplane/http` with:

```cpp
#pragma once

/**
 * Public umbrella for the demiplane HTTP component. Aggregates every public
 * leaf header of the seven layers (spec §3/§4); consumers link
 * Demiplane::Component::Http and write `#include <demiplane/http>`.
 */

// ── Types — protocol-agnostic HTTP primitives (spec §5) ─────────────────────
#include <async_outcome.hpp>
#include <body.hpp>
#include <errors.hpp>
#include <headers.hpp>
#include <http_enums.hpp>
#include <request.hpp>
#include <request_context.hpp>
#include <response.hpp>
#include <response_factory.hpp>
#include <url_decode.hpp>

// ── Routing (spec §8) ────────────────────────────────────────────────────────
#include <controller.hpp>
#include <group.hpp>
#include <middleware.hpp>
#include <route_registry.hpp>
#include <router.hpp>

// ── Connections (spec §6.1) ──────────────────────────────────────────────────
#include <connection_concepts.hpp>
#include <quic_connection.hpp>
#include <request_arena.hpp>
#include <tcp_connection.hpp>
#include <tls_connection.hpp>

// ── Protocol drivers (spec §6.2–§6.4) ────────────────────────────────────────
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <http2_driver.hpp>
#include <http3_driver.hpp>
#include <http_driver_concept.hpp>

// ── Listeners / TLS (spec §7) ────────────────────────────────────────────────
#include <build_ssl_context.hpp>
#include <connection_tracker.hpp>
#include <listener_base.hpp>
#include <quic_listener.hpp>
#include <tcp_listener.hpp>
#include <tls_listener.hpp>

// ── Config (spec §10) ────────────────────────────────────────────────────────
#include <listener_config.hpp>
#include <load_server_config.hpp>
#include <server_config.hpp>
#include <timeouts.hpp>
#include <tls_config.hpp>

// ── Server orchestration (spec §9) ───────────────────────────────────────────
#include <attach_default_listeners.hpp>
#include <run_standalone.hpp>
#include <server.hpp>
#include <server_observer.hpp>
```

- [ ] **Step 5: Populate the combined library**

In `components/http/CMakeLists.txt`, replace the `add_combined_library` block (lines 56-62) with:

```cmake
add_combined_library(${DMP_HTTP}
        DIRECTORIES
        export/
        SOURCES
        export/demiplane/http
        LIBRARIES
        ${DMP_HTTP}.Types
        ${DMP_HTTP}.Routing
        ${DMP_HTTP}.Connection
        ${DMP_HTTP}.Drivers
        ${DMP_HTTP}.Listeners
        ${DMP_HTTP}.Config
        ${DMP_HTTP}.Server
)
```

(The `add_library(Demiplane::Component::Http ALIAS ${DMP_HTTP})` line below it stays as-is.)

- [ ] **Step 6: Build + run — verify it PASSES**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Umbrella -- -j4`
Expected: clean build.
Run: `ctest --test-dir build/debug --output-on-failure -R "Http.Umbrella"`
Expected: 6 tests PASS.

- [ ] **Step 7: Fix the stale PR-7 comment in the Types test section**

In `tests/unit_tests/http/CMakeLists.txt`, the Types target carries this now-false comment:

```cmake
# Dotted aggregate of all the per-type Http Types leaf targets (no :: alias).
# NOT Demiplane::Component::Http — that umbrella aggregates only the old
# Handler lib until PR 7 and would not propagate the Types include dirs.
```

Replace those three lines with:

```cmake
# Dotted aggregate of all the per-type Http Types leaf targets (no :: alias).
# Deliberately NOT the Demiplane.Component.HTTP umbrella: per-layer links
# catch under-linking regressions in each layer's CMake (PR 7 D4); umbrella
# propagation is guarded by the dedicated ...Http.Umbrella target below.
```

- [ ] **Step 8: Re-run the full HTTP unit suite (regression check)**

Run: `ctest --test-dir build/debug --output-on-failure -R "Unit.Http"`
Expected: all HTTP unit binaries PASS (Types, Routing, Connection, Drivers, Listeners, Config, Server, Umbrella).

- [ ] **Step 9: Recommended commit grouping (user commits)**

```bash
git add components/http/export/demiplane/http components/http/CMakeLists.txt \
        tests/unit_tests/http/umbrella/test_umbrella.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http): aggregate the public umbrella + smoke test (PR 7)"
```

---

### Task 2: Restore the HTTP bomber benchmark

**Files:**

- Create: `benchmarks/http/http_bomber.cpp` (verbatim from `150b666~1`)
- Create: `benchmarks/http/CMakeLists.txt`
- Modify: `benchmarks/CMakeLists.txt`

**Interfaces:**

- Consumes: `Demiplane::Component::Http` (Task 1) — solely for Boost.Beast/Asio propagation; the bomber uses no
  demiplane types (landed fact 5).
- Produces: executable `Demiplane.Benchmarks.Http.Bomber` with CLI
  `<host> <port> <target> <threads> <interval_ms> <duration_seconds>`. Task 3's end-to-end smoke drives it.

- [ ] **Step 1: Restore the bomber source verbatim**

```bash
mkdir -p benchmarks/http
git show 150b666~1:benchmarks/http/http_bomber.cpp > benchmarks/http/http_bomber.cpp
```

Do NOT hand-edit the file — spec §12.1 requires "benchmark logic intact", and it last compiled under the same
warning set. (272 lines; a `HttpBomber` class: N worker threads issue closed-connection Beast GETs on an interval,
atomics-based stats + a 500 ms reporter thread, final RPS/latency summary.)

- [ ] **Step 2: Create the benchmark CMake**

Create `benchmarks/http/CMakeLists.txt`:

```cmake
##############################################################################
# HTTP load generator (restored in PR 7; deleted with the old server pre-PR 1).
# Pure Beast client — links the umbrella for Boost propagation and so a
# non-test consumer exercises Demiplane::Component::Http.
##############################################################################
add_executable(${DMP_BENCHMARKS}.Http.Bomber
        http_bomber.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Http.Bomber
        PRIVATE
        Demiplane::Component::Http
        Threads::Threads
)
##############################################################################
```

- [ ] **Step 3: Re-wire benchmarks to include the HTTP dir**

In `benchmarks/CMakeLists.txt`, after the `add_subdirectory(multithread)` line and before the `BUILD_DATABASE`
block, insert (this restores the exact block `150b666` deleted):

```cmake
if (BUILD_HTTP)
    message("Benchmarks for HTTP will be built")
    add_subdirectory(http)
endif ()
```

- [ ] **Step 4: Build — verify clean under -Werror + clang-tidy**

Run: `cmake --build build/debug --target Demiplane.Benchmarks.Http.Bomber -- -j4`
Expected: clean build (CMake re-runs automatically for the new subdirectory). If `-Werror`/clang-tidy findings
appear (flag set may have drifted since the file last built), STOP and use superpowers:systematic-debugging —
fix only the specific finding, keeping measurement logic identical.

- [ ] **Step 5: CLI sanity check (no server needed)**

Run: `./build/debug/benchmarks/http/Demiplane.Benchmarks.Http.Bomber`
Expected: exit code 1 and the usage line
`Usage: ... <host> <port> <target> <threads> <interval_ms> <duration_seconds>`.

- [ ] **Step 6: Recommended commit grouping (user commits)**

```bash
git add benchmarks/http/ benchmarks/CMakeLists.txt
git commit -m "feat(bench): restore the HTTP bomber (deleted pre-PR 1; spec §12.1)"
```

---

### Task 3: Bench server on the new API + end-to-end smoke

**Files:**

- Create: `benchmarks/http/bench_server.cpp`
- Modify: `benchmarks/http/CMakeLists.txt` (append second target)

**Interfaces:**

- Consumes: `Demiplane::Component::Http` (Task 1): `run_standalone`, `HttpController`, `Http11Driver`,
  `ServerConfig::Builder`, `RequestContext::{ok,json,path_param_or}`.
- Produces: executable `Demiplane.Benchmarks.Http.BenchServer` with CLI `[port=8080] [io_threads=4]`, endpoints
  `GET /ping` → `"pong"`, `GET /json` → small JSON, `GET /users/{id}` → parametric echo (the old bomber usage
  example `... 127.0.0.1 8080 /users/1 4 30 30` works again).

- [ ] **Step 1: Write the bench server**

Create `benchmarks/http/bench_server.cpp`:

```cpp
/**
 * Load-test counterpart of the HTTP bomber: a minimal demiplane HTTP server
 * exposing cheap endpoints. Not a measured benchmark itself — the bomber
 * reports client-side RPS/latency; this process is the thing under load.
 *
 *   ./Demiplane.Benchmarks.Http.BenchServer [port=8080] [io_threads=4]
 *   ./Demiplane.Benchmarks.Http.Bomber 127.0.0.1 8080 /ping 4 30 30
 *
 * Ctrl+C (SIGINT/SIGTERM) triggers graceful shutdown via run_standalone.
 */
#include <demiplane/http>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

namespace {

    namespace http = demiplane::http;

    /// GET /ping        → 200 "pong"           (fixed body — framework floor)
    /// GET /json        → 200 {"status":"ok"}  (arena-backed JSON response)
    /// GET /users/{id}  → 200 "user <id>"      (parametric route + arena decode)
    class BenchController final : public http::HttpController {
    public:
        void configure_routes() override {
            Get("/ping", &BenchController::ping);
            Get("/json", &BenchController::json);
            Get("/users/{id}", &BenchController::user);
        }

    private:
        http::AsyncResponse ping(http::RequestContext ctx) {
            co_return ctx.ok("pong");
        }

        http::AsyncResponse json(http::RequestContext ctx) {
            co_return ctx.json(R"({"status":"ok"})");
        }

        http::AsyncResponse user(http::RequestContext ctx) {
            co_return ctx.ok("user " + ctx.path_param_or<std::string>("id", std::string{"?"}));
        }
    };

}  // namespace

int main(const int argc, char* argv[]) {
    namespace http = demiplane::http;

    std::uint16_t port  = 8080;
    std::size_t threads = 4;
    if (argc > 1) {
        port = static_cast<std::uint16_t>(std::stoi(argv[1]));
    }
    if (argc > 2) {
        threads = static_cast<std::size_t>(std::stoi(argv[2]));
    }

    std::cout << "bench_server: http://0.0.0.0:" << port << " (" << threads
              << " io threads) — Ctrl+C for graceful shutdown\n";

    try {
        http::run_standalone(http::ServerConfig::Builder{}.finalize(), threads, [&](http::Server& server) {
            server.add_tcp_listener("0.0.0.0", port, http::Http11Driver{http::Http11Config{}});
            server.add_controller(std::make_shared<BenchController>());
        });
    } catch (const std::exception& e) {
        std::cerr << "bench_server: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Register the target**

Append to `benchmarks/http/CMakeLists.txt`:

```cmake
##############################################################################
# The server under load — new-API counterpart the bomber points at.
##############################################################################
add_executable(${DMP_BENCHMARKS}.Http.BenchServer
        bench_server.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Http.BenchServer
        PRIVATE
        Demiplane::Component::Http
)
##############################################################################
```

- [ ] **Step 3: Build both targets**

Run: `cmake --build build/debug --target Demiplane.Benchmarks.Http.BenchServer -- -j4`
Expected: clean build.

- [ ] **Step 4: End-to-end smoke — bomber against bench server**

```bash
./build/debug/benchmarks/http/Demiplane.Benchmarks.Http.BenchServer 8098 2 &
SERVER_PID=$!
sleep 1
./build/debug/benchmarks/http/Demiplane.Benchmarks.Http.Bomber 127.0.0.1 8098 /ping 2 50 3
kill -INT ${SERVER_PID}
wait ${SERVER_PID}; echo "server exit: $?"
```

Expected: the bomber's final stats show `Successful` ≈ `Total Requests` (100% success against `/ping`), and the
server exits 0 after SIGINT (graceful shutdown). Also spot-check the parametric route while the server runs in a
second terminal session if desired: `/users/1` returns `user 1` (optional — the umbrella test and integration
suite already cover routing).

- [ ] **Step 5: Recommended commit grouping (user commits)**

```bash
git add benchmarks/http/
git commit -m "feat(bench): HTTP bench server on the redesigned API (PR 7)"
```

---

### Task 4: `examples/http/` — minimal server example

**Files:**

- Modify: `cmake/features.cmake`
- Modify: `CMakeLists.txt` (root — after the Benchmarks block)
- Create: `examples/CMakeLists.txt`
- Create: `examples/http/CMakeLists.txt`
- Create: `examples/http/minimal_http_server.cpp`
- Create: `examples/http/server.json`

**Interfaces:**

- Consumes: `Demiplane::Component::Http` (Task 1): `load_server_config`, `attach_default_listeners`,
  `run_standalone`, `HttpController`, `Middleware`/`NextHandler`, `AsyncOutcome` + `gears::err`,
  `Body::read_to_string`, `Headers::set`.
- Produces: executable `Demiplane.Examples.Http.MinimalServer`; the `BUILD_EXAMPLES` CMake option (default ON);
  the `examples/` top-level tree for future example apps.

- [ ] **Step 1: Add the BUILD_EXAMPLES option**

In `cmake/features.cmake`, after the `option(DO_BENCHMARKS ...)` line, add:

```cmake
option(BUILD_EXAMPLES "Build example apps" ON)
```

- [ ] **Step 2: Wire the examples tree into the root build**

In the root `CMakeLists.txt`, directly after the Benchmarks block (`if (DO_BENCHMARKS) ... endif ()` and its
closing `###...` line) and before the final `PrintLineSeparator()`, insert:

```cmake
##############################################################################
# Examples
##############################################################################
if (BUILD_EXAMPLES)
    message("Examples are enabled")
    add_subdirectory(examples)
endif ()
##############################################################################
```

- [ ] **Step 3: Create the examples root CMake**

Create `examples/CMakeLists.txt`:

```cmake
set(DMP_EXAMPLES ${PROJECT_NAME}.Examples)

if (BUILD_HTTP)
    message("Examples for HTTP will be built")
    add_subdirectory(http)
endif ()
```

- [ ] **Step 4: Create the example target CMake**

Create `examples/http/CMakeLists.txt`:

```cmake
##############################################################################
# Minimal HTTP server (spec §10.3 standalone path) — living documentation
# for the public <demiplane/http> API. Successor of the pre-redesign manual
# handler example (spec §12.1).
##############################################################################
add_executable(${DMP_EXAMPLES}.Http.MinimalServer
        minimal_http_server.cpp
)
target_link_libraries(${DMP_EXAMPLES}.Http.MinimalServer
        PRIVATE
        Demiplane::Component::Http
)
##############################################################################
```

- [ ] **Step 5: Write the example**

Create `examples/http/minimal_http_server.cpp`:

```cpp
/**
 * Minimal demiplane HTTP server (spec §10.3, standalone path).
 *
 * Two wiring paths:
 *   ./minimal_http_server                 → programmatic config, 127.0.0.1:8080
 *   ./minimal_http_server server.json     → JSON config via load_server_config
 *                                           + attach_default_listeners
 * Try:
 *   curl http://127.0.0.1:8080/api/hello/world
 *   curl http://127.0.0.1:8080/api/healthz
 *   curl -d 'payload' http://127.0.0.1:8080/api/echo
 *
 * Ctrl+C triggers graceful shutdown (accept loops cancelled, in-flight
 * requests drained, observers notified) — run_standalone owns the executor,
 * the worker threads, and the §9.7 stop → wait → teardown sequence.
 */
#include <demiplane/http>

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

    namespace http = demiplane::http;

    /// GET /hello/{name} — path parameter, URL-decoded into the request arena.
    /// GET /healthz      — arena-backed JSON response.
    /// POST /echo        — typed-error handler: AsyncOutcome + ADL to_http_response.
    class GreeterController final : public http::HttpController {
    public:
        void configure_routes() override {
            Get("/hello/{name}", &GreeterController::hello);
            Get("/healthz", &GreeterController::healthz);
            Post("/echo", &GreeterController::echo);
        }

    private:
        http::AsyncResponse hello(http::RequestContext ctx) {
            co_return ctx.ok("hello, " + ctx.path_param_or<std::string>("name", std::string{"world"}) + "\n");
        }

        http::AsyncResponse healthz(http::RequestContext ctx) {
            co_return ctx.json(R"({"status":"ok"})");
        }

        /// Oversize bodies short-circuit as BodyLimitExceeded; the bake layer
        /// converts the typed error into a 413 via errors.hpp's ADL
        /// to_http_response — the handler never builds an error response.
        http::AsyncOutcome<http::Response, http::BodyLimitExceeded> echo(http::RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(64 * 1024);
            if (body.is_error()) {
                co_return demiplane::gears::err(body.error<http::BodyLimitExceeded>());
            }
            co_return ctx.ok(std::move(body).value());
        }
    };

    /// Post-processing middleware: stamps a header on every response of the
    /// controller it is attached to (runs after the handler returns).
    http::Middleware server_tag_middleware() {
        return [](http::RequestContext ctx, const http::NextHandler& next) -> http::AsyncResponse {
            auto response = co_await next(std::move(ctx));
            response.headers.set("X-Example", "minimal-http-server");
            co_return response;
        };
    }

    /// Report whichever config error alternative the Outcome holds.
    [[nodiscard]] int report_config_error(
        const demiplane::gears::Outcome<http::ServerConfig, http::ConfigFileError, http::ConfigParseError,
                                        http::ConfigSchemaError>& loaded) {
        if (loaded.holds_error<http::ConfigFileError>()) {
            const auto& e = loaded.error<http::ConfigFileError>();
            std::cerr << "config: cannot read " << e.path << ": " << e.reason << '\n';
        } else if (loaded.holds_error<http::ConfigParseError>()) {
            const auto& e = loaded.error<http::ConfigParseError>();
            std::cerr << "config: parse error at " << e.path << ':' << e.line << ": " << e.detail << '\n';
        } else {
            const auto& e = loaded.error<http::ConfigSchemaError>();
            std::cerr << "config: invalid value in " << e.path << ": " << e.detail << '\n';
        }
        return 1;
    }

}  // namespace

int main(const int argc, char* argv[]) {
    namespace http = demiplane::http;

    http::ServerConfig cfg = http::ServerConfig::Builder{}.finalize();
    if (argc > 1) {
        auto loaded = http::load_server_config(argv[1]);
        if (loaded.is_error()) {
            return report_config_error(loaded);
        }
        cfg = std::move(loaded).value();
    }

    const std::size_t threads = cfg.threads();
    std::cout << "minimal_http_server: " << threads << " io threads — Ctrl+C for graceful shutdown\n";

    try {
        http::run_standalone(std::move(cfg), threads, [](http::Server& server) {
            // JSON-declared listeners (no-op when the config has none) …
            http::attach_default_listeners(server);
            // … falling back to a programmatic one so both paths just work.
            if (server.listeners().empty()) {
                server.add_tcp_listener("127.0.0.1", 8080, http::Http11Driver{http::Http11Config{}});
            }

            auto greeter = std::make_shared<GreeterController>();
            greeter->add_middleware(server_tag_middleware());
            server.in_group("/api").add_controller(std::move(greeter));
        });
    } catch (const std::exception& e) {
        std::cerr << "minimal_http_server: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
```

- [ ] **Step 6: Write the sample config**

Create `examples/http/server.json`:

```json
{
    "listeners": [
        {
            "bind": "127.0.0.1",
            "port": 8080,
            "transport": "tcp",
            "protocols": ["http1"]
        }
    ],
    "threads": 2,
    "timeouts": {
        "header_ms": 10000,
        "body_ms": 30000,
        "idle_ms": 60000
    },
    "body_limit": 1048576,
    "request_arena_size": 8192,
    "drain_timeout_ms": 5000,
    "path_normalization": "collapse_trailing_slash"
}
```

- [ ] **Step 7: Build**

Run: `cmake --build build/debug --target Demiplane.Examples.Http.MinimalServer -- -j4`
Expected: CMake reconfigures (new option + subdirectory), then a clean build.

- [ ] **Step 8: Run both wiring paths against the wire**

Programmatic path:

```bash
./build/debug/examples/http/Demiplane.Examples.Http.MinimalServer &
EX_PID=$!
sleep 1
curl -sS --max-time 5 http://127.0.0.1:8080/api/hello/world
curl -sS --max-time 5 http://127.0.0.1:8080/api/healthz
curl -sS --max-time 5 -d 'payload' http://127.0.0.1:8080/api/echo
curl -sSI --max-time 5 http://127.0.0.1:8080/api/healthz | grep -i x-example
kill -INT ${EX_PID}; wait ${EX_PID}; echo "exit: $?"
```

Expected: `hello, world`, `{"status":"ok"}`, `payload`, an `X-Example: minimal-http-server` header line, exit 0.

JSON path:

```bash
./build/debug/examples/http/Demiplane.Examples.Http.MinimalServer examples/http/server.json &
EX_PID=$!
sleep 1
curl -sS --max-time 5 http://127.0.0.1:8080/api/healthz
kill -INT ${EX_PID}; wait ${EX_PID}; echo "exit: $?"
```

Expected: `{"status":"ok"}`, exit 0. Also verify the error path reports cleanly:
`./build/debug/examples/http/Demiplane.Examples.Http.MinimalServer /nonexistent.json` → exit 1 with
`config: cannot read /nonexistent.json: ...`.

- [ ] **Step 9: Recommended commit grouping (user commits)**

```bash
git add cmake/features.cmake CMakeLists.txt examples/
git commit -m "feat(examples): minimal HTTP server example (spec §12.1 examples/http, PR 7)"
```

---

### Task 5: Spec reconciliation

**Files:**

- Modify: `docs/superpowers/specs/2026-05-07-http-redesign-design.md`

**Interfaces:**

- Consumes: the landed Tasks 1–4 surface.
- Produces: a spec whose every claim matches the tree — the redesign's closing bookkeeping. Targeted edits only;
  do not restructure sections.

- [ ] **Step 1: Update the Status header (lines 5-8)**

Replace the `**Status:**` value with:

```markdown
**Status:** COMPLETE — all 7 PRs landed. PR 7 (umbrella + cleanup) plan:
`docs/superpowers/plans/2026-07-09-http-cleanup-umbrella.md`; earlier per-PR plans under `docs/superpowers/plans/`.
Reviewed + reconciled 2026-06-09; final reconciliation 2026-07-09.
```

(Keep `**Date:**`, `**Component:**` as-is; update `**Branch:**` to `component/http-1.1/v1.7`.)

- [ ] **Step 2: Fix §4 directory tree (firewall drop, D1)**

In the §4 tree, delete the line:

```
│  └─ firewall/           {rate_limit.hpp, ip_rule.hpp}   # data types only
```

and its trailing-comma sibling adjustment (the `router/` line above it becomes the last `routing/` entry). Also
delete the §10.1 paragraph sentence starting "`firewall.hpp` types (`rate_limit`, `ip_rule`) move to
`routing/firewall/` ..." and replace it with:

```markdown
The old `firewall_config.hpp` data types (`rate_limit`, `ip_rule`) are dropped in v1 (PR 7 D1): they were deleted
with the legacy config, nothing consumes them, and they return only alongside a real rate-limit middleware.
```

- [ ] **Step 3: Update the §12.1 migration table rows that drifted**

- `components/http/config/include/firewall_config.hpp` row → disposition:
  `Deleted (pre-PR 1). NOT moved to routing/firewall/ — dropped in v1 (PR 7 D1); returns with a real rate-limit middleware.`
- `tests/manual_tests/http/handler/manual_example_http_handler.cpp` row → disposition:
  `Deleted (pre-PR 1). Behaviour covered by tests/integration_tests/http/ (PRs 4-5); example pattern lives at examples/http/minimal_http_server.cpp (PR 7).`
- `benchmarks/http/*` row → disposition:
  `Deleted (pre-PR 1); restored in PR 7 — http_bomber.cpp verbatim (logic intact) + new-API bench_server.cpp target.`

- [ ] **Step 4: Update §12.2 PR 7 row + closing line**

Replace the PR 7 scope cell with:

```markdown
**Umbrella + cleanup** — populate the `Demiplane::Component::Http` combined library + `<demiplane/http>` umbrella
header (all 7 layers), umbrella smoke test, restore `http_bomber` + add `bench_server`, create `examples/http/`,
final spec reconciliation. (The old-code deletions this row originally listed landed pre-PR 1 with d3c40ef/150b666.)
```

Replace the paragraph below the table ("Each PR builds and tests cleanly. The old code coexists ... PR 7 is the
cleanup.") with:

```markdown
Each PR builds and tests cleanly. The old code was deleted just before PR 1 (d3c40ef) rather than coexisting
through PRs 1-6 as originally sketched; PR 7 closed the redesign by aggregating the public umbrella and restoring
the benchmark/example surface.
```

- [ ] **Step 5: Fix the stale §13 comment**

In the §13 CMake skeleton, replace the line:

```cmake
# Tests link this alias until the umbrella aggregates Types (PR 7).
add_library(Demiplane::Component::Http::Types ALIAS ${DMP_HTTP}.Types)
```

with:

```cmake
# Landed shape (PR 7): tests keep per-layer dotted links (D4) — no per-layer
# :: aliases exist; umbrella propagation is guarded by a dedicated smoke test.
```

- [ ] **Step 6: Append PR 7 rows to the §15 Decisions Log table**

```markdown
| Firewall data types (PR 7 D1) | Dropped from v1 — `routing/firewall/` not created | Deleted with the legacy config pre-PR 1; zero consumers; dead data types are YAGNI — they return with an actual rate-limit middleware |
| HTTP benchmarks (PR 7 D2) | `http_bomber.cpp` restored verbatim from `150b666~1` + new `bench_server` on the redesigned API | §12.1 required "benchmark logic intact"; the bomber is a pure Beast client, and the bench server gives the pair an out-of-the-box target |
| Examples tree (PR 7 D3) | New top-level `examples/` (`BUILD_EXAMPLES`, default ON); `examples/http/minimal_http_server.cpp` shows both wiring paths (programmatic + `server.json`) | §12.1 promised the example pattern under `examples/http/`; living documentation of the §10.3 standalone path |
| Test-link policy (PR 7 D4) | Existing tests keep per-layer dotted links; a dedicated `...Http.Umbrella` unit target links ONLY the combined lib | Per-layer links catch under-linking regressions in layer CMake; the smoke target isolates umbrella-propagation failures |
```

- [ ] **Step 7: Verify no stale spec claims remain**

Run:
`grep -n "until PR 7\|PR 7 is the cleanup\|stays untouched until PR 7" docs/superpowers/specs/2026-05-07-http-redesign-design.md`
Expected: no matches. (Historical plan documents under `docs/superpowers/plans/` are point-in-time records — do
NOT edit them.)

- [ ] **Step 8: Recommended commit grouping (user commits)**

```bash
git add docs/superpowers/specs/2026-05-07-http-redesign-design.md
git commit -m "docs(http): final spec reconciliation — redesign complete (PR 7)"
```

---

### Task 6: Final verification sweep

**Files:** none (verification only).

**Interfaces:**

- Consumes: everything above.
- Produces: the evidence backing the PR description. Do not claim success without these outputs
  (superpowers:verification-before-completion).

- [ ] **Step 1: Full debug tree builds clean**

Run: `cmake --build build/debug -- -j$(nproc)`
Expected: zero errors, zero warnings (the tree is -Werror; this also proves benchmarks + examples + all tests
compile together).

- [ ] **Step 2: Full HTTP test battery (unit + integration)**

Run: `ctest --test-dir build/debug --output-on-failure -R "Http"`
Expected: all `Demiplane.Tests.Unit.Http.*` (incl. `.Umbrella`) and `Demiplane.Tests.Integration.Http.*` PASS.

- [ ] **Step 3: Non-HTTP regression spot-check**

Run: `ctest --test-dir build/debug --output-on-failure -R "Serialization|Scroll"`
Expected: PASS (nothing in this PR touches them; this guards the root-CMake edit).

- [ ] **Step 4: ASan run of the HTTP suites**

```bash
cmake --preset asan            # if build/asan is not already configured
cmake --build build/asan -- -j$(nproc)
ctest --test-dir build/asan --output-on-failure -R "Http"
```

Expected: PASS, no ASan reports. (Skip TSan — the 3 pre-existing Nexus ctor/dtor races fire on any component's
first COMPONENT_LOG and are not HTTP regressions.)

- [ ] **Step 5: Fresh-configure sanity (options really default ON)**

```bash
cmake --preset debug -B /tmp/dmp-pr7-fresh-configure >/tmp/dmp-pr7-configure.log 2>&1 || true
grep -E "Examples are enabled|Benchmarks for HTTP will be built" /tmp/dmp-pr7-configure.log
rm -rf /tmp/dmp-pr7-fresh-configure
```

Expected: both lines present. (If the preset hard-codes its binaryDir and rejects `-B`, instead verify the two
lines in build/debug's last configure output — `cmake --preset debug` — and note it.)

- [ ] **Step 6: Hand off for PR**

Use superpowers:finishing-a-development-branch. Suggested PR title: `Http revision: umbrella + cleanup (PR 7/7)`.
The PR body should state: umbrella aggregation + smoke test, bomber restored + bench server added, `examples/http/`
created, firewall types dropped (D1), spec reconciled — redesign complete.
