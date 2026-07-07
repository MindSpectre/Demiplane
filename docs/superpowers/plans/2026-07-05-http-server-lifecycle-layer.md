# HTTP Redesign — PR 5: Server + Observers + Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the `Server` orchestrator that makes the landed PR 1–4 layers a *product*: executor injection
(`Server(ServerConfig, any_io_executor)`, owns no threads), `setup()`/`stop()`/`wait_until_stopped()`/
`async_wait_stopped()`, the six-phase `graceful_shutdown()` honoring the §9.7 shutdown-ordering contract, the
`ServerObserver` interface (lifecycle + per-request hooks), the `run_standalone(cfg, threads, configure)` convenience,
and the §14.2 lifecycle/observer/concurrency integration-test battery — plus resolution of every `TODO(PR5)` marker in
the codebase. Spec: §9 + §12.2 PR 5 of `docs/superpowers/specs/2026-05-07-http-redesign-design.md`.

**Architecture:** `Server` is a thin orchestrator over the landed layers: it owns the `RouteRegistry` + `Router`
(routing), a `std::vector<std::unique_ptr<ListenerBase>>` (transport), the controller and observer lists — and is
*handed* an executor it never creates, runs, or stops. `setup()` freezes routes (aggregating conflicts into one throw),
initializes controllers, binds listeners synchronously, and `co_spawn`s each listener's accept loop **onto its own
strand** with **its own** `cancellation_signal`. `stop()` CASes `running → stopping` and detach-spawns
`graceful_shutdown()`, which (1) dispatches each stop-emit onto the matching run-strand, (1.5) awaits accept-loop
completion (acceptors provably closed), (2) drains in-flight connections to a deadline, (2.5) polls `in_flight() == 0`
(the force-cancel unwind barrier from the `TODO(PR5)` warnings), (3–5) notifies observers/controllers, and (6) flips
`shutdown_complete_` under the cv the blocking `wait_until_stopped()` waits on. Per-request observer hooks fire from
`Router::dispatch` through **nullable `std::function` hooks** the Server wires at `setup()` — no observer interface
below the server layer, no dynamic-inheritance split, routing keeps zero knowledge of the server layer.

**Tech Stack:** C++23 (coroutines, concepts, `std::atomic`/CAS, `condition_variable`), Boost.Asio (`any_io_executor`,
`make_strand`, `co_spawn`, `bind_cancellation_slot`, `cancellation_signal`, `steady_timer`, `signal_set`,
`executor_work_guard`), the landed PR 1–4 dotted leaf targets, GoogleTest (unit + `add_integration_test` on real
`127.0.0.1:0` sockets via the PR 4 Beast client fixture).

## Global Constraints

- Verify every task with the **`debug`** preset: `cmake --build build/debug --target <target> -- -j4` clean, then
  `ctest --test-dir build/debug --output-on-failure -R <pattern>`. Sanitizer steps use `tsan`/`asan` *if they
  configure*; otherwise fall back to `debug` (PR 4 note).
- **No git commits by the executing agent** — the user manages git. Per-task `git` blocks below are the recommended
  grouping for when the user commits. **Never add a Claude/Co-Authored-By signature.**
- Dotted CMake target names only (`Demiplane.Component.HTTP.*`); **no `::` aliases**; the public umbrella
  (`add_combined_library` in `components/http/CMakeLists.txt`) stays untouched until PR 7.
- The zero-additional-allocation invariant (spec §11) extends to this PR's hot-path additions: invoking the Router
  observer hooks must add **zero** global-heap allocations per request (gated in Task 3).
- Concept names are `Is*` (landed convention: `IsHttpDriver`, `IsStreamConnection`). 4-space indent, `/** */` doc
  comments, `[[nodiscard]]` on accessors — match the landed HTTP sources.
- C++23 **headers**, not modules (project decision). Cross-leaf includes use the rooted form (`<router.hpp>`,
  `<server.hpp>`), never relative paths.
- Integration tests bind `127.0.0.1:0`, use real Beast clients, no mocks (project testing philosophy).

---

## Reconciliation against the landed code + spec (read before executing)

Spec §9 predates the landed listener layer and the PR 4 deviations. These are the facts on the ground and the
deviations this plan deliberately takes. D2/D3/D5/D6 were settled with the user on 2026-07-05.

### Landed facts (authoritative over the spec)

1. **`ListenerBase`** (`<listener_base.hpp>`): `bind()` (sync, throws), `awaitable<void> run(Router&)`,
   `awaitable<void> drain_until(steady_clock::time_point)`, **`std::size_t in_flight() const noexcept`** (not in the
   spec sketch — the unwind barrier), `bind_address()`, `bound_port()`. Base class `gears::Immutable`.
2. **Listener ctors** take `(any_io_executor, std::string host, std::uint16_t port, …)` — *not* the spec's single
   `std::string bind`: `TcpListener<Driver>{exec, host, port, driver, arena_size = 8192}`,
   `TlsListener<Drivers...>{exec, host, port, TlsConfig, drivers...}` (arena size fixed at 8 KB in v1, PR 6 wires it),
   `QuicListener<Driver>{exec, host, port, TlsConfig, driver}` (scaffold: `bind()` no-op, `run()` logs + returns,
   `in_flight()` = 0).
3. **`run()` cancellation is state, not exceptions**: the accept loops latch `this_coro::cancellation_state`, break on
   `operation_aborted`, and close the acceptor on *every* exit path (new SYNs refused). The stop signal must be emitted
   with `cancellation_type::terminal`. A `cancellation_slot` holds at most **one** handler → one
   `cancellation_signal` per listener (spec §7.2/§9.3). `cancellation_signal` is immovable → held via `unique_ptr`.
4. **`ConnectionTracker::drain_until(ex, deadline)` only DISPATCHES force-cancels** — the cancelled `serve()`
   coroutines unwind in later executor turns. Callers MUST wait for `in_flight() == 0` before destroying the listener
   (the `TODO(PR5)` markers in `tcp_listener.hpp:43`, `tls_listener.hpp:49`, `connection_tracker.cpp:46`). The PR 4
   integration fixture proves the polling pattern in `TearDown()`.
5. **Multi-worker residual** (`tcp_listener.hpp:83`, `tls_listener.hpp:88`): a stop-emit landing between the accept
   loop's state check and `async_accept` installing its cancel handler is edge-lost — *unless* the emit is serialized
   with the loop's executor turns. The loop body runs check→accept-install in one turn, so spawning `run()` on a
   strand and dispatching the emit onto that strand closes the window.
6. **`Router`** (`<router.hpp>`): `explicit Router(const RouteRegistry&) noexcept`,
   `awaitable<Response> dispatch(RequestContext ctx) const` — routing misses collapse to 404/405 `Response` via ADL
   `to_http_response`; handler exceptions currently propagate to the driver's catch-all → 500 (`http11_driver.hpp`,
   `serve()`); path params injected via `ctx.set_path_param(name, value)`.
7. **`RequestContext` is move-construct-only** (`request_context.hpp:34-41`): move-assign deleted, consumed **by
   value** by the handler chain. After `co_await handler(std::move(ctx))` returns there is **no valid context**.
   Accessors: `method()`, `target()`, `path()`, `arena_alloc()`, ctx-factories `ok/json/created/status(...)`.
8. **`RouteRegistry`**: ctor takes `PathNormalization` (routing-layer enum, `route_registry.hpp:24`); `add_route`
   throws `std::logic_error` after `freeze()`; `freeze()` returns `std::vector<RouteConflictError>`
   (`{HttpMethod method; std::string path; std::string detail;}`). `GroupBinding{registry, controller_sink, prefix}`
   runs the bake; `HttpController` has virtual `initialize()`/`shutdown()` hooks ("the Server wires them in PR 5" —
   `controller.hpp:113-117`).
9. **`Http11Driver`** has **no observer knowledge** and stays untouched: `serve(conn, router)` catches handler
   escapes and synthesizes the 500 itself. Enums: `to_string(HttpMethod)` exists (`http_enums.hpp:66`);
   `HttpStatus`/`HttpMethod`/`HTTP_METHOD_COUNT` as landed.
10. **Config layer**: `TlsConfig` is a **plain struct** at `config/tls_config/tls_config.hpp` (PR 4 D1: PR 6 rewrites
    it in place into `ConfigInterface`). `${DMP_HTTP}.Config` is an INTERFACE aggregate.
11. **CMake per-leaf convention**: each "thing" is a leaf target owning its include dir
    (`add_library(<leaf> STATIC x.cpp)` + `target_include_directories(PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})`;
    header-only leaves are `INTERFACE`); an INTERFACE aggregate per layer. `${DMP_HTTP}` = `Demiplane.Component.HTTP`.
12. **Test harness**: `add_unit_test(<target> <sources…>)` + separate `target_link_libraries(… ${TEST_LIBS})`;
    `add_integration_test(<target> <sources…> LINK_LIBS … LABELS "http")`. Existing binaries:
    `Demiplane.Tests.Unit.Http.{Routing,Types,Connection,Drivers,Listeners}`,
    `Demiplane.Tests.Integration.Http.{Tcp,Tls}`. The PR 4 fixture (
    `tests/integration_tests/http/http_test_fixture.hpp`)
    provides `HttpIntegrationFixture` (hand-wired registry+listener — kept as-is for listener-level tests) and
    **`TcpClient`** (sync Beast client: `send(verb, target, body, ct, keep_alive)`, `write_request(...)`,
    `read_after_close()`), which the new Server tests reuse.
13. **Scroll/Nexus**: `SCROLL_COMPONENT_PREFIX("Name")` at class scope + `COMPONENT_LOG_INF/WRN()` (link
    `Demiplane::Common::Scroll`); `NEXUS_REGISTER(nexus::Immortal)` is just
    `static constexpr nexus::Immortal nexus_policy{}` (link `Demiplane::Common::Nexus`, include `<demiplane/nexus>`).
14. **All `TODO(PR5)` markers** (complete list, all resolved by this plan):
    `components/http/listeners/tcp_listener/tcp_listener.hpp:43,83`,
    `components/http/listeners/tls_listener/tls_listener.hpp:49,88`,
    `components/http/listeners/connection_tracker/connection_tracker.cpp:46`,
    `tests/unit_tests/http/listeners/test_tcp_listener.cpp:16` (EADDRINUSE test),
    `tests/integration_tests/http/test_http_tcp.cpp:17` (verb/param/decode battery).

### Deviations taken (each documented in the relevant task)

- **D1 — Plain-struct `ServerConfig` at the spec's final path now; PR 6 rewrites it in place** (PR 4 D1 precedent).
  Carries exactly what PR 5 consumes: `request_arena_size`, `drain_timeout`, `path_normalization` (its **own** nested
  enum, mapped by the Server onto routing's `PathNormalization` so the config layer keeps zero routing dependency —
  exactly the staging `route_registry.hpp:23` anticipates). `listeners`/`threads`/`timeouts`/`body_limit` are PR 6.
- **D2 — Per-request observer hooks fire in `Router::dispatch` via nullable `std::function` hooks** (user decision
  2026-07-05: no dynamic-inheritance split, no observer interface below the server layer). `Router` gains a
  `Hooks{on_request, on_response, on_unhandled_exception}` member set once by `Server::setup()` (fan-out lambdas over
  the observer list). Empty hooks = zero overhead beyond a null check. Routing gains **no** dependency on the server
  layer. Consequences (documented in code + spec sync): driver-level early responses (malformed 400, header/body-limit
  4xx) and the driver-synthesized 500 body are **not** seen by `on_response`; handler throws **are** seen by
  `on_unhandled_exception` (Router notifies, then rethrows so the driver still writes the 500).
- **D3 — `on_response` takes a `RequestInfo` snapshot** (user decision 2026-07-05). Spec §9.2's
  `on_response(const RequestContext&, const Response&)` is unimplementable: the context is consumed by value by the
  handler chain (landed fact 7). `RequestInfo{HttpMethod method; std::string_view target;}` is snapshotted at dispatch
  entry; its views stay valid through the hook call (the arena resets only at the next keep-alive iteration).
- **D4 — `setup()` never blocks; `on_setup_complete` observers are notified via a detach-spawned coroutine.** Spec
  §9.1 ("does NOT block") contradicts §9.3 step 6 ("awaited as a barrier on exec_"). Blocking would also deadlock any
  caller that runs the executor only after `setup()` (`run_standalone` included). Resolution: `setup()` spawns one
  coroutine on `exec_` that awaits each observer's `on_setup_complete()` sequentially in add order; exceptions fan to
  `on_unhandled_exception`.
- **D5 — Multi-threaded executors are supported: one strand per listener `run()`, stop-emits dispatched onto that
  strand** (user decision 2026-07-05). Closes the landed-fact-5 edge-lost window; the §14.2 concurrency test runs 4 io
  threads. The `TODO(PR5)` comments are rewritten accordingly (Task 11).
- **D6 — Drain completion by polling** (user decision 2026-07-05): `graceful_shutdown()` polls
  `live_accept_loops_ == 0` after the emits (phase 1.5, acceptors provably closed → "new connections refused" is
  testable) and `Σ in_flight() == 0` after the drain (phase 2.5, the unwind barrier) with a 5 ms `steady_timer` tick —
  the PR 4 fixture's proven pattern, no new tracker surface. The phase-2.5 poll is deliberately **unbounded**: a
  suspended coroutine frame cannot be safely freed except by running it to completion, so a handler that ignores
  cancellation delays shutdown rather than corrupting it (logged WRN).
- **D7 — `add_*_listener` take `(std::string host, std::uint16_t port, …)`**, matching the landed listener ctors, not
  the spec's single `std::string bind` (no parsing layer in v1; `attach_default_listeners` in PR 6 is the
  config-driven path).
- **D8 — `Server::setup()` calls `controller->initialize()`** (add order, after freeze, before bind; failures
  propagate and abort setup). Spec §9.3's list omits it but `controller.hpp` promises the wiring in PR 5;
  `graceful_shutdown()` phase 4 already calls `shutdown()` reverse-order per spec §9.5.
- **D9 — `RouteConflictAggregateError` lives in `routing/route_registry/`** next to `RouteConflictError` (the data it
  aggregates), not in the server layer — any registry consumer can use it; the Server throws it from `setup()`.

## File Structure

```
components/http/
├─ config/
│  ├─ server_config/               NEW {server_config.hpp, CMakeLists.txt}          (Task 1)
│  └─ CMakeLists.txt               MOD add leaf + aggregate link                     (Task 1)
├─ routing/
│  ├─ route_registry/route_registry.hpp  MOD +RouteConflictAggregateError            (Task 2)
│  ├─ route_registry/route_registry.cpp  MOD +format_message                         (Task 2)
│  ├─ router/router.hpp            MOD +RequestInfo, +Router::Hooks, +set_hooks      (Task 3)
│  └─ router/router.cpp            MOD dispatch fires hooks, try/catch+rethrow       (Task 3)
├─ server/                         NEW layer
│  ├─ CMakeLists.txt               NEW aggregate (leaves added per task)             (Task 4)
│  ├─ server_observer/             NEW {server_observer.hpp, CMakeLists.txt}         (Task 4)
│  ├─ server/                      NEW {server.hpp, server.cpp, CMakeLists.txt}      (Task 5)
│  └─ run_standalone/              NEW {run_standalone.hpp, .cpp, CMakeLists.txt}    (Task 8)
├─ listeners/
│  ├─ tcp_listener/tcp_listener.hpp        MOD resolve TODO(PR5) comments            (Task 11)
│  ├─ tls_listener/tls_listener.hpp        MOD resolve TODO(PR5) comments            (Task 11)
│  └─ connection_tracker/connection_tracker.cpp  MOD resolve TODO(PR5) comment       (Task 11)
└─ CMakeLists.txt                  MOD add_subdirectory(server)                      (Task 4)

tests/unit_tests/http/
├─ routing/test_route_registry.cpp        MOD +aggregate-error test                  (Task 2)
├─ routing/test_router.cpp                MOD +hook tests                            (Task 3)
├─ routing/test_routing_allocation_gate.cpp  MOD +hook-invocation gate               (Task 3)
└─ listeners/test_tcp_listener.cpp        MOD remove TODO(PR5) line                  (Task 6)

tests/integration_tests/http/
├─ server_test_fixture.hpp         NEW ServerIntegrationFixture + test controllers   (Task 5)
├─ test_http_server_lifecycle.cpp  NEW  §14.2 lifecycle battery                      (Tasks 5,6)
├─ test_http_server_observer.cpp   NEW  §14.2 observer battery                       (Task 7)
├─ test_http_run_standalone.cpp    NEW  SIGINT / standalone battery                  (Task 8)
├─ test_http_server_concurrency.cpp NEW 1000 reqs × 4 io workers                     (Task 9)
├─ http_test_fixture.hpp           MOD +TcpClient::read_response/send_head           (Tasks 6,10)
├─ test_http_tcp.cpp               MOD verb/param/decode battery, drop TODO          (Task 10)
└─ CMakeLists.txt                  MOD +Http.Server integration target               (Task 5)

docs/superpowers/specs/2026-05-07-http-redesign-design.md  MOD spec sync             (Task 12)
```

Dependency order: Task 1 (config) and Task 2 (aggregate error) are independent; Task 3 (hooks) precedes Task 4
(observer header uses `RequestInfo`); Task 5 (Server) needs 1–4; Tasks 6–9 extend Task 5's test target; Task 10 is
independent of the server; Tasks 11–12 are documentation passes at the end.

---

### Task 1: `ServerConfig` plain struct (config leaf)

**Files:**

- Create: `components/http/config/server_config/server_config.hpp`
- Create: `components/http/config/server_config/CMakeLists.txt`
- Modify: `components/http/config/CMakeLists.txt`

**Interfaces:**

- Consumes: nothing (self-contained value type).
- Produces: `demiplane::http::ServerConfig` — `std::size_t request_arena_size` (default `8192`),
  `std::chrono::milliseconds drain_timeout` (default 30 s), nested
  `enum class PathNormalization : std::uint8_t { none, collapse_trailing_slash, collapse_multi_slash }` +
  `PathNormalization path_normalization` (default `collapse_trailing_slash`). CMake leaf
  `${DMP_HTTP}.Config.ServerConfig` (INTERFACE). Consumed by Task 5's `Server` and Task 8's `run_standalone`.

- [ ] **Step 1: Write the header**

`components/http/config/server_config/server_config.hpp`:

```cpp
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace demiplane::http {

    /**
     * @brief Server-level tuning consumed by the Server (spec §10.1 subset).
     *
     * PR 5 ships this as a PLAIN STRUCT (PR 4 D1 precedent: TlsConfig) — the
     * serialization::ConfigInterface version (fields()/Builder/validate(),
     * plus listeners/threads/timeouts/body_limit) lands in PR 6, which
     * rewrites THIS file in place. Kept at the spec's final path so the
     * include is stable.
     */
    struct ServerConfig {
        /// Mirrors routing's PathNormalization (route_registry.hpp). The
        /// Server maps this config enum onto the routing enum so the config
        /// layer carries no routing dependency (route_registry.hpp:23 staging).
        enum class PathNormalization : std::uint8_t {
            none,                     ///< exact byte match
            collapse_trailing_slash,  ///< "/users/" == "/users"   (default)
            collapse_multi_slash,     ///< + "/users//42" == "/users/42"
        };

        /// Per-connection request arena block (spec §6.1); forwarded to
        /// TcpListener. TlsListener stays at its fixed 8 KB default until
        /// PR 6 (PR 4 note in tls_listener.hpp).
        std::size_t request_arena_size = 8192;

        /// Budget for graceful_shutdown()'s drain phase (spec §9.5 phase 2):
        /// in-flight requests get this long to finish before force-cancel.
        std::chrono::milliseconds drain_timeout{std::chrono::seconds{30}};

        PathNormalization path_normalization = PathNormalization::collapse_trailing_slash;
    };

}  // namespace demiplane::http
```

- [ ] **Step 2: Write the CMake leaf**

`components/http/config/server_config/CMakeLists.txt`:

```cmake
##############################################################################
# Http Config — ServerConfig (plain struct; PR 6 promotes to ConfigInterface)
##############################################################################
add_library(${DMP_HTTP}.Config.ServerConfig INTERFACE server_config.hpp)

target_include_directories(${DMP_HTTP}.Config.ServerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
```

- [ ] **Step 3: Wire the leaf into the config aggregate**

In `components/http/config/CMakeLists.txt`, add the subdirectory after the `tls_config` one and the link entry inside
the aggregate:

```cmake
add_subdirectory(tls_config)
add_subdirectory(server_config)
```

and

```cmake
target_link_libraries(${DMP_HTTP}.Config INTERFACE
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Config.ServerConfig
)
```

- [ ] **Step 4: Verify the configure step**

Run: `cmake --preset debug 2>&1 | tail -3`
Expected: configure completes with no CMake errors. (Header-only leaf — full compile validation lands with Task 5's
first consumer; nothing else to run yet.)

- [ ] **Step 5: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add components/http/config/server_config components/http/config/CMakeLists.txt
# suggested message: "http/config: plain-struct ServerConfig for the PR5 Server (PR6 promotes in place)"
```

---

### Task 2: `RouteConflictAggregateError` (routing)

**Files:**

- Modify: `components/http/routing/route_registry/route_registry.hpp` (after the `RouteConflictError` struct,
  currently ending at line 36)
- Modify: `components/http/routing/route_registry/route_registry.cpp` (append `format_message`)
- Test: `tests/unit_tests/http/routing/test_route_registry.cpp` (append)

**Interfaces:**

- Consumes: `RouteConflictError{HttpMethod method; std::string path; std::string detail;}` and
  `to_string(HttpMethod)` from `<http_enums.hpp>` (already included by `route_registry.hpp`).
- Produces: `class RouteConflictAggregateError final : public std::runtime_error` with
  `explicit RouteConflictAggregateError(std::vector<RouteConflictError>)` and
  `const std::vector<RouteConflictError>& conflicts() const noexcept`. Thrown by `Server::setup()` (Task 5).

- [ ] **Step 1: Write the failing test**

Append to `tests/unit_tests/http/routing/test_route_registry.cpp`:

```cpp
TEST(RouteConflictAggregateErrorTest, AggregatesEveryConflictInWhat) {
    std::vector<RouteConflictError> conflicts{
        {HttpMethod::get, "/users", "duplicate registration"},
        {HttpMethod::post, "/orders", "duplicate registration"},
    };
    const RouteConflictAggregateError err{std::move(conflicts)};

    ASSERT_EQ(err.conflicts().size(), 2u);
    const std::string what = err.what();
    EXPECT_NE(what.find("2 route conflict(s)"), std::string::npos);
    EXPECT_NE(what.find("GET /users"), std::string::npos);
    EXPECT_NE(what.find("POST /orders"), std::string::npos);
    EXPECT_NE(what.find("duplicate registration"), std::string::npos);
}
```

(The file already does `using namespace demiplane::http;` — follow its existing test style.)

- [ ] **Step 2: Run it to make sure it fails**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5`
Expected: FAIL to compile — `RouteConflictAggregateError` not declared.

- [ ] **Step 3: Add the type to `route_registry.hpp`**

Add `#include <stdexcept>` to the header's include block, then insert directly after the `RouteConflictError` struct
(line 36):

```cpp
    /// Thrown by Server::setup() when freeze() reports conflicts: every
    /// duplicate (method, path) registration in ONE exception, so
    /// misconfigurations surface all at once, not piecemeal (spec §8.7).
    class RouteConflictAggregateError final : public std::runtime_error {
    public:
        explicit RouteConflictAggregateError(std::vector<RouteConflictError> conflicts)
            : std::runtime_error{format_message(conflicts)},
              conflicts_{std::move(conflicts)} {
        }

        [[nodiscard]] const std::vector<RouteConflictError>& conflicts() const noexcept {
            return conflicts_;
        }

    private:
        [[nodiscard]] static std::string format_message(const std::vector<RouteConflictError>& conflicts);

        std::vector<RouteConflictError> conflicts_;
    };
```

- [ ] **Step 4: Implement `format_message` in `route_registry.cpp`**

Append inside `namespace demiplane::http`:

```cpp
    std::string RouteConflictAggregateError::format_message(const std::vector<RouteConflictError>& conflicts) {
        std::string msg = std::to_string(conflicts.size()) + " route conflict(s):";
        for (const auto& c : conflicts) {
            msg += "\n  ";
            msg += to_string(c.method);
            msg += ' ';
            msg += c.path;
            if (!c.detail.empty()) {
                msg += " — ";
                msg += c.detail;
            }
        }
        return msg;
    }
```

- [ ] **Step 5: Run the tests and make sure they pass**

Run:
`cmake --build build/debug --target Demiplane.Tests.Unit.Http.Routing -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Http.Routing" 2>&1 | tail -5`
Expected: PASS (all existing routing tests + the new one).

- [ ] **Step 6: Suggested commit grouping**

```bash
git add components/http/routing/route_registry tests/unit_tests/http/routing/test_route_registry.cpp
# suggested message: "http/routing: RouteConflictAggregateError — one throw carrying every freeze() conflict"
```

---

### Task 3: Router request-observation hooks + `RequestInfo` (routing)

**Files:**

- Modify: `components/http/routing/router/router.hpp` (full new content below)
- Modify: `components/http/routing/router/router.cpp` (full new content below)
- Test: `tests/unit_tests/http/routing/test_router.cpp` (append)
- Test: `tests/unit_tests/http/routing/test_routing_allocation_gate.cpp` (append)

**Interfaces:**

- Consumes: landed `Router`/`RouteRegistry`/`RequestContext`/`Response`, `to_http_response` (errors.hpp).
- Produces (used by Tasks 4–5):
    - `struct RequestInfo { HttpMethod method; std::string_view target; };` (in `<router.hpp>`)
    - `struct Router::Hooks { std::function<void(const RequestContext&)> on_request;`
      `std::function<void(const RequestInfo&, const Response&)> on_response;`
      `std::function<void(std::exception_ptr)> on_unhandled_exception; };`
    - `void Router::set_hooks(Hooks hooks)` — build phase only, called once by `Server::setup()`.
    - Dispatch semantics: `on_request(ctx)` at entry; `on_response(info, r)` for handler successes AND routing-miss
      404/405 responses; handler throw → `on_unhandled_exception(current_exception())` then **rethrow** (driver still
      produces the 500; `on_response` not fired for that request).

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit_tests/http/routing/test_router.cpp` (note: these `Router` instances are non-`const` because
`set_hooks` mutates; existing `const Router` tests stay untouched and prove the unhooked path):

```cpp
namespace {

    struct HookLog {
        std::vector<std::string> events;
        RequestInfo last_info{};
        HttpStatus last_status{};
    };

    Router::Hooks recording_hooks(HookLog& log) {
        return Router::Hooks{
            .on_request =
                [&log](const RequestContext& ctx) noexcept {
                    log.events.push_back("req:" + std::string{ctx.target()});
                },
            .on_response =
                [&log](const RequestInfo& info, const Response& r) noexcept {
                    log.events.emplace_back("res");
                    log.last_info   = info;
                    log.last_status = r.status;
                },
            .on_unhandled_exception =
                [&log](std::exception_ptr) noexcept { log.events.emplace_back("exc"); },
        };
    }

}  // namespace

TEST_F(RouterTest, HooksFireAroundSuccessfulDispatchInOrder) {
    Router router{registry_};
    HookLog log;
    router.set_hooks(recording_hooks(log));

    const Response r = run_awaitable(router.dispatch(make_ctx(HttpMethod::get, "/users/42")));

    EXPECT_EQ(r.status, HttpStatus::ok);
    ASSERT_EQ(log.events.size(), 2u);
    EXPECT_EQ(log.events[0], "req:/users/42");
    EXPECT_EQ(log.events[1], "res");
    EXPECT_EQ(log.last_info.method, HttpMethod::get);
    EXPECT_EQ(log.last_info.target, "/users/42");
    EXPECT_EQ(log.last_status, HttpStatus::ok);
}

TEST_F(RouterTest, HooksFireForRoutingMisses) {
    Router router{registry_};
    HookLog log;
    router.set_hooks(recording_hooks(log));

    const Response r = run_awaitable(router.dispatch(make_ctx(HttpMethod::get, "/missing")));

    EXPECT_EQ(r.status, HttpStatus::not_found);
    ASSERT_EQ(log.events.size(), 2u);
    EXPECT_EQ(log.events[0], "req:/missing");
    EXPECT_EQ(log.events[1], "res");
    EXPECT_EQ(log.last_status, HttpStatus::not_found);
}

TEST_F(RouterTest, ExceptionHookFiresAndExceptionStillPropagates) {
    Router router{registry_};
    HookLog log;
    router.set_hooks(recording_hooks(log));

    EXPECT_THROW(run_awaitable(router.dispatch(make_ctx(HttpMethod::get, "/boom"))), std::runtime_error);

    // on_response is NOT fired — the 500 for a handler escape is synthesized
    // by the DRIVER after the rethrow (spec §6.3), invisible to the Router.
    ASSERT_EQ(log.events.size(), 2u);
    EXPECT_EQ(log.events[0], "req:/boom");
    EXPECT_EQ(log.events[1], "exc");
}
```

Append to `tests/unit_tests/http/routing/test_routing_allocation_gate.cpp` — first extend its include block:

```cpp
#include <request_context.hpp>
#include <response.hpp>
#include <router.hpp>
```

then append the test:

```cpp
TEST(RoutingAllocationGateTest, ObserverHookInvocationIsAllocationFree) {
    // The Server-wired fan-out lambdas capture one pointer (fits std::function
    // SBO); INVOKING them per request must never touch the global heap.
    // set_hooks itself runs once at setup() — build phase, heap is fine there.
    std::size_t calls = 0;
    const Router::Hooks hooks{
        .on_request  = [p = &calls](const RequestContext&) noexcept { ++*p; },
        .on_response = [p = &calls](const RequestInfo&, const Response&) noexcept { ++*p; },
        .on_unhandled_exception = [p = &calls](std::exception_ptr) noexcept { ++*p; },
    };

    StackArena arena;
    Request req{Headers::owned(arena.alloc)};
    req.method = HttpMethod::get;
    req.target = "/ping";  // string literal — static storage, no alloc
    RequestContext ctx{std::move(req), arena.alloc};
    Response resp{arena.alloc};
    const RequestInfo info{ctx.method(), ctx.target()};

    ArmedRegion region;
    hooks.on_request(ctx);
    hooks.on_response(info, resp);
    const std::size_t allocs = region.finish();

    EXPECT_EQ(allocs, 0u) << "observer hook invocation touched the global heap";
    EXPECT_EQ(calls, 2u);
}
```

(`Request`/`Headers` are pulled in transitively via `<request_context.hpp>`; the gate file's `StackArena`/
`ArmedRegion` already exist above.)

- [ ] **Step 2: Run them to make sure they fail**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Routing -- -j4 2>&1 | tail -5`
Expected: FAIL to compile — `RequestInfo` / `Router::Hooks` / `set_hooks` not declared.

- [ ] **Step 3: Rewrite `router.hpp`**

Full new content of `components/http/routing/router/router.hpp`:

```cpp
#pragma once

#include <exception>
#include <functional>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <http_enums.hpp>
#include <request_context.hpp>
#include <response.hpp>
#include <route_registry.hpp>

namespace demiplane::http {

    /// Request identity snapshotted at dispatch entry for the on_response hook
    /// (D3): the RequestContext is CONSUMED by value by the handler chain, so
    /// it no longer exists when the response is available. `target` views
    /// connection-owned storage — valid through the hook call and the response
    /// write; the arena/buffer reset only at the next keep-alive iteration.
    struct RequestInfo {
        HttpMethod method{};
        std::string_view target;
    };

    /**
     * @brief Thin dispatch facade the protocol drivers call (spec §8.8).
     *
     * find_route + path-param injection + handler invocation. Routing misses
     * (404/405) and handler typed errors are already collapsed to Response by
     * the time dispatch returns; exceptions escape to the driver's catch-all
     * (PR 3). The registry must be frozen before the first dispatch; frozen
     * means immutable, so concurrent dispatch from N io threads is safe.
     *
     * Request-observation hooks (PR 5, D2): the Server wires fan-out lambdas
     * over its ServerObserver list at setup() — plain std::functions, so the
     * routing layer stays free of any server-layer dependency. Unset hooks
     * cost one null check. Hook contract:
     *  - on_request(ctx)        — dispatch entry, before routing;
     *  - on_response(info, r)   — handler successes AND routing-miss 404/405;
     *    NOT fired when the handler throws (the 500 is driver-synthesized);
     *  - on_unhandled_exception — handler escape; fired, then RETHROWN so the
     *    driver's catch-all still writes the 500.
     * Driver-level early responses (malformed 400, header/body-limit 4xx)
     * never reach the Router and are not observed.
     */
    class Router {
    public:
        struct Hooks {
            std::function<void(const RequestContext&)> on_request;
            std::function<void(const RequestInfo&, const Response&)> on_response;
            std::function<void(std::exception_ptr)> on_unhandled_exception;
        };

        explicit Router(const RouteRegistry& registry) noexcept
            : registry_{&registry} {
        }

        /// Build phase ONLY (single-threaded, before the accept loops spawn) —
        /// dispatch reads hooks_ unsynchronized from N io threads afterwards.
        void set_hooks(Hooks hooks) {
            hooks_ = std::move(hooks);
        }

        [[nodiscard]] boost::asio::awaitable<Response> dispatch(RequestContext ctx) const;

    private:
        const RouteRegistry* registry_;
        Hooks hooks_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 4: Rewrite `router.cpp`**

Full new content of `components/http/routing/router/router.cpp`:

```cpp
#include "router.hpp"

#include <utility>

#include <errors.hpp>

namespace demiplane::http {
    boost::asio::awaitable<Response> Router::dispatch(RequestContext ctx) const {
        if (hooks_.on_request)
            hooks_.on_request(ctx);
        const RequestInfo info{ctx.method(), ctx.target()};

        auto resolved = registry_->find_route(ctx.method(), ctx.path(), ctx.arena_alloc());
        if (!resolved) {
            Response r = std::move(resolved).visit([](ResolvedRoute&&) -> Response { std::unreachable(); },
                                                   []<typename E>(E&& e) -> Response { return to_http_response(e); });
            if (hooks_.on_response)
                hooks_.on_response(info, r);
            co_return r;
        }
        auto& [handler, path_params] = resolved.value();
        for (const auto& [name, value] : path_params)
            ctx.set_path_param(name, value);
        try {
            Response r = co_await (*handler)(std::move(ctx));
            if (hooks_.on_response)
                hooks_.on_response(info, r);
            co_return r;
        } catch (...) {
            if (hooks_.on_unhandled_exception)
                hooks_.on_unhandled_exception(std::current_exception());
            throw;  // driver catch-all converts to 500 (spec §6.3)
        }
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Run the tests and make sure they pass**

Run:
`cmake --build build/debug --target Demiplane.Tests.Unit.Http.Routing -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Http.Routing" 2>&1 | tail -6`
Expected: PASS — including every pre-existing router/gate test (proves the unhooked path and the zero-alloc find_route
gates are unbroken).

- [ ] **Step 6: Suggested commit grouping**

```bash
git add components/http/routing/router tests/unit_tests/http/routing/test_router.cpp tests/unit_tests/http/routing/test_routing_allocation_gate.cpp
# suggested message: "http/routing: request-observation hooks on Router (std::function, D2) + RequestInfo snapshot (D3)"
```

---

### Task 4: `ServerObserver` interface + server layer scaffold

**Files:**

- Create: `components/http/server/server_observer/server_observer.hpp`
- Create: `components/http/server/server_observer/CMakeLists.txt`
- Create: `components/http/server/CMakeLists.txt`
- Modify: `components/http/CMakeLists.txt` (add the `server` subdirectory)

**Interfaces:**

- Consumes: `RequestInfo` + `RequestContext`/`Response` (Task 3 / Types), `boost::asio::awaitable`.
- Produces: `class ServerObserver` — `virtual awaitable<void> on_setup_complete()`,
  `virtual awaitable<void> on_shutdown_started()`, `virtual void on_shutdown_complete() noexcept`,
  `virtual void on_request(const RequestContext&) noexcept`,
  `virtual void on_response(const RequestInfo&, const Response&) noexcept`,
  `virtual void on_unhandled_exception(std::exception_ptr) noexcept` — all with no-op defaults. CMake leaf
  `${DMP_HTTP}.Server.Observer` (INTERFACE) + layer aggregate `${DMP_HTTP}.Server`.

- [ ] **Step 1: Write the header**

`components/http/server/server_observer/server_observer.hpp`:

```cpp
#pragma once

#include <exception>

#include <boost/asio/awaitable.hpp>
#include <request_context.hpp>
#include <response.hpp>
#include <router.hpp>

namespace demiplane::http {

    /**
     * @brief Single typed observer interface (spec §9.2) — replaces the old
     *        module's ten callback vectors.
     *
     * Lifecycle hooks: the awaitable ones run ON the injected executor —
     * on_setup_complete via a detach-spawned notification coroutine right
     * after setup() goes live (D4: setup() itself never blocks);
     * on_shutdown_started is AWAITED by graceful_shutdown() phase 3, so real
     * async work (flush a buffer, close a pool) finishes before completion is
     * reported. on_shutdown_complete is sync + noexcept, fired right before
     * wait_until_stopped() unblocks. Observers are notified sequentially in
     * add order; a throwing async hook is caught and fanned to every
     * observer's on_unhandled_exception.
     *
     * Per-request hooks: fired from Router::dispatch through the Server-wired
     * std::function hooks (D2) — sync, noexcept, HOT PATH: keep them cheap and
     * allocation-free (the invocation itself is gated allocation-free).
     * on_response carries a RequestInfo SNAPSHOT (D3): the RequestContext is
     * consumed by the handler chain, so it no longer exists when the response
     * is available; info's views stay valid through the hook call. Handler
     * throws surface via on_unhandled_exception (then the driver writes the
     * 500); driver-level early responses (malformed 400, limit 4xx) and the
     * synthesized 500 body are NOT observed.
     *
     * THREADING: per-request hooks and on_unhandled_exception may fire
     * concurrently from any executor thread (one strand per connection) —
     * implementations must be thread-safe. exception_ptr is heap-managed: the
     * original module's use-after-free category is structurally impossible.
     */
    class ServerObserver {
    public:
        virtual ~ServerObserver() = default;

        virtual boost::asio::awaitable<void> on_setup_complete() {
            co_return;
        }
        virtual boost::asio::awaitable<void> on_shutdown_started() {
            co_return;
        }
        virtual void on_shutdown_complete() noexcept {
        }

        virtual void on_request(const RequestContext& /*ctx*/) noexcept {
        }
        virtual void on_response(const RequestInfo& /*info*/, const Response& /*resp*/) noexcept {
        }
        virtual void on_unhandled_exception(std::exception_ptr /*ep*/) noexcept {
        }
    };

}  // namespace demiplane::http
```

- [ ] **Step 2: Write the CMake leaf**

`components/http/server/server_observer/CMakeLists.txt`:

```cmake
##############################################################################
# Http Server — ServerObserver (single typed observer interface, header-only)
##############################################################################
add_library(${DMP_HTTP}.Server.Observer INTERFACE server_observer.hpp)

target_include_directories(${DMP_HTTP}.Server.Observer INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Server.Observer INTERFACE
        ${DMP_HTTP}.Routing.Router
        ${DMP_HTTP}.Types
        Boost::asio
)
##############################################################################
```

- [ ] **Step 3: Write the layer CMake**

`components/http/server/CMakeLists.txt` (leaves are added by subsequent tasks — same pattern as the listeners layer):

```cmake
##############################################################################
# Http Server — orchestration layer: the ServerObserver interface, the Server
# (executor-injected lifecycle orchestrator), and run_standalone convenience.
# Per-leaf convention; the dotted ${DMP_HTTP}.Server target is an INTERFACE
# aggregate. Leaves are added by subsequent tasks.
##############################################################################

add_subdirectory(server_observer)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Server INTERFACE)

target_link_libraries(${DMP_HTTP}.Server INTERFACE
        ${DMP_HTTP}.Server.Observer
)
##############################################################################
```

- [ ] **Step 4: Wire the layer into the component**

In `components/http/CMakeLists.txt`, insert between the Listeners block and the Exported-library block:

```cmake
##############################################################################
# Http Server layer (PR 5 of redesign)
##############################################################################
add_subdirectory(server)
##############################################################################
```

- [ ] **Step 5: Verify the configure step**

Run: `cmake --preset debug 2>&1 | tail -3`
Expected: configure completes with no CMake errors. (Header-only; first compiled consumer is Task 5.)

- [ ] **Step 6: Suggested commit grouping**

```bash
git add components/http/server components/http/CMakeLists.txt
# suggested message: "http/server: ServerObserver interface + server layer scaffold (spec §9.2, D2-D4)"
```

---

### Task 5: `Server` core (lifecycle orchestrator) + first integration test

**Files:**

- Create: `components/http/server/server/server.hpp`
- Create: `components/http/server/server/server.cpp`
- Create: `components/http/server/server/CMakeLists.txt`
- Modify: `components/http/server/CMakeLists.txt` (add leaf + aggregate link)
- Create: `tests/integration_tests/http/server_test_fixture.hpp`
- Create: `tests/integration_tests/http/test_http_server_lifecycle.cpp`
- Modify: `tests/integration_tests/http/CMakeLists.txt` (new `Http.Server` target)

**Interfaces:**

- Consumes: `ServerConfig` (Task 1), `RouteConflictAggregateError` (Task 2), `Router::Hooks`/`RequestInfo` (Task 3),
  `ServerObserver` (Task 4), landed `ListenerBase`/`TcpListener`/`TlsListener`/`QuicListener`/`TlsConfig`/
  `GroupBinding`/`HttpController`/`RouteRegistry`/`Router`/`IsHttpDriver`.
- Produces (used by Tasks 6–9 and PR 6):
    - `Server(ServerConfig cfg, boost::asio::any_io_executor exec)`; non-copyable/non-movable (`gears::Immutable`).
    - `template <IsHttpDriver Driver> Server& add_tcp_listener(std::string host, std::uint16_t port, Driver driver)`
    -
  `template <IsHttpDriver... Drivers> Server& add_tls_listener(std::string host, std::uint16_t port, TlsConfig tls, Drivers... drivers)`
  -
  `template <IsHttpDriver Driver> Server& add_quic_listener(std::string host, std::uint16_t port, TlsConfig tls, Driver driver)`
    - `template <std::derived_from<HttpController> C> Server& add_controller(std::shared_ptr<C> ctrl)`;
      `GroupBinding in_group(std::string prefix)`; `Server& add_observer(std::shared_ptr<ServerObserver> obs)`
    - `void setup()`, `void stop()`, `void wait_until_stopped()`, `boost::asio::awaitable<void> async_wait_stopped()`
    - `bool is_running() const noexcept`, `std::span<const std::unique_ptr<ListenerBase>> listeners() const noexcept`,
      `const ServerConfig& config() const noexcept`
    - Test infra: `http_it::ServerIntegrationFixture` with `start_server(configure, cfg = {}, io_threads = 1)`,
      `port()`, `shutdown_and_join()`; controllers `http_it::PingController` (`GET /ping` → `"pong"`, `GET /boom` →
      throws) and `http_it::LatchController` (`GET /slow` 150 ms + `slow_entered` latch, `GET /hang` 500 ms +
      `hang_entered` latch).

- [ ] **Step 1: Write the integration fixture**

`tests/integration_tests/http/server_test_fixture.hpp`:

```cpp
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <request_context.hpp>
#include <server.hpp>
#include <server_config.hpp>

#include "http_test_fixture.hpp"  // TcpClient (reused Beast client)

namespace http_it {

    /// GET /ping → 200 "pong"; GET /boom → handler throw (driver 500).
    class PingController final : public demiplane::http::HttpController {
    public:
        void configure_routes() override {
            Get("/ping", &PingController::ping);
            Get("/boom", &PingController::boom);
        }

    private:
        demiplane::http::AsyncResponse ping(demiplane::http::RequestContext ctx) {
            co_return ctx.ok("pong");
        }
        demiplane::http::AsyncResponse boom(demiplane::http::RequestContext) {
            throw std::runtime_error{"handler exploded"};
        }
    };

    /// Timed handlers with entry latches so tests can deterministically wait
    /// until a request is IN FLIGHT before triggering shutdown.
    class LatchController final : public demiplane::http::HttpController {
    public:
        std::atomic<bool> slow_entered{false};
        std::atomic<bool> hang_entered{false};

        void configure_routes() override {
            Get("/slow", &LatchController::slow);  // finishes inside any sane drain window
            Get("/hang", &LatchController::hang);  // outlives a 100ms drain deadline
        }

    private:
        demiplane::http::AsyncResponse slow(demiplane::http::RequestContext ctx) {
            slow_entered.store(true, std::memory_order_release);
            co_await wait(std::chrono::milliseconds{150});
            co_return ctx.ok("slow done");
        }
        demiplane::http::AsyncResponse hang(demiplane::http::RequestContext ctx) {
            hang_entered.store(true, std::memory_order_release);
            co_await wait(std::chrono::milliseconds{500});
            co_return ctx.ok("hang done");
        }
        static boost::asio::awaitable<void> wait(const std::chrono::milliseconds d) {
            const auto ex = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer t{ex};
            t.expires_after(d);
            co_await t.async_wait(boost::asio::use_awaitable);
        }
    };

    /// Owns an io_context + N worker threads + an injected-executor Server.
    /// start_server() runs the build phase + setup() and goes live;
    /// TearDown() runs the full §9.7 caller sequence: stop → wait_until_stopped
    /// → THEN tear the executor down.
    class ServerIntegrationFixture : public ::testing::Test {
    protected:
        boost::asio::io_context ioc_;
        // Keeps ioc_.run() from returning between thread start and setup()'s
        // accept loops (and across the post-shutdown assertions).
        std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> guard_{
            boost::asio::make_work_guard(ioc_)};
        std::optional<demiplane::http::Server> server_;
        std::vector<std::thread> workers_;
        bool torn_down_ = false;

        void start_server(const std::function<void(demiplane::http::Server&)>& configure,
                          demiplane::http::ServerConfig cfg = {},
                          const std::size_t io_threads      = 1) {
            server_.emplace(cfg, ioc_.get_executor());
            configure(*server_);
            server_->setup();  // throws surface in the test body
            workers_.reserve(io_threads);
            for (std::size_t i = 0; i < io_threads; ++i) {
                workers_.emplace_back([this] { ioc_.run(); });
            }
        }

        [[nodiscard]] std::uint16_t port() const {
            return server_->listeners().front()->bound_port();
        }

        /// §9.7 caller sequence. Idempotent — callable from a test body and
        /// again from TearDown.
        void shutdown_and_join() {
            if (torn_down_) {
                return;
            }
            torn_down_ = true;
            if (server_ && server_->is_running()) {
                server_->stop();
                server_->wait_until_stopped();  // executor still driven by workers_
            }
            guard_.reset();
            ioc_.stop();
            for (auto& t : workers_) {
                if (t.joinable()) {
                    t.join();
                }
            }
        }

        void TearDown() override {
            shutdown_and_join();
        }
    };

}  // namespace http_it
```

- [ ] **Step 2: Write the first failing test**

`tests/integration_tests/http/test_http_server_lifecycle.cpp`:

```cpp
#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include <boost/asio/post.hpp>
#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <server.hpp>

#include "server_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;
using namespace std::chrono_literals;

class ServerLifecycleTest : public http_it::ServerIntegrationFixture {};

TEST_F(ServerLifecycleTest, ServesAndStopsGracefully) {
    start_server([](Server& s) {
        s.add_controller(std::make_shared<http_it::PingController>());
        s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    });
    EXPECT_TRUE(server_->is_running());
    ASSERT_GT(port(), 0);

    http_it::TcpClient client{port()};
    const auto res = client.send(bhttp::verb::get, "/ping");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "pong");

    server_->stop();
    server_->wait_until_stopped();
    EXPECT_FALSE(server_->is_running());

    // §9.7: stop() must NOT stop the caller's executor — it may be shared
    // with subsystems that outlive HTTP. Prove it still executes work.
    std::promise<void> ran;
    boost::asio::post(ioc_, [&ran] { ran.set_value(); });
    ASSERT_EQ(ran.get_future().wait_for(1s), std::future_status::ready);
}
```

- [ ] **Step 3: Add the integration CMake target**

Append to `tests/integration_tests/http/CMakeLists.txt`:

```cmake
add_integration_test(${INTEGRATION_TESTING_TARGET}.Http.Server
        test_http_server_lifecycle.cpp
        LINK_LIBS
        Demiplane.Component.HTTP.Server
        Demiplane.Component.HTTP.Listeners
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Types
        Demiplane.Component.HTTP.Config
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
        LABELS "http"
)
target_include_directories(${INTEGRATION_TESTING_TARGET}.Http.Server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
```

- [ ] **Step 4: Run it to make sure it fails**

Run:
`cmake --preset debug && cmake --build build/debug --target Demiplane.Tests.Integration.Http.Server -- -j4 2>&1 | tail -5`
Expected: FAIL to compile — `<server.hpp>` not found.

- [ ] **Step 5: Write `server.hpp`**

`components/http/server/server/server.hpp`:

```cpp
#pragma once

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <demiplane/gears>
#include <demiplane/nexus>
#include <demiplane/scroll>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <controller.hpp>
#include <group.hpp>
#include <http_driver_concept.hpp>
#include <listener_base.hpp>
#include <quic_listener.hpp>
#include <route_registry.hpp>
#include <router.hpp>
#include <server_config.hpp>
#include <server_observer.hpp>
#include <tcp_listener.hpp>
#include <tls_config.hpp>
#include <tls_listener.hpp>

namespace demiplane::http {

    /**
     * @brief Thin lifecycle orchestrator over the landed layers (spec §9).
     *
     * The Server is HANDED an executor and owns no io_context and no threads
     * (spec §3/§9.1): the caller decides the thread↔context topology, and HTTP
     * coexists with the logger / DB pool / S3 client on caller-owned executors.
     * setup() binds synchronously and co_spawns each listener's accept loop
     * onto ITS OWN strand of the injected executor (D5 — a stop emit is only
     * safe when serialized with the loop's turns) bound to ITS OWN
     * cancellation signal (a slot holds one handler, spec §7.2). stop() is
     * non-blocking + idempotent and NEVER stops the executor. Shutdown
     * ordering contract (§9.7): after stop(), keep driving the executor until
     * wait_until_stopped() / async_wait_stopped() returns — only THEN tear the
     * executor down. wait_until_stopped() must be called from a thread NOT
     * driving the executor (use async_wait_stopped() there).
     *
     * Build phase (add_* / in_group) is single-threaded by contract; any
     * registration after setup() throws std::logic_error. A correct caller
     * destroys the Server only after wait_until_stopped() returned — the
     * destructor's stop() is a backstop for a leaked-running Server, not a
     * shutdown mechanism (spec §9.6).
     */
    class Server : gears::Immutable {
    public:
        NEXUS_REGISTER(nexus::Immortal);

        Server(ServerConfig cfg, boost::asio::any_io_executor exec);

        /// Backstop only: requests stop() if still running. Never blocks,
        /// never joins, never touches the executor's lifetime (spec §9.6).
        ~Server();

        // ── Build phase ───────────────────────────────────────────────────
        template <IsHttpDriver Driver>
        Server& add_tcp_listener(std::string host, const std::uint16_t port, Driver driver) {
            require_build("add_tcp_listener");
            listeners_.push_back(std::make_unique<TcpListener<Driver>>(
                exec_, std::move(host), port, std::move(driver), cfg_.request_arena_size));
            return *this;
        }

        template <IsHttpDriver... Drivers>
        Server& add_tls_listener(std::string host, const std::uint16_t port, TlsConfig tls, Drivers... drivers) {
            require_build("add_tls_listener");
            // Arena size stays at the TlsListener 8 KB default until PR 6
            // wires request_arena_size through (PR 4 note in tls_listener.hpp).
            listeners_.push_back(std::make_unique<TlsListener<Drivers...>>(
                exec_, std::move(host), port, std::move(tls), std::move(drivers)...));
            return *this;
        }

        template <IsHttpDriver Driver>
        Server& add_quic_listener(std::string host, const std::uint16_t port, TlsConfig tls, Driver driver) {
            require_build("add_quic_listener");
            listeners_.push_back(std::make_unique<QuicListener<Driver>>(
                exec_, std::move(host), port, std::move(tls), std::move(driver)));
            return *this;
        }

        template <std::derived_from<HttpController> C>
        Server& add_controller(std::shared_ptr<C> ctrl) {
            in_group("").add_controller(std::move(ctrl));
            return *this;
        }

        /// Prefix-scoped mounting (spec §8.4). The returned binding writes into
        /// this Server's registry/controller list; using it after setup()
        /// throws via the frozen registry.
        [[nodiscard]] GroupBinding in_group(std::string prefix) {
            require_build("in_group");
            return GroupBinding{registry_, controllers_, std::move(prefix)};
        }

        Server& add_observer(std::shared_ptr<ServerObserver> obs);

        // ── Lifecycle ─────────────────────────────────────────────────────
        /// Freeze routes (all conflicts thrown at once), initialize
        /// controllers (D8), bind every listener synchronously, wire observer
        /// hooks, spawn accept loops. Does NOT block and spawns no threads
        /// (D4) — the loops go live the moment the caller runs the executor.
        /// Throws: std::logic_error (state / no listeners),
        /// RouteConflictAggregateError, boost::system::system_error (bind),
        /// anything a controller initialize() throws. On throw the Server is
        /// NOT running and must be discarded.
        void setup();

        /// Request graceful shutdown; non-blocking, idempotent, thread-safe.
        /// No-op unless running (documented: stop() before setup() is a no-op).
        /// NEVER stops the executor (§9.7).
        void stop();

        /// Block until graceful shutdown completes (§9.7). Returns immediately
        /// if setup() never ran. MUST NOT be called from an executor thread —
        /// it would block the very shutdown it waits for; use
        /// async_wait_stopped() there.
        void wait_until_stopped();

        /// Awaitable twin for callers already running on the injected executor.
        [[nodiscard]] boost::asio::awaitable<void> async_wait_stopped();

        // ── Introspection ─────────────────────────────────────────────────
        [[nodiscard]] bool is_running() const noexcept {
            return state_.load(std::memory_order_acquire) == State::running;
        }
        [[nodiscard]] std::span<const std::unique_ptr<ListenerBase>> listeners() const noexcept {
            return listeners_;
        }
        [[nodiscard]] const ServerConfig& config() const noexcept {
            return cfg_;
        }

    private:
        enum class State : std::uint8_t { build, running, stopping, stopped };

        void require_build(std::string_view what) const;
        void wire_observer_hooks();
        void fan_unhandled_exception(std::exception_ptr ep) noexcept;
        [[nodiscard]] std::size_t total_in_flight() const noexcept;
        boost::asio::awaitable<void> notify_setup_observers();
        boost::asio::awaitable<void> graceful_shutdown();
        [[nodiscard]] static PathNormalization map_normalization(ServerConfig::PathNormalization n) noexcept;

        ServerConfig cfg_;
        boost::asio::any_io_executor exec_;  // injected; NOT owned, never stopped
        std::atomic<State> state_{State::build};

        RouteRegistry registry_;
        Router router_{registry_};
        std::vector<std::shared_ptr<HttpController>> controllers_;
        std::vector<std::shared_ptr<ServerObserver>> observers_;
        std::vector<std::unique_ptr<ListenerBase>> listeners_;

        // One strand + one stop signal PER listener (spec §7.2/§9.3: a slot
        // holds a single handler; the emit must be serialized with the accept
        // loop's turns — D5). cancellation_signal is immovable → unique_ptr.
        std::vector<boost::asio::any_io_executor> run_strands_;
        std::vector<std::unique_ptr<boost::asio::cancellation_signal>> stop_signals_;
        std::atomic<std::size_t> live_accept_loops_{0};

        std::mutex shutdown_mutex_;
        std::condition_variable shutdown_cv_;
        bool shutdown_complete_ = false;

        SCROLL_COMPONENT_PREFIX("Server");
    };

}  // namespace demiplane::http
```

- [ ] **Step 6: Write `server.cpp`**

`components/http/server/server/server.cpp`:

```cpp
#include "server.hpp"

#include <chrono>
#include <exception>
#include <stdexcept>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace demiplane::http {

    namespace {
        using namespace std::chrono_literals;
        // Poll cadence for the accept-loop / in-flight unwind barriers (D6) and
        // async_wait_stopped. Force-cancelled frames unwind within a few
        // executor turns, so the polls are short in practice.
        constexpr auto POLL_TICK = 5ms;
    }  // namespace

    Server::Server(ServerConfig cfg, boost::asio::any_io_executor exec)
        : cfg_{cfg},
          exec_{std::move(exec)},
          registry_{map_normalization(cfg.path_normalization)} {
    }

    Server::~Server() {
        // Backstop for a leaked-running Server (spec §9.6): request stop, do
        // NOT wait. A correct caller already ran the §9.7 sequence — if the
        // executor is about to die, in-flight work cannot be saved here anyway.
        if (is_running()) {
            stop();
        }
    }

    Server& Server::add_observer(std::shared_ptr<ServerObserver> obs) {
        require_build("add_observer");
        if (!obs) {
            throw std::invalid_argument{"Server::add_observer: null observer"};
        }
        observers_.push_back(std::move(obs));
        return *this;
    }

    void Server::setup() {
        require_build("setup");
        if (listeners_.empty()) {
            throw std::logic_error{"Server::setup: no listeners added"};
        }
        if (auto conflicts = registry_.freeze(); !conflicts.empty()) {
            throw RouteConflictAggregateError{std::move(conflicts)};
        }
        for (const auto& ctrl : controllers_) {  // D8: add order; throws abort setup
            ctrl->initialize();
        }
        for (const auto& listener : listeners_) {  // sync bind — failures surface HERE
            listener->bind();
        }
        wire_observer_hooks();

        run_strands_.reserve(listeners_.size());
        stop_signals_.reserve(listeners_.size());
        live_accept_loops_.store(listeners_.size(), std::memory_order_release);
        for (const auto& listener : listeners_) {
            run_strands_.push_back(boost::asio::make_strand(exec_));
            stop_signals_.push_back(std::make_unique<boost::asio::cancellation_signal>());
            boost::asio::co_spawn(run_strands_.back(),
                                  listener->run(router_),
                                  boost::asio::bind_cancellation_slot(
                                      stop_signals_.back()->slot(), [this](const std::exception_ptr ep) {
                                          // Runs on the listener's strand when run() finishes.
                                          // run() treats cancellation as state, so ep is an
                                          // ESCAPE (accept-loop bug/fatal) — surface it.
                                          if (ep) {
                                              fan_unhandled_exception(ep);
                                          }
                                          live_accept_loops_.fetch_sub(1, std::memory_order_acq_rel);
                                      }));
        }
        if (!observers_.empty()) {  // D4: notified on exec_, setup() never blocks
            boost::asio::co_spawn(exec_, notify_setup_observers(), boost::asio::detached);
        }
        state_.store(State::running, std::memory_order_release);
        COMPONENT_LOG_INF() << "setup complete: " << listeners_.size() << " listener(s) live";
    }

    void Server::stop() {
        auto expected = State::running;
        if (!state_.compare_exchange_strong(expected, State::stopping, std::memory_order_acq_rel)) {
            return;  // build (stop-before-setup no-op), stopping, stopped: idempotent
        }
        COMPONENT_LOG_INF() << "stop requested — spawning graceful shutdown";
        boost::asio::co_spawn(exec_, graceful_shutdown(), boost::asio::detached);
    }

    void Server::wait_until_stopped() {
        std::unique_lock lk{shutdown_mutex_};
        shutdown_cv_.wait(lk, [this] {
            return shutdown_complete_ || state_.load(std::memory_order_acquire) == State::build;
        });
    }

    boost::asio::awaitable<void> Server::async_wait_stopped() {
        boost::asio::steady_timer tick{exec_};
        while (true) {
            const auto s = state_.load(std::memory_order_acquire);
            if (s == State::stopped || s == State::build) {
                co_return;
            }
            tick.expires_after(POLL_TICK);
            co_await tick.async_wait(boost::asio::use_awaitable);
        }
    }

    boost::asio::awaitable<void> Server::graceful_shutdown() {
        namespace asio = boost::asio;
        asio::steady_timer tick{exec_};

        // Phase 1: cancel accept loops — one signal per listener, each emit
        // DISPATCHED onto the strand its run() executes on (D5: emit is only
        // safe when serialized with the loop's turns on a multi-threaded
        // executor; this also closes the edge-lost-emit window).
        for (std::size_t i = 0; i < listeners_.size(); ++i) {
            asio::dispatch(run_strands_[i], [sig = stop_signals_[i].get()] {
                sig->emit(asio::cancellation_type::terminal);
            });
        }
        // Phase 1.5: await accept-loop completion. run() closes its acceptor
        // on every exit path, so from here new connections are provably
        // REFUSED, not backlogged (spec §14.2).
        while (live_accept_loops_.load(std::memory_order_acquire) > 0) {
            tick.expires_after(POLL_TICK);
            co_await tick.async_wait(asio::use_awaitable);
        }

        // Phase 2: drain in-flight requests up to drain_timeout (shared
        // deadline — total wait is bounded by ONE timeout, not one per
        // listener). At the deadline the trackers force-cancel survivors.
        const auto deadline = std::chrono::steady_clock::now() + cfg_.drain_timeout;
        for (const auto& listener : listeners_) {
            co_await listener->drain_until(deadline);
        }
        // Phase 2.5: unwind barrier (D6). drain_until only DISPATCHES
        // force-cancels; the cancelled serve() frames unwind in later executor
        // turns. Destroying listeners (or letting the caller kill the
        // executor) with frames still suspended would be a use-after-free —
        // poll until every tracker Handle released. Deliberately UNBOUNDED: a
        // suspended frame cannot be freed except by completion, so a handler
        // that ignores cancellation delays shutdown rather than corrupting it.
        if (total_in_flight() > 0) {
            COMPONENT_LOG_WRN() << "drain deadline passed with " << total_in_flight()
                                << " connection(s) force-cancelled — waiting for unwind";
        }
        while (total_in_flight() > 0) {
            tick.expires_after(POLL_TICK);
            co_await tick.async_wait(asio::use_awaitable);
        }

        // Phase 3: async shutdown observers — awaited ON the still-driven
        // executor (§9.7), so real async work finishes before completion is
        // reported. A throwing observer fans to everyone's
        // on_unhandled_exception and shutdown continues.
        for (const auto& obs : observers_) {
            try {
                co_await obs->on_shutdown_started();
            } catch (...) {
                fan_unhandled_exception(std::current_exception());
            }
        }

        // Phase 4: controller shutdown — sync, REVERSE add order (spec §9.5).
        for (auto it = controllers_.rbegin(); it != controllers_.rend(); ++it) {
            try {
                (*it)->shutdown();
            } catch (...) {
                fan_unhandled_exception(std::current_exception());
            }
        }

        // Phase 5: sync, noexcept completion notifications.
        for (const auto& obs : observers_) {
            obs->on_shutdown_complete();
        }

        // Phase 6: report completion — unblocks wait_until_stopped(). The
        // executor is NOT stopped or drained; the caller owns it (§9.7).
        {
            std::lock_guard lk{shutdown_mutex_};
            shutdown_complete_ = true;
            state_.store(State::stopped, std::memory_order_release);
        }
        shutdown_cv_.notify_all();
        COMPONENT_LOG_INF() << "graceful shutdown complete";
    }

    void Server::require_build(const std::string_view what) const {
        if (state_.load(std::memory_order_acquire) != State::build) {
            throw std::logic_error{"Server::" + std::string{what}
                                   + ": registration/setup after setup() (registry frozen, spec §8.1 phase 3)"};
        }
    }

    void Server::wire_observer_hooks() {
        if (observers_.empty()) {
            return;  // hot path keeps null hooks — one branch, zero fan-out
        }
        // Fan-out lambdas capture only `this` (std::function SBO — invocation
        // is allocation-free, gated in the routing gate test). observers_ is
        // immutable from setup() on (require_build guards add_observer).
        router_.set_hooks(Router::Hooks{
            .on_request =
                [this](const RequestContext& ctx) noexcept {
                    for (const auto& obs : observers_) {
                        obs->on_request(ctx);
                    }
                },
            .on_response =
                [this](const RequestInfo& info, const Response& resp) noexcept {
                    for (const auto& obs : observers_) {
                        obs->on_response(info, resp);
                    }
                },
            .on_unhandled_exception =
                [this](const std::exception_ptr ep) noexcept {
                    for (const auto& obs : observers_) {
                        obs->on_unhandled_exception(ep);
                    }
                },
        });
    }

    void Server::fan_unhandled_exception(const std::exception_ptr ep) noexcept {
        for (const auto& obs : observers_) {
            obs->on_unhandled_exception(ep);
        }
    }

    std::size_t Server::total_in_flight() const noexcept {
        std::size_t n = 0;
        for (const auto& listener : listeners_) {
            n += listener->in_flight();
        }
        return n;
    }

    boost::asio::awaitable<void> Server::notify_setup_observers() {
        // D4: sequential, add order, on the injected executor. setup() itself
        // returned already — this is a notification, not a barrier.
        for (const auto& obs : observers_) {
            try {
                co_await obs->on_setup_complete();
            } catch (...) {
                fan_unhandled_exception(std::current_exception());
            }
        }
    }

    PathNormalization Server::map_normalization(const ServerConfig::PathNormalization n) noexcept {
        switch (n) {
            case ServerConfig::PathNormalization::none:
                return PathNormalization::none;
            case ServerConfig::PathNormalization::collapse_multi_slash:
                return PathNormalization::collapse_multi_slash;
            case ServerConfig::PathNormalization::collapse_trailing_slash:
                break;
        }
        return PathNormalization::collapse_trailing_slash;
    }

}  // namespace demiplane::http
```

- [ ] **Step 7: Write the CMake leaf + aggregate link**

`components/http/server/server/CMakeLists.txt`:

```cmake
##############################################################################
# Http Server — Server (executor-injected lifecycle orchestrator, spec §9)
##############################################################################
add_library(${DMP_HTTP}.Server.Core STATIC server.cpp)

target_include_directories(${DMP_HTTP}.Server.Core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Server.Core
        PUBLIC
        ${DMP_HTTP}.Server.Observer
        ${DMP_HTTP}.Routing
        ${DMP_HTTP}.Listeners
        ${DMP_HTTP}.Config
        Boost::asio
        Demiplane::Common::Gears
        Demiplane::Common::Nexus
        Demiplane::Common::Scroll
)
##############################################################################
```

In `components/http/server/CMakeLists.txt`, add `add_subdirectory(server)` after `add_subdirectory(server_observer)`
and `${DMP_HTTP}.Server.Core` to the aggregate's link list:

```cmake
add_subdirectory(server_observer)
add_subdirectory(server)
```

```cmake
target_link_libraries(${DMP_HTTP}.Server INTERFACE
        ${DMP_HTTP}.Server.Observer
        ${DMP_HTTP}.Server.Core
)
```

- [ ] **Step 8: Build + run the test**

Run:
`cmake --preset debug && cmake --build build/debug --target Demiplane.Tests.Integration.Http.Server -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Http.Server" 2>&1 | tail -6`
Expected: PASS (`ServesAndStopsGracefully`).

- [ ] **Step 9: Regression sweep**

Run: `cmake --build build/debug -- -j4 && ctest --test-dir build/debug --output-on-failure -L http 2>&1 | tail -8`
Expected: every existing http-labeled unit + integration test still passes.

- [ ] **Step 10: Suggested commit grouping**

```bash
git add components/http/server tests/integration_tests/http/server_test_fixture.hpp tests/integration_tests/http/test_http_server_lifecycle.cpp tests/integration_tests/http/CMakeLists.txt
# suggested message: "http/server: executor-injected Server — setup/stop/graceful_shutdown/wait (spec §9, D4-D8)"
```

---

### Task 6: Lifecycle integration battery (§14.2)

**Files:**

- Modify: `tests/integration_tests/http/test_http_server_lifecycle.cpp` (append)
- Modify: `tests/integration_tests/http/http_test_fixture.hpp` (add `TcpClient::read_response`)
- Modify: `tests/unit_tests/http/listeners/test_tcp_listener.cpp` (delete the `TODO(PR5)` line 16 — the EADDRINUSE
  test lands here at server level, where §14.2 wants it)

**Interfaces:**

- Consumes: everything Task 5 produced; `LatchController` latches; `TcpClient::{send, write_request,
  read_after_close}`.
- Produces: `TcpClient::read_response() -> ParsedResponse` (used again by Task 9).

- [ ] **Step 1: Add `TcpClient::read_response`**

In `tests/integration_tests/http/http_test_fixture.hpp`, insert into `TcpClient` (right after `write_request`):

```cpp
        /// Read one response for a previously write_request()-ed request —
        /// lets a test trigger server-side events (e.g. graceful shutdown)
        /// BETWEEN sending and receiving.
        ParsedResponse read_response() {
            ParsedResponse res;
            bhttp::read(socket_, buffer_, res);
            return res;
        }
```

- [ ] **Step 2: Append the failing battery**

Append to `tests/integration_tests/http/test_http_server_lifecycle.cpp`:

```cpp
namespace {

    /// Registers GET /dup — two instances collide on freeze().
    class DupController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/dup", &DupController::h);
        }

    private:
        AsyncResponse h(RequestContext ctx) {
            co_return ctx.ok("dup");
        }
    };

    void add_ping_tcp(Server& s) {
        s.add_controller(std::make_shared<http_it::PingController>());
        s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    }

}  // namespace

TEST_F(ServerLifecycleTest, StopBeforeSetupIsANoOp) {
    Server server{ServerConfig{}, ioc_.get_executor()};
    server.stop();  // documented no-op (spec §9.7 corollary)
    EXPECT_FALSE(server.is_running());
    server.wait_until_stopped();  // returns immediately — setup() never ran
}

TEST_F(ServerLifecycleTest, StopIsIdempotentAndWaitReturnsAgain) {
    start_server(add_ping_tcp);
    server_->stop();
    server_->stop();  // second call: CAS fails, silently ignored
    server_->wait_until_stopped();
    server_->stop();  // after stopped: still a no-op
    server_->wait_until_stopped();  // shutdown_complete_ latched — immediate
    EXPECT_FALSE(server_->is_running());
}

TEST_F(ServerLifecycleTest, SetupWithoutListenersThrows) {
    Server server{ServerConfig{}, ioc_.get_executor()};
    server.add_controller(std::make_shared<http_it::PingController>());
    EXPECT_THROW(server.setup(), std::logic_error);
    EXPECT_FALSE(server.is_running());
}

TEST_F(ServerLifecycleTest, RegistrationAfterSetupThrows) {
    start_server(add_ping_tcp);
    EXPECT_THROW(server_->add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}}), std::logic_error);
    EXPECT_THROW(server_->add_observer(std::make_shared<ServerObserver>()), std::logic_error);
    EXPECT_THROW((void)server_->in_group("/late"), std::logic_error);  // (void): in_group is [[nodiscard]]
    EXPECT_THROW(server_->add_controller(std::make_shared<http_it::PingController>()), std::logic_error);
    EXPECT_THROW(server_->setup(), std::logic_error);  // double setup
    EXPECT_TRUE(server_->is_running());                // still serving despite the throws
}

TEST_F(ServerLifecycleTest, SetupThrowsWhenPortInUse) {
    // §14.2 lifecycle: setup() failure on port-in-use (resolves the PR 4
    // TODO(PR5) in test_tcp_listener.cpp). SO_REUSEADDR does not permit two
    // LISTENING sockets on one port — the second bind is EADDRINUSE.
    boost::asio::io_context probe;
    const boost::asio::ip::tcp::acceptor taken{
        probe, {boost::asio::ip::make_address("127.0.0.1"), 0}};  // open+bind+listen
    const auto port = taken.local_endpoint().port();

    server_.emplace(ServerConfig{}, ioc_.get_executor());
    server_->add_controller(std::make_shared<http_it::PingController>());
    server_->add_tcp_listener("127.0.0.1", port, Http11Driver{Http11Config{}});
    EXPECT_THROW(server_->setup(), boost::system::system_error);
    EXPECT_FALSE(server_->is_running());
}

TEST_F(ServerLifecycleTest, ConflictingRoutesThrowAggregateAtSetup) {
    server_.emplace(ServerConfig{}, ioc_.get_executor());
    server_->add_controller(std::make_shared<DupController>());
    server_->add_controller(std::make_shared<DupController>());
    server_->add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    try {
        server_->setup();
        FAIL() << "expected RouteConflictAggregateError";
    } catch (const RouteConflictAggregateError& e) {
        EXPECT_EQ(e.conflicts().size(), 1u);
        EXPECT_NE(std::string{e.what()}.find("/dup"), std::string::npos);
    }
    EXPECT_FALSE(server_->is_running());
}

TEST_F(ServerLifecycleTest, GracefulShutdownCompletesInFlightRequests) {
    auto latch = std::make_shared<http_it::LatchController>();
    start_server([&](Server& s) {
        s.add_controller(latch);
        s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    });

    http_it::TcpClient client{port()};
    client.write_request(bhttp::verb::get, "/slow");
    while (!latch->slow_entered.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(1ms);  // request provably in flight
    }
    server_->stop();
    server_->wait_until_stopped();

    // Drain phase let the 150ms handler finish and the response reach the
    // wire BEFORE shutdown completed (spec §14.2 "in-flight completes").
    const auto res = client.read_response();
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "slow done");
}

TEST_F(ServerLifecycleTest, NewConnectionsRefusedAfterShutdown) {
    start_server(add_ping_tcp);
    const auto p = port();
    server_->stop();
    server_->wait_until_stopped();
    // Acceptors are closed in phase 1.5 — a fresh connect is REFUSED, not
    // silently backlogged (spec §14.2; the TcpClient ctor connect throws).
    EXPECT_THROW(http_it::TcpClient{p}, boost::system::system_error);
}

TEST_F(ServerLifecycleTest, DrainDeadlineForceCancelsStragglers) {
    ServerConfig cfg;
    cfg.drain_timeout = std::chrono::milliseconds{100};  // << the 500ms /hang handler
    auto latch        = std::make_shared<http_it::LatchController>();
    start_server(
        [&](Server& s) {
            s.add_controller(latch);
            s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
        },
        cfg);

    http_it::TcpClient client{port()};
    client.write_request(bhttp::verb::get, "/hang");
    while (!latch->hang_entered.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(1ms);
    }
    const auto t0 = std::chrono::steady_clock::now();
    server_->stop();
    server_->wait_until_stopped();
    // Bounded: 100ms drain + force-cancel + handler-completion unwind (the
    // 500ms timer is not slot-bound — phase 2.5 waits for it), NOT the
    // 30s default drain budget.
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 5s);

    // Force-cancelled connection: socket closed, no response ever written.
    const auto ec = client.read_after_close();
    EXPECT_TRUE(ec) << "expected the force-cancelled connection to be dead, got a clean read";
}

TEST_F(ServerLifecycleTest, AsyncWaitStoppedCompletesWithShutdown) {
    start_server(add_ping_tcp);
    // The awaitable twin (§9.1) — for callers already ON the executor, where
    // the blocking wait_until_stopped() would deadlock the shutdown it awaits.
    std::promise<void> stopped;
    boost::asio::co_spawn(
        ioc_,
        [this]() -> boost::asio::awaitable<void> { co_await server_->async_wait_stopped(); },
        [&stopped](std::exception_ptr) { stopped.set_value(); });
    auto fut = stopped.get_future();
    EXPECT_EQ(fut.wait_for(50ms), std::future_status::timeout);  // still running — must not complete early
    server_->stop();
    ASSERT_EQ(fut.wait_for(5s), std::future_status::ready);  // completes once shutdown finishes
    server_->wait_until_stopped();                            // latched — returns immediately
    EXPECT_FALSE(server_->is_running());
}

TEST_F(ServerLifecycleTest, GroupPrefixMountsControllerAtServerLevel) {
    start_server([](Server& s) {
        s.in_group("/api/v1").add_controller(std::make_shared<http_it::PingController>());
        s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    });
    http_it::TcpClient client{port()};
    // keep_alive=true: the second request reuses the socket.
    EXPECT_EQ(client.send(bhttp::verb::get, "/api/v1/ping", {}, "text/plain", true).body(), "pong");
    EXPECT_EQ(client.send(bhttp::verb::get, "/ping").result_int(), 404u);  // unprefixed path not mounted
}
```

Also add the three includes the new tests need at the top of the file (after the existing includes):

```cpp
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <route_registry.hpp>  // RouteConflictAggregateError
```

- [ ] **Step 3: Run to see the new tests fail/compile-check**

Run: `cmake --build build/debug --target Demiplane.Tests.Integration.Http.Server -- -j4 2>&1 | tail -5`
Expected: compiles (Task 5 delivered every API used); if anything is missing this step catches the drift now.

- [ ] **Step 4: Run the battery**

Run: `ctest --test-dir build/debug --output-on-failure -R "Http.Server" 2>&1 | tail -8`
Expected: PASS — all lifecycle tests green.

- [ ] **Step 5: Delete the resolved test TODO**

In `tests/unit_tests/http/listeners/test_tcp_listener.cpp`, delete line 16:

```cpp
// TODO(PR5): add an integration test that bind() throws boost::system::system_error on EADDRINUSE (bind two listeners to the same fixed port) — §14.2 lifecycle.
```

Run:
`cmake --build build/debug --target Demiplane.Tests.Unit.Http.Listeners -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Http.Listeners" 2>&1 | tail -4`
Expected: PASS (comment-only change).

- [ ] **Step 6: Suggested commit grouping**

```bash
git add tests/integration_tests/http/test_http_server_lifecycle.cpp tests/integration_tests/http/http_test_fixture.hpp tests/unit_tests/http/listeners/test_tcp_listener.cpp
# suggested message: "http/tests: §14.2 lifecycle battery — port-in-use, frozen registration, drain, force-cancel"
```

---

### Task 7: Observer integration battery (§14.2)

**Files:**

- Create: `tests/integration_tests/http/test_http_server_observer.cpp`
- Modify: `tests/integration_tests/http/CMakeLists.txt` (add the source to the `Http.Server` target)

**Interfaces:**

- Consumes: `ServerObserver` (Task 4), `Server::add_observer` (Task 5), fixture + controllers (Task 5).
- Produces: nothing new — test-only.

- [ ] **Step 1: Write the tests**

`tests/integration_tests/http/test_http_server_observer.cpp`:

```cpp
#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <server.hpp>
#include <server_observer.hpp>

#include "server_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;
using namespace std::chrono_literals;

namespace {

    /// Thread-safe event log — hooks fire on io threads, asserts run on the
    /// test thread.
    class RecordingObserver final : public ServerObserver {
    public:
        boost::asio::awaitable<void> on_setup_complete() override {
            push("setup_complete");
            co_return;
        }
        boost::asio::awaitable<void> on_shutdown_started() override {
            // REAL async work: if the Server failed to await this,
            // shutdown_complete would overtake it and the order assert fails.
            const auto ex = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer t{ex};
            t.expires_after(100ms);
            co_await t.async_wait(boost::asio::use_awaitable);
            push("shutdown_started");
        }
        void on_shutdown_complete() noexcept override {
            push("shutdown_complete");
        }
        void on_request(const RequestContext& ctx) noexcept override {
            push("request:" + std::string{ctx.target()});
        }
        void on_response(const RequestInfo& info, const Response& resp) noexcept override {
            push("response:" + std::string{info.target} + ":"
                 + std::to_string(std::to_underlying(resp.status)));
        }
        void on_unhandled_exception(std::exception_ptr) noexcept override {
            push("exception");
        }

        [[nodiscard]] std::vector<std::string> events() {
            std::lock_guard lk{mu_};
            return events_;
        }
        [[nodiscard]] bool saw(const std::string& e) {
            std::lock_guard lk{mu_};
            return std::ranges::find(events_, e) != events_.end();
        }
        [[nodiscard]] std::ptrdiff_t index_of(const std::string& e) {
            std::lock_guard lk{mu_};
            const auto it = std::ranges::find(events_, e);
            return it == events_.end() ? -1 : it - events_.begin();
        }

    private:
        void push(std::string e) {
            std::lock_guard lk{mu_};
            events_.push_back(std::move(e));
        }
        std::mutex mu_;
        std::vector<std::string> events_;
    };

    /// on_shutdown_started throws — must fan to on_unhandled_exception and
    /// NOT abort the shutdown.
    class ThrowingShutdownObserver final : public ServerObserver {
    public:
        boost::asio::awaitable<void> on_shutdown_started() override {
            throw std::runtime_error{"observer exploded"};
            co_return;  // unreachable; keeps this a coroutine
        }
    };

}  // namespace

class ServerObserverTest : public http_it::ServerIntegrationFixture {
protected:
    std::shared_ptr<RecordingObserver> observer_ = std::make_shared<RecordingObserver>();

    void start_with_observer() {
        start_server([&](Server& s) {
            s.add_observer(observer_);
            s.add_controller(std::make_shared<http_it::PingController>());
            s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
        });
    }

    void wait_for(const std::string& event, const std::chrono::seconds cap = 2s) {
        const auto deadline = std::chrono::steady_clock::now() + cap;
        while (!observer_->saw(event) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }
        ASSERT_TRUE(observer_->saw(event)) << "timed out waiting for '" << event << "'";
    }
};

TEST_F(ServerObserverTest, LifecycleAndRequestHooksFireInOrder) {
    start_with_observer();
    wait_for("setup_complete");  // D4: notified async on the executor

    http_it::TcpClient client{port()};
    EXPECT_EQ(client.send(bhttp::verb::get, "/ping").result_int(), 200u);
    wait_for("response:/ping:200");

    server_->stop();
    server_->wait_until_stopped();

    // shutdown_started (with its 100ms of awaited async work) must precede
    // shutdown_complete, which must precede wait_until_stopped() returning —
    // both are already in the log HERE.
    const auto i_setup    = observer_->index_of("setup_complete");
    const auto i_req      = observer_->index_of("request:/ping");
    const auto i_res      = observer_->index_of("response:/ping:200");
    const auto i_started  = observer_->index_of("shutdown_started");
    const auto i_complete = observer_->index_of("shutdown_complete");
    ASSERT_NE(i_setup, -1);
    ASSERT_NE(i_req, -1);
    ASSERT_NE(i_res, -1);
    ASSERT_NE(i_started, -1);
    ASSERT_NE(i_complete, -1);
    EXPECT_LT(i_setup, i_req);
    EXPECT_LT(i_req, i_res);
    EXPECT_LT(i_res, i_started);
    EXPECT_LT(i_started, i_complete);
}

TEST_F(ServerObserverTest, ResponseHookFiresForRoutingMisses) {
    start_with_observer();
    http_it::TcpClient client{port()};
    EXPECT_EQ(client.send(bhttp::verb::get, "/nope").result_int(), 404u);
    wait_for("response:/nope:404");
    EXPECT_TRUE(observer_->saw("request:/nope"));
}

TEST_F(ServerObserverTest, UnhandledExceptionHookFiresAndClientGets500) {
    start_with_observer();
    http_it::TcpClient client{port()};
    EXPECT_EQ(client.send(bhttp::verb::get, "/boom").result_int(), 500u);
    wait_for("exception");
    EXPECT_TRUE(observer_->saw("request:/boom"));
    // The 500 is DRIVER-synthesized after the rethrow — invisible to
    // on_response (D2, documented).
    EXPECT_FALSE(observer_->saw("response:/boom:500"));
}

TEST_F(ServerObserverTest, ThrowingShutdownObserverFansToAllAndShutdownCompletes) {
    start_server([&](Server& s) {
        s.add_observer(std::make_shared<ThrowingShutdownObserver>());
        s.add_observer(observer_);
        s.add_controller(std::make_shared<http_it::PingController>());
        s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    });
    server_->stop();
    server_->wait_until_stopped();  // completes despite the throw
    EXPECT_TRUE(observer_->saw("exception"));          // fanned to EVERY observer
    EXPECT_TRUE(observer_->saw("shutdown_complete"));  // phases 4-6 still ran
}
```

- [ ] **Step 2: Add the source to the integration target**

In `tests/integration_tests/http/CMakeLists.txt`, add `test_http_server_observer.cpp` to the
`${INTEGRATION_TESTING_TARGET}.Http.Server` source list (after `test_http_server_lifecycle.cpp`).

- [ ] **Step 3: Build + run**

Run:
`cmake --build build/debug --target Demiplane.Tests.Integration.Http.Server -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Http.Server" 2>&1 | tail -8`
Expected: PASS — observer + lifecycle batteries green.

- [ ] **Step 4: Suggested commit grouping**

```bash
git add tests/integration_tests/http/test_http_server_observer.cpp tests/integration_tests/http/CMakeLists.txt
# suggested message: "http/tests: §14.2 observer battery — hook order, 404/exception paths, awaited async shutdown"
```

---

### Task 8: `run_standalone` + signal-driven shutdown test

**Files:**

- Create: `components/http/server/run_standalone/run_standalone.hpp`
- Create: `components/http/server/run_standalone/run_standalone.cpp`
- Create: `components/http/server/run_standalone/CMakeLists.txt`
- Modify: `components/http/server/CMakeLists.txt` (leaf + aggregate link)
- Create: `tests/integration_tests/http/test_http_run_standalone.cpp`
- Modify: `tests/integration_tests/http/CMakeLists.txt` (add the source)

**Interfaces:**

- Consumes: `Server`, `ServerConfig`.
- Produces: `void run_standalone(ServerConfig cfg, std::size_t threads, const std::function<void(Server&)>& configure)`
  — owns io_context + threads + SIGINT/SIGTERM handling; blocks until graceful shutdown completes; throws
  `std::invalid_argument` on `threads == 0`; `configure`/`setup()` exceptions propagate (no threads started yet).

- [ ] **Step 1: Write the failing tests**

`tests/integration_tests/http/test_http_run_standalone.cpp`:

```cpp
#include <atomic>
#include <chrono>
#include <csignal>
#include <memory>
#include <stdexcept>
#include <thread>

#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>
#include <unistd.h>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <run_standalone.hpp>
#include <server.hpp>

#include "server_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;
using namespace std::chrono_literals;

namespace {

    /// Drives run_standalone on a side thread; the test thread plays "ops".
    struct StandaloneRun {
        std::atomic<Server*> server{nullptr};
        std::atomic<bool> returned{false};
        std::thread runner;

        explicit StandaloneRun(const std::size_t threads = 2) {
            runner = std::thread{[this, threads] {
                run_standalone(ServerConfig{}, threads, [this](Server& s) {
                    s.add_controller(std::make_shared<http_it::PingController>());
                    s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
                    server.store(&s, std::memory_order_release);  // valid until run_standalone returns
                });
                returned.store(true, std::memory_order_release);
            }};
        }

        /// Wait until the server is live; returns its bound port.
        std::uint16_t await_live() {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            while (std::chrono::steady_clock::now() < deadline) {
                if (auto* s = server.load(std::memory_order_acquire); s && s->is_running()) {
                    return s->listeners().front()->bound_port();
                }
                std::this_thread::sleep_for(1ms);
            }
            ADD_FAILURE() << "run_standalone never went live";
            return 0;
        }

        void await_return() {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            while (!returned.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
            }
            EXPECT_TRUE(returned.load(std::memory_order_acquire)) << "run_standalone did not return";
            if (runner.joinable()) {
                runner.join();
            }
        }

        ~StandaloneRun() {
            if (runner.joinable()) {
                runner.join();
            }
        }
    };

}  // namespace

TEST(RunStandaloneTest, ServesAndStopsOnSigint) {
    StandaloneRun run;
    const auto port = run.await_live();
    ASSERT_GT(port, 0);
    {
        http_it::TcpClient client{port};
        const auto res = client.send(bhttp::verb::get, "/ping");
        EXPECT_EQ(res.result_int(), 200u);
        EXPECT_EQ(res.body(), "pong");
    }
    ::kill(::getpid(), SIGINT);  // §14.2: SIGINT → graceful shutdown
    run.await_return();
    // run.server is dangling once returned — do not touch it here.
}

TEST(RunStandaloneTest, StopsViaServerStopToo) {
    StandaloneRun run;
    const auto port = run.await_live();
    ASSERT_GT(port, 0);
    run.server.load(std::memory_order_acquire)->stop();  // programmatic path, no signal
    run.await_return();
}

TEST(RunStandaloneTest, ZeroThreadsThrows) {
    EXPECT_THROW(run_standalone(ServerConfig{}, 0, [](Server&) {}), std::invalid_argument);
}
```

- [ ] **Step 2: Add the source to the integration target and see it fail**

Add `test_http_run_standalone.cpp` to the `${INTEGRATION_TESTING_TARGET}.Http.Server` source list.
Run: `cmake --build build/debug --target Demiplane.Tests.Integration.Http.Server -- -j4 2>&1 | tail -5`
Expected: FAIL — `<run_standalone.hpp>` not found.

- [ ] **Step 3: Write the header**

`components/http/server/run_standalone/run_standalone.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <functional>

#include <server.hpp>
#include <server_config.hpp>

namespace demiplane::http {

    /**
     * @brief Convenience for the "HTTP owns the process" case (spec §9.1).
     *
     * Creates an internal io_context + `threads` worker threads, calls
     * `configure(server)` for listener/controller/observer wiring, runs
     * setup(), installs SIGINT/SIGTERM → stop(), blocks until graceful
     * shutdown completes, then tears the context down and joins — exactly the
     * §9.7 stop → wait → stop-context → join sequence, so trivial apps never
     * reason about the shutdown-ordering contract.
     *
     * Throws std::invalid_argument on threads == 0; configure()/setup()
     * exceptions propagate (worker threads are started only after setup()
     * succeeded, so the unwind is trivially clean).
     */
    void run_standalone(ServerConfig cfg, std::size_t threads, const std::function<void(Server&)>& configure);

}  // namespace demiplane::http
```

- [ ] **Step 4: Write the implementation**

`components/http/server/run_standalone/run_standalone.cpp`:

```cpp
#include "run_standalone.hpp"

#include <csignal>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/system/error_code.hpp>

namespace demiplane::http {

    void run_standalone(ServerConfig cfg, const std::size_t threads,
                        const std::function<void(Server&)>& configure) {
        if (threads == 0) {
            throw std::invalid_argument{"run_standalone: threads must be >= 1"};
        }
        boost::asio::io_context ioc{static_cast<int>(threads)};
        // Keeps ioc.run() from returning in the window between a worker
        // starting and it picking up posted work (and after shutdown, until
        // the teardown below releases it).
        auto guard = boost::asio::make_work_guard(ioc);

        Server server{std::move(cfg), ioc.get_executor()};
        configure(server);
        server.setup();  // throws propagate — no threads started yet

        boost::asio::signal_set signals{ioc, SIGINT, SIGTERM};
        signals.async_wait([&server](const boost::system::error_code& ec, int /*signo*/) {
            if (!ec) {
                server.stop();  // thread-safe, idempotent
            }
        });

        std::vector<std::jthread> workers;
        workers.reserve(threads);
        for (std::size_t i = 0; i < threads; ++i) {
            workers.emplace_back([&ioc] { ioc.run(); });
        }

        server.wait_until_stopped();  // §9.7: workers keep driving ioc until here

        // NOW it is safe to tear the executor down (§9.7 canonical sequence).
        signals.cancel();
        guard.reset();
        ioc.stop();
        // workers (jthreads) join on scope exit; server/ioc are destroyed
        // after them — every coroutine frame already unwound (phase 2.5).
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Write the CMake leaf + aggregate link**

`components/http/server/run_standalone/CMakeLists.txt`:

```cmake
##############################################################################
# Http Server — run_standalone ("HTTP owns the process" convenience, spec §9.1)
##############################################################################
add_library(${DMP_HTTP}.Server.RunStandalone STATIC run_standalone.cpp)

target_include_directories(${DMP_HTTP}.Server.RunStandalone PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Server.RunStandalone
        PUBLIC
        ${DMP_HTTP}.Server.Core
        Boost::asio
)
##############################################################################
```

In `components/http/server/CMakeLists.txt`: add `add_subdirectory(run_standalone)` after `add_subdirectory(server)`
and `${DMP_HTTP}.Server.RunStandalone` to the aggregate's link list.

- [ ] **Step 6: Build + run**

Run:
`cmake --preset debug && cmake --build build/debug --target Demiplane.Tests.Integration.Http.Server -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Http.Server" 2>&1 | tail -8`
Expected: PASS — including both standalone tests (the SIGINT test installs a real handler via `signal_set`; it
unregisters on destruction, so the rest of the binary is unaffected).

- [ ] **Step 7: Suggested commit grouping**

```bash
git add components/http/server/run_standalone components/http/server/CMakeLists.txt tests/integration_tests/http/test_http_run_standalone.cpp tests/integration_tests/http/CMakeLists.txt
# suggested message: "http/server: run_standalone — owned context/threads/signals, §9.7 sequence internalized"
```

---

### Task 9: Concurrency battery — 1000 requests across 4 io workers (§14.2)

**Files:**

- Create: `tests/integration_tests/http/test_http_server_concurrency.cpp`
- Modify: `tests/integration_tests/http/CMakeLists.txt` (add the source)

**Interfaces:**

- Consumes: fixture `start_server(configure, cfg, io_threads)` (Task 5), `TcpClient` keep-alive sends.
- Produces: nothing new — test-only. Exercises D5 (strand-per-run, per-connection strands) under a genuinely
  multi-threaded executor; the TSan step is the §14.2 "clean under TSan" gate.

- [ ] **Step 1: Write the test**

`tests/integration_tests/http/test_http_server_concurrency.cpp`:

```cpp
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <request_context.hpp>
#include <server.hpp>

#include "server_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;

namespace {

    class CountingController final : public HttpController {
    public:
        std::atomic<std::size_t> served{0};

        void configure_routes() override {
            Get("/ping", &CountingController::ping);
            Get("/users/{id}", &CountingController::user);
        }

    private:
        AsyncResponse ping(RequestContext ctx) {
            served.fetch_add(1, std::memory_order_relaxed);
            co_return ctx.ok("pong");
        }
        AsyncResponse user(RequestContext ctx) {
            served.fetch_add(1, std::memory_order_relaxed);
            co_return ctx.ok("user:" + std::to_string(ctx.path_param<int>("id").value_or(-1)));
        }
    };

}  // namespace

class ServerConcurrencyTest : public http_it::ServerIntegrationFixture {};

TEST_F(ServerConcurrencyTest, ThousandRequestsAcrossFourIoWorkers) {
    constexpr std::size_t CLIENTS    = 8;
    constexpr std::size_t PER_CLIENT = 125;  // 8 × 125 = 1000 (spec §14.2)

    auto ctrl = std::make_shared<CountingController>();
    start_server(
        [&](Server& s) {
            s.add_controller(ctrl);
            s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
        },
        ServerConfig{},
        /*io_threads=*/4);

    std::atomic<std::size_t> failures{0};
    std::vector<std::thread> clients;
    clients.reserve(CLIENTS);
    for (std::size_t c = 0; c < CLIENTS; ++c) {
        clients.emplace_back([&, c] {
            try {
                http_it::TcpClient client{port()};  // one keep-alive socket per client thread
                for (std::size_t j = 0; j < PER_CLIENT; ++j) {
                    const bool keep = j + 1 < PER_CLIENT;  // final request closes the socket
                    if ((c + j) % 2 == 0) {
                        const auto res = client.send(bhttp::verb::get, "/ping", {}, "text/plain", keep);
                        if (res.result_int() != 200u || res.body() != "pong") {
                            failures.fetch_add(1, std::memory_order_relaxed);
                        }
                    } else {
                        const auto id  = std::to_string(c * PER_CLIENT + j);
                        const auto res = client.send(bhttp::verb::get, "/users/" + id, {}, "text/plain", keep);
                        if (res.result_int() != 200u || res.body() != "user:" + id) {
                            failures.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                }
            } catch (...) {
                failures.fetch_add(PER_CLIENT, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : clients) {
        t.join();
    }

    EXPECT_EQ(failures.load(), 0u);
    EXPECT_EQ(ctrl->served.load(), CLIENTS * PER_CLIENT);
    // TearDown runs the full graceful shutdown under the 4-worker executor —
    // the shutdown paths get the same TSan coverage as the serve path.
}
```

- [ ] **Step 2: Add the source + build + run (debug)**

Add `test_http_server_concurrency.cpp` to the `${INTEGRATION_TESTING_TARGET}.Http.Server` source list.
Run:
`cmake --build build/debug --target Demiplane.Tests.Integration.Http.Server -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Http.Server" 2>&1 | tail -6`
Expected: PASS.

- [ ] **Step 3: TSan gate (if the preset configures — PR 4 note)**

Run:
`cmake --preset tsan && cmake --build build/tsan --target Demiplane.Tests.Integration.Http.Server -- -j4 && ctest --test-dir build/tsan --output-on-failure -R "Http.Server" 2>&1 | tail -8`
Expected: PASS with **zero TSan reports** (§14.2 "clean under TSan"). If the preset fails to configure on this
machine, record that in the task notes and rely on the debug run — do not silently skip.

- [ ] **Step 4: Suggested commit grouping**

```bash
git add tests/integration_tests/http/test_http_server_concurrency.cpp tests/integration_tests/http/CMakeLists.txt
# suggested message: "http/tests: §14.2 concurrency — 1000 keep-alive requests over a 4-worker executor, TSan-gated"
```

---

### Task 10: Verb / multi-param / URL-decode battery on the wire (resolves the `test_http_tcp.cpp` TODO)

**Files:**

- Modify: `tests/integration_tests/http/test_http_tcp.cpp` (extend `EchoController`, delete the TODO at line 17,
  append tests)
- Modify: `tests/integration_tests/http/http_test_fixture.hpp` (add `TcpClient::send_head`)

**Interfaces:**

- Consumes: PR 4 `HttpIntegrationFixture` (unchanged), landed driver/routing behavior — no product code changes.
- Produces: `TcpClient::send_head(std::string_view target) -> ParsedResponse` (Beast `response_parser` with
  `skip(true)` — a HEAD response carries headers only).

- [ ] **Step 1: Add `TcpClient::send_head`**

In `tests/integration_tests/http/http_test_fixture.hpp`, insert into `TcpClient` (after `read_response` from Task 6):

```cpp
        /// HEAD request. The response has no body regardless of
        /// Content-Length, so the parser must be told to skip it.
        ParsedResponse send_head(const std::string_view target) {
            bhttp::request<bhttp::empty_body> req{bhttp::verb::head, target, 11};
            req.set(bhttp::field::host, "127.0.0.1");
            bhttp::write(socket_, req);

            bhttp::response_parser<bhttp::string_body> parser;
            parser.skip(true);
            bhttp::read(socket_, buffer_, parser);
            return parser.release();
        }
```

- [ ] **Step 2: Extend the controller and delete the TODO**

In `tests/integration_tests/http/test_http_tcp.cpp`:

Delete line 17:

```cpp
    // TODO(PR5): broaden §14.2 coverage — PUT/PATCH/HEAD/OPTIONS verbs, multiple path params, and URL-decoded path/query *values*.
```

Extend `EchoController::configure_routes()` with:

```cpp
            Put("/items/{id}", &EchoController::put_item);
            Patch("/items/{id}", &EchoController::patch_item);
            Head("/hello", &EchoController::hello_head);
            Options("/hello", &EchoController::hello_options);
            Get("/users/{id}/posts/{post_id}", &EchoController::user_post);
            Get("/files/{name}", &EchoController::file_name);
            Get("/search", &EchoController::search);
```

and add the private handlers:

```cpp
        AsyncResponse put_item(RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(1 << 20);
            co_return ctx.ok("put:" + ctx.path_param<std::string>("id").value_or("?") + ":"
                             + std::move(body).value());
        }
        AsyncResponse patch_item(RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(1 << 20);
            co_return ctx.ok("patch:" + ctx.path_param<std::string>("id").value_or("?") + ":"
                             + std::move(body).value());
        }
        AsyncResponse hello_head(RequestContext ctx) {
            // Empty body — a HEAD response must not carry a payload.
            co_return ctx.status(HttpStatus::ok).set_header("X-Head-Route", "yes");
        }
        AsyncResponse hello_options(RequestContext ctx) {
            co_return ctx.status(HttpStatus::no_content).set_header("Allow", "GET, HEAD, OPTIONS");
        }
        AsyncResponse user_post(RequestContext ctx) {
            co_return ctx.ok(ctx.path_param<std::string>("id").value_or("?") + "|"
                             + ctx.path_param<std::string>("post_id").value_or("?"));
        }
        AsyncResponse file_name(RequestContext ctx) {
            co_return ctx.ok("file:" + ctx.path_param<std::string>("name").value_or("?"));
        }
        AsyncResponse search(RequestContext ctx) {
            co_return ctx.ok("q=" + ctx.query_or<std::string>("q", "none"));
        }
```

- [ ] **Step 3: Append the tests**

```cpp
TEST_F(HttpTcpTest, PutRoundTripsBodyAndParam) {
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::put, "/items/7", "new-name");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "put:7:new-name");
}

TEST_F(HttpTcpTest, PatchRoundTripsBodyAndParam) {
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::patch, "/items/9", "delta");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "patch:9:delta");
}

TEST_F(HttpTcpTest, HeadDispatchesHeadersWithoutBody) {
    http_it::TcpClient client{port_};
    const auto res = client.send_head("/hello");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(std::string(res["X-Head-Route"]), "yes");
    EXPECT_TRUE(res.body().empty());
}

TEST_F(HttpTcpTest, OptionsReturnsAllow) {
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::options, "/hello");
    EXPECT_EQ(res.result_int(), 204u);
    EXPECT_NE(std::string(res["Allow"]).find("GET"), std::string::npos);
}

TEST_F(HttpTcpTest, MultiplePathParamsCapture) {
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::get, "/users/42/posts/777");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "42|777");
}

TEST_F(HttpTcpTest, PathParamValuesUrlDecode) {
    // %20 decodes to space; a raw '+' in a PATH stays literal
    // (plus_is_space=false — spec §8.6).
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::get, "/files/a%20b+c");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "file:a b+c");
}

TEST_F(HttpTcpTest, QueryParamValuesUrlDecodePlusAsSpace) {
    // In a QUERY both %20 and '+' decode to space (plus_is_space=true).
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::get, "/search?q=a%20b+c");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "q=a b c");
}
```

- [ ] **Step 4: Build + run**

Run:
`cmake --build build/debug --target Demiplane.Tests.Integration.Http.Tcp -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Http.Tcp" 2>&1 | tail -6`
Expected: PASS — old and new wire tests green. (These exercise landed PR 2/PR 3 behavior; a failure here is a real
finding in routing/driver code, not in the tests — investigate before "fixing" the assertion. The decode expectations
follow spec §8.6 exactly.)

- [ ] **Step 5: Suggested commit grouping**

```bash
git add tests/integration_tests/http/test_http_tcp.cpp tests/integration_tests/http/http_test_fixture.hpp
# suggested message: "http/tests: §14.2 wire battery — PUT/PATCH/HEAD/OPTIONS, multi-param, path/query decode"
```

---

### Task 11: Resolve the remaining `TODO(PR5)` markers (listeners + tracker)

**Files:**

- Modify: `components/http/listeners/tcp_listener/tcp_listener.hpp`
- Modify: `components/http/listeners/tls_listener/tls_listener.hpp`
- Modify: `components/http/listeners/connection_tracker/connection_tracker.cpp`

**Interfaces:** comment-only — no code changes. After this task `grep -rn "TODO(PR5)" --include="*.hpp"
--include="*.cpp"` over the repo returns nothing.

- [ ] **Step 1: `tcp_listener.hpp` — lifetime-contract WARN (lines 43-46)**

Replace:

```cpp
     * // TODO(PR5) WARN: drain_until only DISPATCHES force-cancels; it does not await the cancelled serve()
     * coroutines' unwind on ANY executor (cancels + unwinds run in later executor turns). The integration
     * fixture bridges this by polling in_flight() == 0 after drain; the PR5 Server must do the same or await
     * a completion signal fired by the last Handle::release(). See ConnectionTracker::drain_until.
```

with:

```cpp
     * WARN: drain_until only DISPATCHES force-cancels; it does not await the cancelled serve()
     * coroutines' unwind on ANY executor (cancels + unwinds run in later executor turns). The Server
     * honors this in graceful_shutdown() phase 2.5 by polling in_flight() == 0 after the drain; direct
     * users (the integration fixture) must do the same. See ConnectionTracker::drain_until.
```

- [ ] **Step 2: `tcp_listener.hpp` — multi-worker WARN (lines 83-87)**

Replace:

```cpp
            // TODO(PR5) WARN: multi-worker residual — an emit landing between the
            // loop-top state check and async_accept installing its cancel handler
            // is edge-lost; that accept then parks until the next inbound SYN
            // (self-heals on any connection attempt; the post-loop close still
            // runs). Unreachable on the single-threaded v1 executor.
```

with:

```cpp
            // WARN: multi-worker — an emit landing between the loop-top state
            // check and async_accept installing its cancel handler would be
            // edge-lost. The check→install section runs in ONE executor turn,
            // so the window is closed whenever the emit is serialized with
            // this coroutine's turns: the Server spawns run() on a dedicated
            // strand and DISPATCHES the stop emit onto it (PR 5, D5). Direct
            // users on a multi-threaded executor must do the same.
```

- [ ] **Step 3: `tls_listener.hpp` — lifetime-contract WARN (lines 49-52)**

Replace:

```cpp
     * // TODO(PR5) WARN: drain_until only DISPATCHES force-cancels; it does not await the cancelled serve()
     * coroutines' unwind on ANY executor (cancels + unwinds run in later executor turns). The integration
     * fixture bridges this by polling in_flight() == 0 after drain; the PR5 Server must do the same or await
     * a completion signal fired by the last Handle::release(). See ConnectionTracker::drain_until.
```

with the same four replacement lines as Step 1.

- [ ] **Step 4: `tls_listener.hpp` — run() rationale pointer (lines 87-89)**

Replace:

```cpp
            // Cancellation as STATE, not exceptions — see TcpListener::run for the
            // full rationale (incl. the TODO(PR5) WARN about the multi-worker
            // edge-lost emit window between the state check and accept install).
```

with:

```cpp
            // Cancellation as STATE, not exceptions — see TcpListener::run for the
            // full rationale (incl. the multi-worker WARN: stop emits must be
            // dispatched onto this run()'s strand, as the Server does — PR 5, D5).
```

- [ ] **Step 5: `connection_tracker.cpp` — drain WARN (lines 46-50)**

Replace:

```cpp
        // TODO(PR5) WARN: this only DISPATCHES force-cancels; it does not wait for the cancelled serve() coroutines
        // to unwind (in_flight_ -> 0) — on ANY executor: the cancels and the unwinds run in later executor turns, so
        // drain returning never implies the frames are gone. Callers must wait for in_flight() == 0 before destroying
        // the owning listener (the integration fixture polls it in TearDown; the PR5 Server must poll or await a
        // completion signal fired by the last Handle::release()).
```

with:

```cpp
        // WARN: this only DISPATCHES force-cancels; it does not wait for the cancelled serve() coroutines
        // to unwind (in_flight_ -> 0) — on ANY executor: the cancels and the unwinds run in later executor turns, so
        // drain returning never implies the frames are gone. Callers must wait for in_flight() == 0 before destroying
        // the owning listener (Server::graceful_shutdown phase 2.5 polls it; the integration fixture polls in
        // TearDown).
```

- [ ] **Step 6: Verify nothing is left and nothing broke**

Run:
`grep -rn "TODO(PR5)" /home/grivin/Workspace/Demiplane --include="*.hpp" --include="*.cpp" --include="*.md" | grep -v docs/superpowers/plans`
Expected: no output.
Run: `cmake --build build/debug -- -j4 && ctest --test-dir build/debug --output-on-failure -L http 2>&1 | tail -6`
Expected: PASS (comment-only changes).

- [ ] **Step 7: Suggested commit grouping**

```bash
git add components/http/listeners
# suggested message: "http/listeners: resolve TODO(PR5) markers — contracts now honored by Server (D5, D6)"
```

---

### Task 12: Spec sync (§9 + decisions log)

**Files:**

- Modify: `docs/superpowers/specs/2026-05-07-http-redesign-design.md`

The spec is the reference for PR 6/PR 7 — it must not contradict the landed PR 5 surface. Targeted edits only.

- [ ] **Step 1: Status line**

Extend the `**Status:**` line (line 5) with:
`PR 5 plan: docs/superpowers/plans/2026-07-05-http-server-lifecycle-layer.md.`

- [ ] **Step 2: §9.1 — listener-adder signatures + setup comment**

In the `Server` code block (§9.1), replace the three adder declarations:

```cpp
    template <IsHttpDriver Driver>
    Server& add_tcp_listener(std::string bind, Driver driver);

    template <IsHttpDriver... Drivers>
    Server& add_tls_listener(std::string bind, TlsConfig tls, Drivers... drivers);

    template <IsHttpDriver Driver>
    Server& add_quic_listener(std::string bind, TlsConfig tls, Driver driver);
```

with:

```cpp
    // (host, port) split matches the landed listener ctors (PR 5 D7); the
    // config-driven path is attach_default_listeners (PR 6).
    template <IsHttpDriver Driver>
    Server& add_tcp_listener(std::string host, std::uint16_t port, Driver driver);

    template <IsHttpDriver... Drivers>
    Server& add_tls_listener(std::string host, std::uint16_t port, TlsConfig tls, Drivers... drivers);

    template <IsHttpDriver Driver>
    Server& add_quic_listener(std::string host, std::uint16_t port, TlsConfig tls, Driver driver);
```

- [ ] **Step 3: §9.2 — ServerObserver as landed**

Replace the §9.2 code block with:

```cpp
// Request identity snapshot for on_response: the RequestContext is CONSUMED
// by value by the handler chain, so it no longer exists when the response is
// available (PR 5 D3). Views stay valid through the hook call.
struct RequestInfo {
    HttpMethod       method;
    std::string_view target;
};

class ServerObserver {
public:
    virtual ~ServerObserver() = default;

    virtual asio::awaitable<void> on_setup_complete() { co_return; }
    virtual asio::awaitable<void> on_shutdown_started() { co_return; }
    virtual void on_shutdown_complete() noexcept {}

    virtual void on_request(const RequestContext&) noexcept {}
    virtual void on_response(const RequestInfo&, const Response&) noexcept {}

    // Captures std::exception_ptr (heap-managed) — fixes the original UAF.
    virtual void on_unhandled_exception(std::exception_ptr) noexcept {}
};
```

and append after the existing explanatory paragraphs:

> Per-request hooks are wired by `Server::setup()` into the `Router` as plain nullable `std::function`s (PR 5 D2) —
> the routing layer carries no server-layer dependency and no observer inheritance. `Router::dispatch` fires
> `on_request` at entry, `on_response` for handler successes and routing-miss 404/405s, and
> `on_unhandled_exception` (then **rethrows**) for handler escapes — so the driver-synthesized 500 and driver-level
> early responses (malformed 400, limit 4xx) are not observed. Hooks may fire concurrently from any executor thread;
> implementations must be thread-safe and allocation-free on the hot path.

- [ ] **Step 4: §9.3 — setup() steps as landed**

Replace the §9.3 numbered list with:

```markdown
1. Validate state is `build`; validate at least one listener. (Thread count is the caller's concern — the Server
   owns no threads.)
2. `registry_.freeze()` → if conflicts, throw `RouteConflictAggregateError` (all conflicts in one throw).
3. Call `initialize()` on every controller, add order (PR 5 D8); a throw aborts setup.
4. Call `bind()` on every listener synchronously. Bind failures throw.
5. Wire the observer fan-out hooks into the Router (D2; skipped when no observers).
6. `co_spawn` each listener's `run(router_)` on **its own strand** of `exec_` (D5 — the stop emit must be
   serialized with the loop's turns), each bound to **its own** `listener_stop_signals_[i]->slot()` — a slot holds
   a single handler, so the signal cannot be shared across listeners (§7.2).
7. Detach-spawn the `on_setup_complete` notification coroutine on `exec_` (sequential, add order) — `setup()`
   itself never blocks (D4).
8. Set `state_` to `running`.
```

- [ ] **Step 5: §9.5 — phases 1.5 / 2.5**

In the `graceful_shutdown` code block, replace the Phase 1 + Phase 2 section:

```cpp
    // Phase 1: cancel accept loops (new connections refused) — one signal per
    // listener (§9.3); a single shared slot would cancel only the last loop.
    for (auto& sig : listener_stop_signals_)
        sig->emit(asio::cancellation_type::terminal);

    // Phase 2: drain in-flight requests up to drain_timeout
    auto drain_deadline = std::chrono::steady_clock::now() + cfg_.drain_timeout;
    for (auto& l : listeners_) co_await l->drain_until(drain_deadline);
```

with:

```cpp
    // Phase 1: cancel accept loops — one signal per listener (§9.3), each
    // emit DISPATCHED onto the strand its run() executes on (D5).
    for (std::size_t i = 0; i < listeners_.size(); ++i)
        asio::dispatch(run_strands_[i],
                       [sig = listener_stop_signals_[i].get()] { sig->emit(asio::cancellation_type::terminal); });

    // Phase 1.5: poll accept-loop completion — acceptors provably closed,
    // new connections REFUSED from here on (D6).
    while (live_accept_loops_ > 0) co_await tick(5ms);

    // Phase 2: drain in-flight requests up to drain_timeout (shared deadline).
    auto drain_deadline = std::chrono::steady_clock::now() + cfg_.drain_timeout;
    for (auto& l : listeners_) co_await l->drain_until(drain_deadline);

    // Phase 2.5: unwind barrier (D6) — drain only DISPATCHES force-cancels;
    // poll Σ in_flight() == 0 before touching listener lifetimes. Unbounded:
    // a suspended frame can only be freed by completion.
    while (total_in_flight() > 0) co_await tick(5ms);
```

- [ ] **Step 6: §15 Decisions Log — append rows**

Append to the decisions table:

```markdown
| Observer per-request hooks | Fired in `Router::dispatch` via nullable `std::function` hooks the Server wires at `setup()` (PR 5 D2)                                    | No observer interface below the server layer, no dynamic-inheritance split (user decision 2026-07-05); routing keeps zero server dependency; unset hooks cost one null check |
| `on_response` signature    | `(const RequestInfo&, const Response&)` — snapshot of {method, target} taken at dispatch entry (PR 5 D3)                                   | The spec's original `(const RequestContext&, …)` was unimplementable: the ctx is consumed by value by the handler chain                                                       |
| `setup()` observer barrier | Non-blocking: `on_setup_complete` notified via a detach-spawned coroutine on `exec_` (PR 5 D4)                                            | §9.1 "does NOT block" beats §9.3's barrier — a blocking barrier deadlocks any caller that drives the executor only after setup()                                              |
| Accept-loop threading      | One strand per listener `run()`; stop emits dispatched onto that strand (PR 5 D5)                                                          | `cancellation_signal::emit` must be serialized with the loop's turns on a multi-threaded executor; closes the edge-lost-emit window                                           |
| Drain-unwind completion    | `graceful_shutdown` polls `live_accept_loops_ == 0` (phase 1.5) and `Σ in_flight() == 0` (phase 2.5) on a 5 ms tick (PR 5 D6)              | Force-cancels are only dispatched; frames unwind in later turns. Polling is the fixture-proven pattern; a tracker completion event adds cross-strand surface for little gain  |
| `ServerConfig` staging     | Plain struct (`request_arena_size`, `drain_timeout`, `path_normalization`) at the final path; PR 6 rewrites in place (PR 5 D1, PR 4 D1)    | Same staging pattern as `TlsConfig`/`Http11Config`; config layer keeps its own PathNormalization enum so it carries no routing dependency                                     |
```

- [ ] **Step 7: Verify the spec still reads coherently**

Re-read §9 top to bottom once; confirm no remaining §9 text contradicts the landed signatures (in particular no
remaining reference to a blocking observer barrier or a shared stop signal).

- [ ] **Step 8: Suggested commit grouping**

```bash
git add docs/superpowers/specs/2026-05-07-http-redesign-design.md
# suggested message: "docs: sync spec §9 + decisions log with the landed PR5 Server (D1-D9)"
```

---

## Final acceptance sweep (after all tasks)

- [ ] `grep -rn "TODO(PR5)" . --include="*.hpp" --include="*.cpp"` → empty.
- [ ] `cmake --build build/debug -- -j4` → clean.
- [ ] `ctest --test-dir build/debug --output-on-failure -L http` → all pass.
- [ ] `ctest --test-dir build/tsan --output-on-failure -R "Http.Server"` (if tsan configures) → all pass, no reports.
- [ ] `ctest --test-dir build/asan --output-on-failure -L http` (if asan configures) → all pass, no reports.

## Coverage traceability (spec §14.2 lifecycle/observer/concurrency → tests)

| §14.2 requirement                                                     | Test                                                                                          |
|-----------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|
| `setup()` failure on port-in-use                                      | `ServerLifecycleTest.SetupThrowsWhenPortInUse`                                                |
| Injected executor: live after `setup()`, `wait_until_stopped()` gates | `ServerLifecycleTest.ServesAndStopsGracefully`                                                |
| `stop()` idempotent, never stops the caller's executor                | `StopIsIdempotentAndWaitReturnsAgain`, executor-post assert in `ServesAndStopsGracefully`     |
| SIGINT via `run_standalone` → graceful shutdown                       | `RunStandaloneTest.ServesAndStopsOnSigint`                                                    |
| Registration after `setup()` throws                                   | `ServerLifecycleTest.RegistrationAfterSetupThrows`                                            |
| Conflicts aggregated at `setup()`                                     | `ServerLifecycleTest.ConflictingRoutesThrowAggregateAtSetup`                                  |
| Graceful: in-flight completes                                         | `ServerLifecycleTest.GracefulShutdownCompletesInFlightRequests`                               |
| Graceful: new connections refused                                     | `ServerLifecycleTest.NewConnectionsRefusedAfterShutdown`                                      |
| Graceful: `on_shutdown_started` runs and is awaited                   | `ServerObserverTest.LifecycleAndRequestHooksFireInOrder`                                      |
| Graceful: drain timeout force-cancels remaining                       | `ServerLifecycleTest.DrainDeadlineForceCancelsStragglers`                                     |
| Concurrency: 1000 requests across N workers, TSan-clean               | `ServerConcurrencyTest.ThousandRequestsAcrossFourIoWorkers` (+ tsan)                          |
| Observer: `on_request`/`on_response` fire in order                    | `ServerObserverTest.LifecycleAndRequestHooksFireInOrder`, `ResponseHookFiresForRoutingMisses` |
| Observer: `on_unhandled_exception` on handler throw                   | `ServerObserverTest.UnhandledExceptionHookFiresAndClientGets500` (+ Router unit tests)        |
| §14.2 verbs/params/decode broadening (`TODO(PR5)`)                    | Task 10 battery in `test_http_tcp.cpp`                                                        |
| `async_wait_stopped()` awaitable twin (§9.1)                          | `ServerLifecycleTest.AsyncWaitStoppedCompletesWithShutdown`                                   |
| Server-level group mounting (§8.4 `in_group`)                         | `ServerLifecycleTest.GroupPrefixMountsControllerAtServerLevel`                                |

