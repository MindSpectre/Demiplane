# HTTP Redesign — PR 4: Listeners + TLS Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the transport layer that turns the landed PR-3 connections + drivers into a *running* HTTP server:
`ListenerBase` (the one polymorphic seam), `ConnectionTracker` (in-flight counting + graceful force-cancel),
`TcpListener<Driver>`, `TlsConnection` + `build_ssl_context` + `TlsListener<Drivers...>` (TCP+TLS+ALPN dispatch), and a
`QuicListener` scaffold — plus the project's **first HTTP integration tests** exercising h1-over-TCP and h1-over-TLS on a
real `127.0.0.1:0` socket via a Boost.Beast client. Spec: §7 + §12.2 PR 4 of
`docs/superpowers/specs/2026-05-07-http-redesign-design.md`.

**Architecture:** A thin transport layer over the landed Connection (PR 3) and Driver (PR 3) layers. A *listener* owns an
`asio::ip::tcp::acceptor` on a caller-injected `any_io_executor` (there is no `Server` yet — PR 5); `bind()` opens it
synchronously (failures throw immediately), `run(Router&)` is a cancellable accept loop that accepts **directly onto a
fresh per-connection strand**, heap-allocates the (non-movable) connection as a `shared_ptr`, registers it with the
tracker, and `co_spawn`s `driver.serve(conn, router)` on that strand. `ListenerBase` is the *only* virtual interface in
the runtime path — the Server (PR 5) will hold `unique_ptr<ListenerBase>`. The `ConnectionTracker` holds a `weak_ptr` +
a force-cancel thunk per connection; `drain_until(deadline)` polls the in-flight count and, on expiry, dispatches an
`emit(terminal)` onto each surviving connection's strand. `TlsListener` carries a tuple of drivers, advertises the union
of their ALPN ids, and after the TLS handshake dispatches to the driver whose `id()` matches the negotiated protocol.

**Tech Stack:** C++23 (concepts, coroutines, variadic templates, `std::pmr`), Boost.Asio (`any_io_executor`,
`make_strand`, `co_spawn`, `bind_cancellation_slot`, `cancellation_signal`, `steady_timer`), Boost.Asio **SSL**
(`ssl::context`, `ssl::stream<beast::tcp_stream>`, ALPN via raw OpenSSL `SSL_CTX_set_alpn_select_cb` /
`SSL_select_next_proto`), OpenSSL 3.6.3 (`OpenSSL::SSL` / `OpenSSL::Crypto`), Boost.Beast (`tcp_stream`, `ssl_stream`,
sync + async HTTP client/server), the landed PR1–PR3 leaf targets, GoogleTest. Dotted target names only — there are no
`::` aliases (see Reconciliation).

---

## Reconciliation against the landed code + spec (read before executing)

The spec §7 sketch predates the landed Connection/Driver layers. These are the facts on the ground and the deviations
this plan deliberately takes; each load-bearing mechanism was verified by a **compiled spike** (results below).

### Landed facts (authoritative over the spec)

1. **Per-leaf CMake convention, dotted names only.** `${DMP_HTTP}` = `${DMP_COMPONENT}.HTTP` =
   `Demiplane.Component.HTTP`. Each "thing" is its own leaf target owning its include dir (headers cross-reference as
   `<router.hpp>`, never `../`); an INTERFACE target aggregates each layer (`${DMP_HTTP}.Connection`,
   `${DMP_HTTP}.Drivers`, …). **No `::` aliases.** The public umbrella (`add_combined_library(${DMP_HTTP} …)`) has an
   **empty `LIBRARIES` list** — it is populated in PR 7, so this PR does **not** touch it.
2. **`TcpConnection`** (`<tcp_connection.hpp>`): `class TcpConnection : gears::Immutable` (non-copy, non-move),
   `using stream_type = boost::beast::tcp_stream`, ctor `explicit TcpConnection(boost::asio::ip::tcp::socket socket,
   std::size_t arena_size = 8192)`. Methods: `stream()`, `arena_alloc()`, `reset_request_arena()`,
   `expires_after(ms)`, `awaitable<void> async_close()`, `cancel_slot()` (returns `signal_.slot()`), `remote_address()`,
   `static negotiated_protocol() -> Protocol::http1`, `static is_secure() -> false`. Private member
   `boost::asio::cancellation_signal signal_`. **It has no `cancel()` method yet — this PR adds one (D2).**
3. **`Connection` / `StreamConnection` concepts** (`<connection_concepts.hpp>`) as landed — arena + lifecycle +
   metadata, refined by `StreamConnection`'s `stream()`. `cancel()` is **not** in the concept and this PR does not add it
   there (D2).
4. **`Http11Driver`** (`<http11_driver.hpp>`): `explicit Http11Driver(const Http11Config&)`,
   `static constexpr Protocol id()`, `static constexpr std::span<const std::string_view> accepted_alpns()`,
   `template <StreamConnection ConnT> awaitable<void> serve(ConnT& conn, Router& router)`. `Http2Driver` mirrors with
   `Protocol::http2` / `"h2"`; `Http3Driver` with `Protocol::http3` / `"h3"` and a `template <Connection ConnT> serve`.
5. **`Router`** (`<router.hpp>`): `explicit Router(const RouteRegistry&)`, `awaitable<Response> dispatch(RequestContext)
   const`. **`HttpDriver`** concept (`<http_driver_concept.hpp>`): `{T::id()} -> Protocol`, `{T::accepted_alpns()} ->
   std::span<const std::string_view>`.
6. **Enums** (`<http_enums.hpp>`): `enum class Protocol : std::uint8_t { http1, http2, http3 }`. `HttpMethod` has 8
   values (`unknown` at 0); `HttpStatus`/`HttpVersion` as landed.
7. **`gears` idioms**: `gears::Immutable` (deletes copy+move), `gears::NonCopyable` (move-only),
   `gears::force_non_static(this)` / `gears::force_non_const(this)` static guards (used by scaffolds to keep a method
   non-static/non-const without state). `gears::Outcome` API: `gears::err(E{})`, `.value()`, `operator bool`,
   `.visit(...)`. Headers: `<demiplane/gears>` subheaders or the rooted leaf includes already on the link path.
8. **Scroll logging**: `COMPONENT_LOG_INF/WRN/ERR/DBG()` paired with a class-scope `SCROLL_COMPONENT_PREFIX("Name")`;
   no-ops unless `DMP_COMPONENT_LOGGING`. Available via `#include <demiplane/scroll>`; link `Demiplane::Common::Scroll`.
9. **OpenSSL** is project-wide: `find_package(OpenSSL REQUIRED)` at `CMakeLists.txt:101`; targets `OpenSSL::SSL` /
   `OpenSSL::Crypto` (used in `common/crypto/CMakeLists.txt`). `boost/asio/ssl.hpp` and `boost/beast/ssl.hpp` are present
   in the vcpkg install (`build/<preset>/vcpkg_installed/x64-linux-clang/include`). **This PR introduces the codebase's
   first `asio::ssl` usage** (verified to compile + link + handshake — see spike S2).
10. **Test harness**: `add_unit_test(<target> <sources…>)` then a separate `target_link_libraries(<target> PRIVATE …
    ${TEST_LIBS})`, label `unit`. `add_integration_test(<target> <sources…> LINK_LIBS … LABELS "…" ENVIRONMENT …)`,
    label `integration`. `${UNIT_TESTING_TARGET}` = `Demiplane.Tests.Unit`; `${INTEGRATION_TESTING_TARGET}` =
    `Demiplane.Tests.Integration`; `${TEST_LIBS}` = the four GTest targets. `tests/integration_tests/http/` does **not**
    exist yet — this PR creates it and wires it under the `if (BUILD_HTTP)` block of `tests/CMakeLists.txt`.
11. **Build preset**: the **`debug`** preset (`build/debug`) configures + builds the HTTP component cleanly on the
    current branch (verified 2026-06-15). PR 3 noted `release` failing to configure under the project-local vcpkg
    toolchain; **use `--preset debug` / `build/debug` for all per-task verification**. Sanitizer steps use `asan` (UAF on
    the tracker) / `tsan` (the multi-thread accept path) *if they configure*; otherwise fall back to `debug`. Work
    happens on the current branch `component/http-1.1/v1.4`; **no git commits in this execution** — the user manages git
    (the per-task `git` blocks below are the recommended grouping for when the user does commit).

### Deviations taken (each documented in the relevant task)

- **D1 — Plain-struct `TlsConfig` at the spec's final path now; PR 6 rewrites it in place.** Spec §10.1's `TlsConfig` is
  a `serialization::ConfigInterface` class, but that layer (with its `MinVersion` enum-encoding question, spec §16) is
  PR 6. Following the PR-3 precedent (`Http11Config` is a plain struct fed later by config), this PR ships a **plain
  `struct TlsConfig`** carrying exactly the fields `build_ssl_context` consumes. It lives at the spec's final location
  `config/tls_config/tls_config.hpp` (so the include path is stable); PR 6 **rewrites that file** into the
  `ConfigInterface`-backed version and `build_ssl_context` switches `.cert_file` → `.cert_file()`. Naming it `TlsConfig`
  now avoids a same-namespace redefinition collision when PR 6 lands; placing it under `config/` (not `listeners/`)
  avoids a later file move.
- **D2 — `ConnectionTracker` stores a `weak_ptr` + a force-cancel thunk, not the spec's literal
  `std::list<asio::cancellation_signal>`.** The landed `TcpConnection` **owns its own** `cancellation_signal` (the
  driver binds I/O to `conn.cancel_slot()`), so a tracker-owned signal list would be a second, disconnected signal. This
  plan instead: (a) adds a tiny `void cancel() noexcept` to `TcpConnection`/`TlsConnection` that emits `terminal` on the
  connection's own signal; (b) the tracker stores `weak_ptr<void>` + a thunk `[strand](shared_ptr<void> c){
  asio::dispatch(strand, [c]{ static_pointer_cast<Conn>(c)->cancel(); }); }`. Force-cancel does `if (auto c =
  wp.lock()) thunk(c);` — the `weak_ptr` defeats the use-after-free where a connection's `serve` completes and destroys
  the connection between the snapshot and the dispatched emit (spike S1). `cancel()` is **not** added to the `Connection`
  concept (that would ripple into `TestConnection`/`QuicConnection`); the tracker requires it structurally on the
  concrete type.
- **D3 — Listeners take an injected `any_io_executor`; there is no `Server` yet.** Spec §9's executor injection lands in
  PR 5. Here each listener ctor takes `(asio::any_io_executor, std::string host, std::uint16_t port, …)` and integration
  tests drive `run()`/`drain_until()` directly via a fixture that owns the `io_context` + worker thread + stop signal.
  PR 5's `Server::setup()` will `co_spawn(listener->run(router_), bind_cancellation_slot(per_listener_signal, …))` and
  `Server::graceful_shutdown()` will call `listener->drain_until(deadline)` — the exact contract this PR's fixture
  exercises.
- **D4 — `build_ssl_context` sets the ALPN-select callback with the listener's member buffer as `arg`.** The advertised
  ALPN wire-format buffer must outlive the `SSL_CTX` (spec §16). The `TlsListener` owns `std::string advertised_alpn_`
  (built from the driver tuple at construction) and passes it **by reference** to `build_ssl_context`, which stores
  `&advertised_alpn_` as the C-callback `arg`. Because the buffer and the `ssl::context` are both `TlsListener` members,
  the arg outlives the context. The ALPN failure return is **`SSL_TLSEXT_ERR_ALERT_FATAL`** (spike S2 — OpenSSL 3.6.3
  does **not** define `SSL_TLSEXT_ERR_ALPN_FAILED`; ALERT_FATAL aborts the handshake with `no_application_protocol`,
  giving the spec's "client offering only h2 → connection closes" behavior).
- **D5 — Integration TLS cert is an embedded PEM written to a temp file in fixture `SetUp()`.** Hermetic (no committed
  key, no `openssl` CLI / runtime cert-gen dependency) while still exercising `build_ssl_context`'s **file-loading** path
  (`use_certificate_chain_file` / `use_private_key_file`). The embedded self-signed cert (CN=localhost, SAN
  IP:127.0.0.1) is valid until 2126.
- **D6 — `QuicListener` is an INTERFACE scaffold.** `bind()` succeeds (no-op), `run()` returns immediately; a
  `static_assert` enforces it is paired with `Http3Driver` only (spec §7.3). It links no ngtcp2/nghttp3 symbols
  (continues PR 3's D4). QUIC is not tracked/drained (no real connections).
- **D7 — `accept`-onto-strand, not accept-then-rebind.** `beast::tcp_stream` caches its executor at construction, so the
  accept loop calls `acceptor_.async_accept(asio::make_strand(exec_), …)` to get a socket already bound to the
  per-connection strand, then constructs the connection and `co_spawn`s `serve` on `conn->stream().get_executor()` (=
  that strand). This keeps the stream's timer + I/O and the tracker's force-cancel all on one strand (spike S1).

### Validated mechanisms (spike results the executor can trust)

Both spikes were compiled with the exact `build/debug` toolchain flags (`clang++ -stdlib=libc++ -std=c++23` +
`-isystem …/vcpkg_installed/x64-linux-clang/include`, linking `-lssl -lcrypto -lpthread`) and run.

| Mechanism                                                                                                          | Result                                                                                         |
|--------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------|
| **S1** `ConnectionTracker`: `weak_ptr` entry + `asio::dispatch(strand, [c]{ c->cancel(); })` force-cancel, 200 conns, 4-thread `io_context`, short drain deadline | clean under **plain + ASan/UBSan** (no leaks, no UAF); `in_flight → 0`; surviving conns unwind |
| **S1** Connection lifetime: `make_shared<Conn>`, `co_spawn(strand, [conn, h=move(handle)]{ co_await serve; })`     | conn + tracker `Handle` both live for the coroutine's lifetime; destroyed cleanly after unwind |
| **S2** `ssl::stream<beast::tcp_stream>` server + client handshake over `127.0.0.1:0`                               | compiles, links (`OpenSSL::SSL`/`Crypto`), handshakes                                          |
| **S2** ALPN: `SSL_CTX_set_alpn_select_cb` + `SSL_select_next_proto`, arg = `std::string` member outliving the ctx  | server (advertising `http/1.1`) + client (offering `h2`,`http/1.1`) negotiate **`http/1.1`**   |
| **S2** Compile-time driver-tuple walk (`std::index_sequence` fold) → invoke the runtime-ALPN-selected driver       | selects the h1 driver from `std::tuple<H1>`                                                     |
| **S2** ALPN mismatch (h1-only server, h2-only client) → return `SSL_TLSEXT_ERR_ALERT_FATAL`                        | **both** handshakes fail: "no application protocol" / "tlsv1 alert no application protocol"     |
| **S3** D7 accept-onto-strand: `acceptor.async_accept(make_strand(exec), …)` → assign to `tcp::socket` → `beast::tcp_stream{std::move(sock)}` | compiles; returned socket converts to `tcp::socket`; its executor **is** the strand; round-trips |
| **S4** accept-loop stop: `co_spawn(…, bind_cancellation_slot(sig, detached))` → `sig.emit(terminal)` → `async_accept` returns `operation_aborted`; loop closes the acceptor | co_spawn-slot cancellation reaches `async_accept`; after `acceptor.close()` a fresh connect gets **"Connection refused"** (not a backlog hang) |
| **env** OpenSSL `3.6.3`, Boost `1.91`; `SSL_TLSEXT_ERR_ALPN_FAILED` **undefined** → use `SSL_TLSEXT_ERR_ALERT_FATAL` (needs `#include <openssl/tls1.h>`) | confirmed                                                                                       |

---

## File Structure

```
components/http/config/                               ← NEW layer (PR 6 grows it)
├─ CMakeLists.txt                                     aggregate INTERFACE ${DMP_HTTP}.Config
└─ tls_config/   {tls_config.hpp, CMakeLists.txt}     INTERFACE ${DMP_HTTP}.Config.TlsConfig   (plain struct, D1)

components/http/connection/
└─ tls_connection/ {tls_connection.hpp, tls_connection.cpp, CMakeLists.txt}  STATIC ${DMP_HTTP}.Connection.Tls
   (modify tcp_connection/tcp_connection.hpp → + cancel())
   (modify connection/CMakeLists.txt → + add_subdirectory(tls_connection) + link)

components/http/listeners/                            ← NEW layer
├─ CMakeLists.txt                                     aggregate INTERFACE ${DMP_HTTP}.Listeners (grows per task)
├─ listener_base/      {listener_base.hpp, CMakeLists.txt}                          INTERFACE ${DMP_HTTP}.Listeners.Base
├─ connection_tracker/ {connection_tracker.hpp, connection_tracker.cpp, CMakeLists.txt}  STATIC ${DMP_HTTP}.Listeners.ConnectionTracker
├─ tcp_listener/       {tcp_listener.hpp, CMakeLists.txt}                            INTERFACE ${DMP_HTTP}.Listeners.Tcp  (header template)
├─ tls_listener/       {build_ssl_context.hpp, build_ssl_context.cpp, tls_listener.hpp, CMakeLists.txt}  STATIC ${DMP_HTTP}.Listeners.Tls
└─ quic_listener/      {quic_listener.hpp, CMakeLists.txt}                           INTERFACE ${DMP_HTTP}.Listeners.Quic  (scaffold, D6)

tests/unit_tests/http/listeners/
├─ test_connection_tracker.cpp     RAII counter, drain-early, force-cancel-on-deadline (synthetic conns)
├─ test_build_ssl_context.cpp      context builds from a temp cert; bad cert path throws
└─ test_quic_listener.cpp          scaffold: bind() ok, run() returns, Http3Driver-only static_assert

tests/integration_tests/http/
├─ CMakeLists.txt
├─ http_test_fixture.hpp           io_context + worker + TcpListener on :0 + Beast client helpers + embedded TLS cert
├─ test_http_tcp.cpp               verbs, path/query params, keep-alive, 404/405, handler-exception→500, drain/force-cancel
└─ test_http_tls.cpp               TLS handshake, ALPN negotiates http/1.1, round trip, h2-only offer → connection closes

Modified:
├─ components/http/CMakeLists.txt                          + add_subdirectory(config) + add_subdirectory(listeners)
├─ components/http/connection/CMakeLists.txt               + add_subdirectory(tls_connection) + link ${DMP_HTTP}.Connection.Tls
├─ components/http/connection/tcp_connection/tcp_connection.hpp  + void cancel()
├─ tests/CMakeLists.txt                                    + add_subdirectory(integration_tests/http) under if(BUILD_HTTP)
└─ tests/unit_tests/http/CMakeLists.txt                    + Http.Listeners unit test target
```

Per-task verification: `cmake --build build/debug --target <target> -- -j4` clean;
`ctest --test-dir build/debug --output-on-failure -R <pattern>` passes.

---

## Task 1: Bootstrap config + listeners layer skeletons

**Files:**

- Create: `components/http/config/CMakeLists.txt`
- Create: `components/http/listeners/CMakeLists.txt`
- Modify: `components/http/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Goal:** Register both new layers as empty INTERFACE aggregates and wire the (still-empty) HTTP integration-test
subdirectory, so leaves and tests attach cleanly per task. No code/test yet (same precedent as PR 1/2/3 Task 1).

- [ ] **Step 1: Create the directory trees**

```bash
cd /home/grivin/Workspace/Demiplane
mkdir -p components/http/config/tls_config
mkdir -p components/http/listeners/{listener_base,connection_tracker,tcp_listener,tls_listener,quic_listener}
mkdir -p components/http/connection/tls_connection
mkdir -p tests/unit_tests/http/listeners tests/integration_tests/http
```

- [ ] **Step 2: Create `components/http/config/CMakeLists.txt`**

```cmake
##############################################################################
# Http Config — configuration value types. Per-leaf convention; the dotted
# ${DMP_HTTP}.Config target is an INTERFACE aggregate. PR 4 lands only the
# plain TlsConfig leaf (D1); PR 6 grows this with ServerConfig/ListenerConfig/
# Timeouts/load_server_config and rewrites tls_config into a ConfigInterface.
##############################################################################
add_subdirectory(tls_config)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Config INTERFACE)

target_link_libraries(${DMP_HTTP}.Config INTERFACE
        ${DMP_HTTP}.Config.TlsConfig
)
##############################################################################
```

- [ ] **Step 3: Create `components/http/listeners/CMakeLists.txt`** (leaves added by later tasks; start with just the
  aggregate so the layer registers)

```cmake
##############################################################################
# Http Listeners — transport layer: the ListenerBase polymorphic seam, the
# connection tracker, and concrete listeners (TCP, TLS+ALPN, QUIC scaffold).
# Per-leaf convention; the dotted ${DMP_HTTP}.Listeners target is an INTERFACE
# aggregate. Leaves are added by subsequent tasks.
##############################################################################

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Listeners INTERFACE)
##############################################################################
```

- [ ] **Step 4: Register both layers + the TLS connection.** In `components/http/CMakeLists.txt`, after the drivers
  block and *before* the `# HTTP Exported library` block, add:

```cmake
##############################################################################
# Http Config layer (PR 4 of redesign — plain TlsConfig; PR 6 grows it)
##############################################################################
add_subdirectory(config)
##############################################################################


##############################################################################
# Http Listeners layer (PR 4 of redesign)
##############################################################################
add_subdirectory(listeners)
##############################################################################
```

(The `connection/` subdirectory already exists and is added earlier; Task 9 appends the `tls_connection` leaf inside
`components/http/connection/CMakeLists.txt`. The umbrella `add_combined_library(${DMP_HTTP} … LIBRARIES)` stays
untouched — it is populated in PR 7.)

- [ ] **Step 5: Wire the HTTP integration-test subdirectory.** In `tests/CMakeLists.txt`, the HTTP block currently reads:

```cmake
if (BUILD_HTTP)
    message("Nexus tests will be built")
    add_subdirectory(unit_tests/http)
endif ()
```

Change it to:

```cmake
if (BUILD_HTTP)
    message("Http tests will be built")
    add_subdirectory(unit_tests/http)
    add_subdirectory(integration_tests/http)
endif ()
```

- [ ] **Step 6: Create a placeholder `tests/integration_tests/http/CMakeLists.txt`** (so the `add_subdirectory` above
  resolves; the first real test target lands in Task 7)

```cmake
##############################################################################
# Http integration tests (real 127.0.0.1:0 sockets via Boost.Beast clients).
# Test targets are added by later tasks.
##############################################################################
```

- [ ] **Step 7: Configure + sanity build**

```bash
cmake --preset debug 2>&1 | tail -5
cmake --build build/debug --target Demiplane.Tests.Unit.Http.Drivers -- -j4 2>&1 | tail -5
```

Expected: configure succeeds (the two empty INTERFACE aggregates validate at configure time); existing tests still build.

- [ ] **Step 8: Commit**

```bash
git add components/http/config/CMakeLists.txt components/http/listeners/CMakeLists.txt \
        components/http/CMakeLists.txt tests/CMakeLists.txt tests/integration_tests/http/CMakeLists.txt
git commit -m "feat(http): bootstrap config + listeners layer skeletons

Empty INTERFACE aggregates Demiplane.Component.HTTP.Config and .Listeners
registered; integration_tests/http wired under BUILD_HTTP. Leaves land per task."
```

---

## Task 2: `TlsConfig` — plain config struct (D1)

**Files:**

- Create: `components/http/config/tls_config/tls_config.hpp`
- Create: `components/http/config/tls_config/CMakeLists.txt`
- Modify: `components/http/config/CMakeLists.txt` (already references the leaf; no change if Task 1 wrote it — verify)

**Goal:** Spec §10.1's TLS settings as a **plain struct** carrying exactly what `build_ssl_context` consumes (D1). No
`ConfigInterface`, no test of its own (defaults are exercised by `build_ssl_context`'s unit test in Task 10 and the TLS
integration test in Task 12) — same precedent as PR 3's `Http11Config`.

- [ ] **Step 1: Create `components/http/config/tls_config/tls_config.hpp`**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace demiplane::http {

    /**
     * @brief TLS settings consumed by build_ssl_context (spec §7.4 / §10.1).
     *
     * PR 4 ships this as a PLAIN STRUCT (D1) — the serialization::ConfigInterface
     * version (with fields()/Builder/validate()) lands in PR 6, which rewrites
     * THIS file in place; build_ssl_context then switches `.cert_file` to
     * `.cert_file()`. Kept at the spec's final path so the include is stable.
     */
    struct TlsConfig {
        enum class MinVersion : std::uint8_t { tls12, tls13 };

        std::string cert_file;        // PEM cert chain (required)
        std::string key_file;         // PEM private key (required)
        std::string key_passphrase;   // optional; empty → no passphrase callback
        std::string dh_params_file;   // optional DH params
        std::string ca_file;          // optional; required if require_client_cert

        MinVersion min_version       = MinVersion::tls12;
        bool        session_cache    = true;
        bool        require_client_cert = false;
    };

}  // namespace demiplane::http
```

- [ ] **Step 2: Create `components/http/config/tls_config/CMakeLists.txt`**

```cmake
##############################################################################
# Http Config — TlsConfig (plain struct; PR 6 promotes to ConfigInterface, D1)
##############################################################################
add_library(${DMP_HTTP}.Config.TlsConfig INTERFACE tls_config.hpp)

target_include_directories(${DMP_HTTP}.Config.TlsConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
```

- [ ] **Step 3: Configure + sanity build** (no symbols yet; just confirms the leaf + aggregate resolve)

```bash
cmake --preset debug 2>&1 | tail -3
cmake --build build/debug --target Demiplane.Component.HTTP.Config -- -j4 2>&1 | tail -5
```

Expected: the INTERFACE aggregate "builds" (nothing to compile) without error.

- [ ] **Step 4: Commit**

```bash
git add components/http/config/tls_config components/http/config/CMakeLists.txt
git commit -m "feat(http/config): TlsConfig plain struct (D1)

Fields build_ssl_context consumes; PR 6 rewrites this file into the
ConfigInterface-backed version. Lives at the spec's final config/ path."
```

---

## Task 3: `ListenerBase` — the one polymorphic seam

**Files:**

- Create: `components/http/listeners/listener_base/listener_base.hpp`
- Create: `components/http/listeners/listener_base/CMakeLists.txt`
- Modify: `components/http/listeners/CMakeLists.txt`

**Goal:** Spec §7.1: the abstract interface the Server (PR 5) holds as `unique_ptr<ListenerBase>` — the only virtual
dispatch in the runtime path. Header-only INTERFACE leaf; concrete behavior + its tests land with `TcpListener` (Task 6)
and the integration tests (Tasks 7–8).

- [ ] **Step 1: Create `components/http/listeners/listener_base/listener_base.hpp`**

```cpp
#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include <boost/asio/awaitable.hpp>

#include <router.hpp>

namespace demiplane::http {

    /**
     * @brief Type-erased listener interface (spec §7.1).
     *
     * The ONLY virtual seam in the runtime path: the Server (PR 5) owns
     * std::vector<std::unique_ptr<ListenerBase>>. Concrete listeners
     * (TcpListener<Driver>, TlsListener<Drivers...>, QuicListener<Http3Driver>)
     * are templated on their driver(s); ListenerBase erases that so the Server
     * does not template on the protocol set.
     *
     * Lifecycle: bind() synchronously (throws on failure) → the caller
     * co_spawns run(router) bound to a cancellation slot → on shutdown the
     * caller emits terminal on that slot (stops accepting) and awaits
     * drain_until(deadline) (in-flight requests finish or are force-cancelled).
     */
    class ListenerBase {
    public:
        ListenerBase()                               = default;
        ListenerBase(const ListenerBase&)            = delete;
        ListenerBase& operator=(const ListenerBase&) = delete;
        ListenerBase(ListenerBase&&)                 = delete;
        ListenerBase& operator=(ListenerBase&&)      = delete;
        virtual ~ListenerBase()                      = default;

        /// Open + bind + listen. Synchronous; throws boost::system::system_error
        /// (or std::system_error for cert load) on failure — surfaced immediately.
        virtual void bind() = 0;

        /// Accept loop until the associated cancellation slot is emitted.
        virtual boost::asio::awaitable<void> run(Router& router) = 0;

        /// Wait for in-flight connections to finish, force-cancelling whatever
        /// remains at `deadline`. Delegates to the listener's ConnectionTracker.
        virtual boost::asio::awaitable<void> drain_until(
            std::chrono::steady_clock::time_point deadline) = 0;

        [[nodiscard]] virtual std::string bind_address() const = 0;
        [[nodiscard]] virtual std::uint16_t bound_port() const = 0;   // for tests on :0
    };

}  // namespace demiplane::http
```

- [ ] **Step 2: Create `components/http/listeners/listener_base/CMakeLists.txt`**

```cmake
##############################################################################
# Http Listeners — ListenerBase (abstract interface; header-only)
##############################################################################
add_library(${DMP_HTTP}.Listeners.Base INTERFACE listener_base.hpp)

target_include_directories(${DMP_HTTP}.Listeners.Base INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Listeners.Base INTERFACE
        ${DMP_HTTP}.Routing.Router
        Boost::asio
)
##############################################################################
```

- [ ] **Step 3: Wire into the aggregate** — in `components/http/listeners/CMakeLists.txt`, add `add_subdirectory(listener_base)`
  above the aggregate and link it:

```cmake
add_subdirectory(listener_base)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Listeners INTERFACE)

target_link_libraries(${DMP_HTTP}.Listeners INTERFACE
        ${DMP_HTTP}.Listeners.Base
)
##############################################################################
```

- [ ] **Step 4: Configure + sanity build**

```bash
cmake --preset debug 2>&1 | tail -3
cmake --build build/debug --target Demiplane.Component.HTTP.Listeners -- -j4 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add components/http/listeners/listener_base components/http/listeners/CMakeLists.txt
git commit -m "feat(http/listeners): ListenerBase abstract interface (spec §7.1)

The single virtual seam: bind() / run(Router&) / drain_until(deadline) /
bind_address() / bound_port(). Server (PR 5) holds unique_ptr<ListenerBase>."
```

---

## Task 4: `ConnectionTracker` — in-flight counting + graceful force-cancel (D2)

**Files:**

- Create: `components/http/listeners/connection_tracker/connection_tracker.hpp`
- Create: `components/http/listeners/connection_tracker/connection_tracker.cpp`
- Create: `components/http/listeners/connection_tracker/CMakeLists.txt`
- Create: `tests/unit_tests/http/listeners/test_connection_tracker.cpp`
- Modify: `components/http/listeners/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** Spec §7.2 reconciled per **D2** (spike S1): track in-flight connections with an atomic counter + a
`weak_ptr` + force-cancel thunk per connection; `register_connection` returns an RAII `Handle` that deregisters on
destruction; `drain_until` polls the counter and, on the deadline, dispatches `emit(terminal)` onto each surviving
connection's strand (the `weak_ptr` makes the late force-cancel use-after-free-safe). This is the cheap place to pin the
force-cancel timing — the integration tests (Task 8) then exercise it on a real listener.

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/listeners/test_connection_tracker.cpp`

```cpp
#include <atomic>
#include <chrono>
#include <memory>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>

#include <connection_tracker.hpp>

using namespace demiplane::http;
using namespace std::chrono_literals;

namespace {
    // Minimal connection model: owns a strand + a cancel flag. cancel() is what
    // ConnectionTracker requires structurally (D2) — not part of the Connection
    // concept.
    struct FakeConn {
        boost::asio::strand<boost::asio::io_context::executor_type> strand;
        std::atomic<bool> cancelled{false};
        explicit FakeConn(boost::asio::io_context& ioc) : strand{boost::asio::make_strand(ioc)} {}
        void cancel() noexcept { cancelled.store(true, std::memory_order_release); }
    };
}  // namespace

TEST(ConnectionTrackerTest, HandleRaiiCountsInFlight) {
    boost::asio::io_context ioc;
    ConnectionTracker tracker;
    EXPECT_EQ(tracker.in_flight(), 0u);
    {
        auto c1 = std::make_shared<FakeConn>(ioc);
        auto c2 = std::make_shared<FakeConn>(ioc);
        auto h1 = tracker.register_connection(c1, c1->strand);
        auto h2 = tracker.register_connection(c2, c2->strand);
        EXPECT_EQ(tracker.in_flight(), 2u);
    }  // both Handles destroyed → deregister
    EXPECT_EQ(tracker.in_flight(), 0u);
}

TEST(ConnectionTrackerTest, DrainReturnsImmediatelyWhenEmpty) {
    boost::asio::io_context ioc;
    ConnectionTracker tracker;
    auto fut = boost::asio::co_spawn(
        ioc,
        tracker.drain_until(ioc.get_executor(), std::chrono::steady_clock::now() + 10s),
        boost::asio::use_future);
    ioc.run();
    fut.get();  // does not block for 10s; rethrows on error
    SUCCEED();
}

TEST(ConnectionTrackerTest, ForceCancelsSurvivorsAtDeadline) {
    boost::asio::io_context ioc;
    ConnectionTracker tracker;

    auto c1 = std::make_shared<FakeConn>(ioc);
    auto c2 = std::make_shared<FakeConn>(ioc);
    auto h1 = std::make_shared<ConnectionTracker::Handle>(
        tracker.register_connection(c1, c1->strand));
    auto h2 = std::make_shared<ConnectionTracker::Handle>(
        tracker.register_connection(c2, c2->strand));

    // They never finish on their own → drain hits the deadline and force-cancels.
    auto fut = boost::asio::co_spawn(
        ioc,
        tracker.drain_until(ioc.get_executor(), std::chrono::steady_clock::now() + 100ms),
        boost::asio::use_future);
    ioc.run();
    fut.get();

    EXPECT_TRUE(c1->cancelled.load());
    EXPECT_TRUE(c2->cancelled.load());
}

TEST(ConnectionTrackerTest, DeadConnectionIsSkippedNotUseAfterFree) {
    boost::asio::io_context ioc;
    ConnectionTracker tracker;

    auto c1 = std::make_shared<FakeConn>(ioc);
    // Register but then drop the connection while the Handle still exists — the
    // weak_ptr in the tracker must lock() to null and skip (D2 / spike S1).
    auto h1 = std::make_shared<ConnectionTracker::Handle>(
        tracker.register_connection(c1, c1->strand));
    c1.reset();

    auto fut = boost::asio::co_spawn(
        ioc,
        tracker.drain_until(ioc.get_executor(), std::chrono::steady_clock::now() + 50ms),
        boost::asio::use_future);
    ioc.run();
    fut.get();  // no crash, no UAF
    SUCCEED();
}
```

- [ ] **Step 2: Wire the test target** — append to `tests/unit_tests/http/CMakeLists.txt`:

```cmake
##############################################################################
# Test HTTP Listeners layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Listeners
        listeners/test_connection_tracker.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Listeners
        PRIVATE
        Demiplane.Component.HTTP.Listeners
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Types
        Demiplane.Component.HTTP.Config
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
)
##############################################################################
```

(The source list grows in Tasks 10 + 13; check before appending. The link list is the full layer set this test target
will need by the end of the PR — wired up front so later tasks only add sources.)

- [ ] **Step 3: Configure + build — expect failure** (`connection_tracker.hpp` missing):

```bash
cmake --preset debug 2>&1 | tail -3
cmake --build build/debug --target Demiplane.Tests.Unit.Http.Listeners -- -j4 2>&1 | tail -10
```

- [ ] **Step 4: Create `components/http/listeners/connection_tracker/connection_tracker.hpp`**

```cpp
#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/dispatch.hpp>

namespace demiplane::http {

    /**
     * @brief Tracks in-flight connections for graceful shutdown (spec §7.2, D2).
     *
     * The landed connections own their own cancellation_signal (the driver binds
     * I/O to conn.cancel_slot()), so this tracker does NOT own signals. Instead
     * each entry holds a weak_ptr to the connection + a thunk that dispatches
     * `conn->cancel()` (emit terminal on the connection's own signal) onto the
     * connection's strand. drain_until() polls the counter and, at the deadline,
     * force-cancels every survivor; the weak_ptr makes a late force-cancel
     * use-after-free-safe (a connection whose serve() already finished is gone).
     *
     * register_connection() requires only that the concrete connection type has
     * a `void cancel()` method (TcpConnection/TlsConnection provide it) — cancel
     * is intentionally not on the Connection concept.
     */
    class ConnectionTracker {
    public:
        ConnectionTracker()                                    = default;
        ConnectionTracker(const ConnectionTracker&)            = delete;
        ConnectionTracker& operator=(const ConnectionTracker&) = delete;
        ConnectionTracker(ConnectionTracker&&)                 = delete;
        ConnectionTracker& operator=(ConnectionTracker&&)      = delete;

        struct Entry {
            std::weak_ptr<void> conn;
            std::function<void(const std::shared_ptr<void>&)> cancel;
        };

        /// RAII deregistration: on destruction, erase the entry + decrement the
        /// counter. Move-only (move nulls the source so the dtor is a no-op).
        class Handle {
        public:
            Handle(ConnectionTracker* tracker, std::list<Entry>::iterator it) noexcept
                : tracker_{tracker}, it_{it} {}
            Handle(Handle&& o) noexcept : tracker_{o.tracker_}, it_{o.it_} { o.tracker_ = nullptr; }
            Handle& operator=(Handle&& o) noexcept {
                if (this != &o) {
                    release();
                    tracker_  = o.tracker_;
                    it_       = o.it_;
                    o.tracker_ = nullptr;
                }
                return *this;
            }
            Handle(const Handle&)            = delete;
            Handle& operator=(const Handle&) = delete;
            ~Handle() { release(); }

        private:
            void release() noexcept;
            ConnectionTracker* tracker_;
            std::list<Entry>::iterator it_;
        };

        template <typename Conn>
        Handle register_connection(const std::shared_ptr<Conn>& conn,
                                   boost::asio::any_io_executor strand) {
            auto thunk = [strand = std::move(strand)](const std::shared_ptr<void>& c) {
                boost::asio::dispatch(strand, [c] {
                    std::static_pointer_cast<Conn>(c)->cancel();
                });
            };
            std::lock_guard lk{mu_};
            in_flight_.fetch_add(1, std::memory_order_acq_rel);
            auto it = entries_.insert(entries_.end(),
                                      Entry{std::weak_ptr<void>{conn}, std::move(thunk)});
            return Handle{this, it};
        }

        /// Poll the counter until it reaches 0 or `deadline` passes, then
        /// force-cancel every surviving connection. Runs on `ex`.
        boost::asio::awaitable<void> drain_until(
            boost::asio::any_io_executor ex,
            std::chrono::steady_clock::time_point deadline);

        [[nodiscard]] std::size_t in_flight() const noexcept {
            return in_flight_.load(std::memory_order_acquire);
        }

    private:
        std::atomic<std::size_t> in_flight_{0};
        std::mutex mu_;
        std::list<Entry> entries_;
    };

}  // namespace demiplane::http
```

> `#include <atomic>` is pulled in transitively by `<memory>`/`<mutex>` on libc++, but add it explicitly to the include
> list above (`#include <atomic>`) — the build uses `-Werror` and we rely on `std::atomic` directly.

- [ ] **Step 5: Create `components/http/listeners/connection_tracker/connection_tracker.cpp`**

```cpp
#include "connection_tracker.hpp"

#include <vector>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace demiplane::http {

    void ConnectionTracker::Handle::release() noexcept {
        if (tracker_ == nullptr) {
            return;
        }
        std::lock_guard lk{tracker_->mu_};
        tracker_->entries_.erase(it_);
        tracker_->in_flight_.fetch_sub(1, std::memory_order_acq_rel);
        tracker_ = nullptr;
    }

    boost::asio::awaitable<void> ConnectionTracker::drain_until(
        boost::asio::any_io_executor ex, std::chrono::steady_clock::time_point deadline) {
        using namespace std::chrono_literals;
        boost::asio::steady_timer timer{ex};

        while (in_flight_.load(std::memory_order_acquire) > 0
               && std::chrono::steady_clock::now() < deadline) {
            timer.expires_after(20ms);
            co_await timer.async_wait(boost::asio::use_awaitable);
        }

        // Snapshot the survivors under the lock; fire the cancel thunks after
        // releasing it (a thunk dispatches onto another strand, and a completing
        // connection's Handle dtor also takes the lock — do not hold it across).
        std::vector<Entry> survivors;
        {
            std::lock_guard lk{mu_};
            survivors.reserve(entries_.size());
            for (const auto& e : entries_) {
                survivors.push_back(e);
            }
        }
        for (const auto& e : survivors) {
            if (auto conn = e.conn.lock()) {   // skip connections that already finished
                e.cancel(conn);                // `conn` kept alive across the dispatch by capture
            }
        }
        co_return;
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Create `components/http/listeners/connection_tracker/CMakeLists.txt`**

```cmake
##############################################################################
# Http Listeners — ConnectionTracker (in-flight counting + force-cancel, D2)
##############################################################################
add_library(${DMP_HTTP}.Listeners.ConnectionTracker STATIC connection_tracker.cpp)

target_include_directories(${DMP_HTTP}.Listeners.ConnectionTracker PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Listeners.ConnectionTracker
        PUBLIC
        Boost::asio
)
##############################################################################
```

- [ ] **Step 7: Wire into the aggregate** — `add_subdirectory(connection_tracker)` + `${DMP_HTTP}.Listeners.ConnectionTracker`
  in the `components/http/listeners/CMakeLists.txt` aggregate link list.

- [ ] **Step 8: Build + run — expect pass**

```bash
cmake --preset debug 2>&1 | tail -3
cmake --build build/debug --target Demiplane.Tests.Unit.Http.Listeners -- -j4 2>&1 | tail -5
ctest --test-dir build/debug --output-on-failure -R Http.Listeners 2>&1 | tail -10
```

- [ ] **Step 9: Build + run under ASan if it configures** (the force-cancel UAF guard is the point — spike S1):

```bash
cmake --preset asan 2>&1 | tail -3 && \
  cmake --build build/asan --target Demiplane.Tests.Unit.Http.Listeners -- -j4 2>&1 | tail -3 && \
  ctest --test-dir build/asan --output-on-failure -R Http.Listeners 2>&1 | tail -5 || \
  echo "asan preset unavailable — debug coverage stands"
```

- [ ] **Step 10: Commit**

```bash
git add components/http/listeners/connection_tracker components/http/listeners/CMakeLists.txt \
        tests/unit_tests/http/listeners/test_connection_tracker.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/listeners): ConnectionTracker — in-flight count + force-cancel (D2)

weak_ptr + per-conn cancel thunk dispatched onto the connection's strand;
RAII Handle deregisters. Late force-cancel is UAF-safe via weak_ptr.lock()."
```

---

## Task 5: `TcpConnection::cancel()` + `TcpListener<Driver>`

**Files:**

- Modify: `components/http/connection/tcp_connection/tcp_connection.hpp` (add `cancel()`)
- Modify: `tests/unit_tests/http/connection/test_tcp_connection.cpp` (cancel smoke)
- Create: `components/http/listeners/tcp_listener/tcp_listener.hpp`
- Create: `components/http/listeners/tcp_listener/CMakeLists.txt`
- Create: `tests/unit_tests/http/listeners/test_tcp_listener.cpp` (construct + concept conformance; bind/run is Task 6)
- Modify: `components/http/listeners/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt` (add the new unit source)

**Goal:** Add the `cancel()` the tracker requires (D2) to the landed `TcpConnection`, then build the plain-TCP listener
(spec §7.3): accept **onto a per-connection strand** (D7), heap-allocate the connection, register it with the tracker,
`co_spawn` `driver.serve(conn, router)` on that strand. `bind()` opens the acceptor synchronously; `drain_until`
delegates to the tracker. The full accept→serve→drain path is wire-exercised in Tasks 6–8; this task lands the code and
the cheap construct/conformance unit checks.

- [ ] **Step 1: Add `cancel()` to `TcpConnection`.** In `components/http/connection/tcp_connection/tcp_connection.hpp`,
  add this method next to `cancel_slot()` (the `<boost/asio/cancellation_signal.hpp>` include is already present):

```cpp
        /// Force-cancel this connection's in-flight I/O (graceful shutdown). The
        /// ConnectionTracker dispatches this onto the connection's strand (D2),
        /// so emit() is serialized with the serve coroutine's I/O on that strand.
        void cancel() noexcept {
            signal_.emit(boost::asio::cancellation_type::terminal);
        }
```

- [ ] **Step 2: Append a smoke assertion** to `tests/unit_tests/http/connection/test_tcp_connection.cpp`:

```cpp
TEST(TcpConnectionTest, CancelOnFreshConnectionIsHarmless) {
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::socket sock{ioc};
    TcpConnection conn{std::move(sock)};
    conn.cancel();  // no slot connected yet → safe no-op, must not throw/crash
    SUCCEED();
}
```

- [ ] **Step 3: Build the Connection unit test — expect pass** (proves `cancel()` compiles + is safe):

```bash
cmake --build build/debug --target Demiplane.Tests.Unit.Http.Connection -- -j4 2>&1 | tail -5
ctest --test-dir build/debug --output-on-failure -R Http.Connection 2>&1 | tail -5
```

- [ ] **Step 4: Write the failing TcpListener unit test** — `tests/unit_tests/http/listeners/test_tcp_listener.cpp`

```cpp
#include <concepts>

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <listener_base.hpp>
#include <tcp_listener.hpp>

using namespace demiplane::http;

// TcpListener<Driver> is the polymorphic seam the Server holds (spec §7.1).
static_assert(std::derived_from<TcpListener<Http11Driver>, ListenerBase>);

TEST(TcpListenerTest, ConstructsAndReportsBindAddress) {
    boost::asio::io_context ioc;
    TcpListener<Http11Driver> listener{ioc.get_executor(), "127.0.0.1", 0,
                                       Http11Driver{Http11Config{}}};
    // No bind() here — opening a real socket / accepting is integration (Task 6).
    EXPECT_EQ(listener.bind_address(), "127.0.0.1");
}
```

- [ ] **Step 5: Register the test source** — add `listeners/test_tcp_listener.cpp` to the `Http.Listeners` source list
  in `tests/unit_tests/http/CMakeLists.txt`.

- [ ] **Step 6: Build — expect failure** (`tcp_listener.hpp` missing).

- [ ] **Step 7: Create `components/http/listeners/tcp_listener/tcp_listener.hpp`**

```cpp
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/error.hpp>

#include <connection_tracker.hpp>
#include <http_driver_concept.hpp>
#include <listener_base.hpp>
#include <router.hpp>
#include <tcp_connection.hpp>

namespace demiplane::http {

    /**
     * @brief Plain-TCP listener for one driver (spec §7.3).
     *
     * Accepts each connection ONTO A FRESH STRAND (D7 — beast::tcp_stream caches
     * its executor at construction, so the socket must already be strand-bound),
     * heap-allocates the (non-movable) TcpConnection as a shared_ptr, registers
     * it with the tracker, and co_spawns driver.serve() on that strand.
     *
     * LIFETIME CONTRACT: the caller MUST co_await drain_until(...) before
     * destroying the listener — spawned serve coroutines hold a tracker Handle
     * that references this listener's tracker, and call driver.serve() through
     * `this`.
     */
    template <HttpDriver Driver>
    class TcpListener final : public ListenerBase {
    public:
        TcpListener(boost::asio::any_io_executor exec, std::string host, std::uint16_t port,
                    Driver driver, std::size_t arena_size = 8192)
            : exec_{std::move(exec)},
              host_{std::move(host)},
              port_{port},
              driver_{std::move(driver)},
              arena_size_{arena_size},
              acceptor_{exec_} {}

        void bind() override {
            const boost::asio::ip::tcp::endpoint ep{boost::asio::ip::make_address(host_), port_};
            acceptor_.open(ep.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(ep);   // throws boost::system::system_error on failure
            acceptor_.listen(boost::asio::socket_base::max_listen_connections);
        }

        boost::asio::awaitable<void> run(Router& router) override {
            namespace asio = boost::asio;
            for (;;) {
                auto strand = asio::make_strand(exec_);
                boost::beast::error_code ec;
                asio::ip::tcp::socket sock = co_await acceptor_.async_accept(
                    strand, asio::redirect_error(asio::use_awaitable, ec));
                if (ec == asio::error::operation_aborted) {
                    break;          // stop(): the run-coroutine's cancellation slot was emitted
                }
                if (ec) {
                    continue;       // transient accept error — keep accepting
                }
                auto conn   = std::make_shared<TcpConnection>(std::move(sock), arena_size_);
                auto handle = tracker_.register_connection(conn, strand);
                asio::co_spawn(
                    strand,
                    [this, &router, conn, h = std::move(handle)]() -> asio::awaitable<void> {
                        co_await driver_.serve(*conn, router);
                    },
                    asio::detached);
            }
            // The loop only exits on shutdown (operation_aborted). Close the
            // acceptor so new SYNs are REFUSED (ECONNREFUSED), not silently
            // backlogged + left unserved while we drain (spec §14.2; spike S4).
            boost::beast::error_code ignore;
            acceptor_.close(ignore);
            co_return;
        }

        boost::asio::awaitable<void> drain_until(
            std::chrono::steady_clock::time_point deadline) override {
            co_await tracker_.drain_until(exec_, deadline);
        }

        [[nodiscard]] std::string bind_address() const override {
            return host_;
        }
        [[nodiscard]] std::uint16_t bound_port() const override {
            return acceptor_.local_endpoint().port();
        }

    private:
        boost::asio::any_io_executor exec_;
        std::string host_;
        std::uint16_t port_;
        Driver driver_;
        std::size_t arena_size_;
        boost::asio::ip::tcp::acceptor acceptor_;
        ConnectionTracker tracker_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 8: Create `components/http/listeners/tcp_listener/CMakeLists.txt`** (header-only template → INTERFACE)

```cmake
##############################################################################
# Http Listeners — TcpListener<Driver> (plain TCP; header-only template)
##############################################################################
add_library(${DMP_HTTP}.Listeners.Tcp INTERFACE tcp_listener.hpp)

target_include_directories(${DMP_HTTP}.Listeners.Tcp INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Listeners.Tcp INTERFACE
        ${DMP_HTTP}.Listeners.Base
        ${DMP_HTTP}.Listeners.ConnectionTracker
        ${DMP_HTTP}.Connection.Tcp
        ${DMP_HTTP}.Drivers.Concept
        ${DMP_HTTP}.Routing.Router
        Boost::beast
        Boost::asio
)
##############################################################################
```

- [ ] **Step 9: Wire into the aggregate** — `add_subdirectory(tcp_listener)` + `${DMP_HTTP}.Listeners.Tcp` in
  `components/http/listeners/CMakeLists.txt`.

- [ ] **Step 10: Build + run — expect pass**

```bash
cmake --preset debug 2>&1 | tail -3
cmake --build build/debug --target Demiplane.Tests.Unit.Http.Listeners -- -j4 2>&1 | tail -5
ctest --test-dir build/debug --output-on-failure -R Http.Listeners 2>&1 | tail -5
```

- [ ] **Step 11: Commit**

```bash
git add components/http/connection/tcp_connection/tcp_connection.hpp \
        tests/unit_tests/http/connection/test_tcp_connection.cpp \
        components/http/listeners/tcp_listener components/http/listeners/CMakeLists.txt \
        tests/unit_tests/http/listeners/test_tcp_listener.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http): TcpConnection::cancel() + TcpListener<Driver>

cancel() emits terminal on the connection's own signal (D2). TcpListener
accepts onto a per-connection strand (D7), tracks each connection, and
co_spawns driver.serve(). bind()/run()/drain wire-tested in PR4 integration."
```

---

## Task 6: Integration fixture + h1-over-TCP round-trip battery

**Files:**

- Create: `tests/integration_tests/http/http_test_fixture.hpp`
- Create: `tests/integration_tests/http/test_http_tcp.cpp`
- Modify: `tests/integration_tests/http/CMakeLists.txt`

**Goal:** The project's **first HTTP integration test** (spec §14.2): bind a `TcpListener<Http11Driver>` to
`127.0.0.1:0`, capture `bound_port()`, drive real requests through a Boost.Beast client, assert on the wire response,
then run the graceful shutdown sequence (emit stop → `drain_until` → stop the context → join). This validates the whole
`TcpListener` → `TcpConnection` → strand-spawned `serve` path that the in-memory `TestConnection` (PR 3) could not.

> **Fixture threading (D3).** The fixture owns the `io_context` + **one** worker thread + the stop signal — the
> responsibilities PR 5's `Server` will assume. One worker keeps `stop_signal_.emit()` serialized with the accept loop;
> the *multi-thread* force-cancel path is covered by the ConnectionTracker spike/unit test (Task 4) under ASan, and the
> N-worker/TSan concurrency matrix is a PR 5 acceptance item (the Server owns the thread topology).

- [ ] **Step 1: Create the fixture** — `tests/integration_tests/http/http_test_fixture.hpp`

```cpp
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <group.hpp>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <listener_base.hpp>
#include <route_registry.hpp>
#include <router.hpp>
#include <tcp_listener.hpp>

namespace http_it {

    namespace beast = boost::beast;
    namespace asio  = boost::asio;
    namespace bhttp = boost::beast::http;

    using ParsedResponse = bhttp::response<bhttp::string_body>;

    /// Owns an io_context + one worker thread + a route registry + a listener
    /// bound to 127.0.0.1:0. Subclasses register controllers in SetUp() then call
    /// start(make_listener()). TearDown() runs the §9.7 shutdown sequence.
    class HttpIntegrationFixture : public ::testing::Test {
    protected:
        asio::io_context ioc_;
        demiplane::http::RouteRegistry registry_;
        std::vector<std::shared_ptr<demiplane::http::HttpController>> controllers_;
        std::optional<demiplane::http::Router> router_;
        std::unique_ptr<demiplane::http::ListenerBase> listener_;
        asio::cancellation_signal stop_signal_;
        std::thread worker_;
        std::uint16_t port_{0};

        void add_controller(std::shared_ptr<demiplane::http::HttpController> ctrl) {
            demiplane::http::GroupBinding{registry_, controllers_, ""}.add_controller(std::move(ctrl));
        }

        /// Freeze routes, bind the listener, go live on the worker thread.
        void start(std::unique_ptr<demiplane::http::ListenerBase> listener) {
            ASSERT_TRUE(registry_.freeze().empty()) << "route conflicts in test setup";
            router_.emplace(registry_);
            listener_ = std::move(listener);
            listener_->bind();
            port_ = listener_->bound_port();
            ASSERT_GT(port_, 0);
            asio::co_spawn(ioc_, listener_->run(*router_),
                           asio::bind_cancellation_slot(stop_signal_.slot(), asio::detached));
            worker_ = std::thread{[this] { ioc_.run(); }};
        }

        /// Build the default plain-TCP / Http11 listener.
        std::unique_ptr<demiplane::http::ListenerBase> make_tcp_listener() {
            return std::make_unique<demiplane::http::TcpListener<demiplane::http::Http11Driver>>(
                ioc_.get_executor(), "127.0.0.1", 0,
                demiplane::http::Http11Driver{demiplane::http::Http11Config{}});
        }

        void TearDown() override {
            if (listener_) {
                auto fut = asio::co_spawn(
                    ioc_,
                    [this]() -> asio::awaitable<void> {
                        stop_signal_.emit(asio::cancellation_type::terminal);  // stop accepting
                        co_await listener_->drain_until(
                            std::chrono::steady_clock::now() + std::chrono::seconds{2});
                    },
                    asio::use_future);
                fut.get();  // blocks the (non-io) test thread until drain completes (§9.7)
            }
            ioc_.stop();
            if (worker_.joinable()) {
                worker_.join();
            }
        }
    };

    /// Synchronous Beast client over one TCP socket — reusable for keep-alive.
    class TcpClient {
    public:
        explicit TcpClient(std::uint16_t port) : socket_{ioc_} {
            socket_.connect({asio::ip::make_address("127.0.0.1"), port});
        }

        ParsedResponse send(bhttp::verb verb, std::string target, std::string body = {},
                            std::string_view content_type = "text/plain", bool keep_alive = false) {
            bhttp::request<bhttp::string_body> req{verb, target, 11};
            req.set(bhttp::field::host, "127.0.0.1");
            req.keep_alive(keep_alive);
            if (!body.empty()) {
                req.set(bhttp::field::content_type, std::string{content_type});
                req.body() = std::move(body);
            }
            req.prepare_payload();
            bhttp::write(socket_, req);

            ParsedResponse res;
            bhttp::read(socket_, buffer_, res);
            return res;
        }

    private:
        asio::io_context ioc_;
        asio::ip::tcp::socket socket_;
        beast::flat_buffer buffer_;
    };

}  // namespace http_it
```

- [ ] **Step 2: Create the test** — `tests/integration_tests/http/test_http_tcp.cpp`

```cpp
#include <memory>
#include <string>

#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <request_context.hpp>

#include "http_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;

namespace {

    class EchoController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &EchoController::hello);
            Get("/users/{id}", &EchoController::user);
            Post("/echo", &EchoController::echo);
            Get("/boom", &EchoController::boom);
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
            if (!body) {
                co_return ctx.status(HttpStatus::payload_too_large, "too big");
            }
            co_return ctx.json(std::move(body).value());
        }
        AsyncResponse boom(RequestContext) {
            throw std::runtime_error{"handler exploded"};
        }
    };

    class HttpTcpTest : public http_it::HttpIntegrationFixture {
    protected:
        void SetUp() override {
            add_controller(std::make_shared<EchoController>());
            start(make_tcp_listener());
        }
    };

}  // namespace

TEST_F(HttpTcpTest, GetRoundTrip) {
    http_it::TcpClient client{port_};
    auto res = client.send(bhttp::verb::get, "/hello");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "hello world");
    EXPECT_EQ(std::string(res[bhttp::field::server]), "Demiplane");
}

TEST_F(HttpTcpTest, PathAndQueryParams) {
    http_it::TcpClient client{port_};
    auto res = client.send(bhttp::verb::get, "/users/42?v=hi");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "user:42 v=hi");
}

TEST_F(HttpTcpTest, PostJsonEchoedThroughArena) {
    http_it::TcpClient client{port_};
    const std::string payload = R"({"name":"demiplane"})";
    auto res = client.send(bhttp::verb::post, "/echo", payload, "application/json");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), payload);
    EXPECT_EQ(std::string(res[bhttp::field::content_type]), "application/json");
}

TEST_F(HttpTcpTest, UnknownPathIs404) {
    http_it::TcpClient client{port_};
    EXPECT_EQ(client.send(bhttp::verb::get, "/nope").result_int(), 404u);
}

TEST_F(HttpTcpTest, WrongVerbIs405WithAllow) {
    http_it::TcpClient client{port_};
    auto res = client.send(bhttp::verb::delete_, "/hello");
    EXPECT_EQ(res.result_int(), 405u);
    EXPECT_NE(std::string(res["Allow"]).find("GET"), std::string::npos);
}

TEST_F(HttpTcpTest, HandlerExceptionBecomes500) {
    http_it::TcpClient client{port_};
    // A 500 RESPONSE, not a dropped connection (the original module's bug).
    EXPECT_EQ(client.send(bhttp::verb::get, "/boom").result_int(), 500u);
}

TEST_F(HttpTcpTest, KeepAliveServesTwoRequestsOnOneSocket) {
    http_it::TcpClient client{port_};
    auto first = client.send(bhttp::verb::get, "/hello", {}, "text/plain", /*keep_alive=*/true);
    EXPECT_EQ(first.body(), "hello world");
    EXPECT_TRUE(first.keep_alive());
    auto second = client.send(bhttp::verb::get, "/users/7?v=q", {}, "text/plain", /*keep_alive=*/false);
    EXPECT_EQ(second.body(), "user:7 v=q");
}
```

- [ ] **Step 3: Wire the integration test target** — replace the placeholder
  `tests/integration_tests/http/CMakeLists.txt` with:

```cmake
##############################################################################
# Http integration tests — real 127.0.0.1:0 sockets via Boost.Beast clients.
##############################################################################
add_integration_test(${INTEGRATION_TESTING_TARGET}.Http.Tcp
        test_http_tcp.cpp
        LINK_LIBS
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
target_include_directories(${INTEGRATION_TESTING_TARGET}.Http.Tcp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
```

> `add_integration_test` takes sources positionally and libraries via `LINK_LIBS` (see the pgsql integration tests). The
> fixture header is found via the explicit `target_include_directories` on `${CMAKE_CURRENT_SOURCE_DIR}`. `OpenSSL::*`
> is on the link line now so the TLS test (Task 12) only adds a source + a second target with no relinking surprises.

- [ ] **Step 4: Configure + build + run — expect pass**

```bash
cmake --preset debug 2>&1 | tail -3
cmake --build build/debug --target Demiplane.Tests.Integration.Http.Tcp -- -j4 2>&1 | tail -10
ctest --test-dir build/debug --output-on-failure -R Http.Tcp 2>&1 | tail -15
```

Expected: all seven cases pass; the test process exits cleanly (graceful shutdown joins the worker — no hang).

- [ ] **Step 5: Commit**

```bash
git add tests/integration_tests/http/http_test_fixture.hpp \
        tests/integration_tests/http/test_http_tcp.cpp tests/integration_tests/http/CMakeLists.txt
git commit -m "test(http/integration): first end-to-end h1-over-TCP battery

Real 127.0.0.1:0 socket via a Beast client: verbs, path/query params, JSON
echo, 404/405, handler-exception→500, keep-alive. Fixture owns ioc + worker +
graceful shutdown (emit stop → drain_until → stop ctx → join), per §9.7."
```

---

## Task 7: Integration — graceful drain + force-cancel on a real listener

**Files:**

- Modify: `tests/integration_tests/http/http_test_fixture.hpp` (extract a reusable `graceful_shutdown`; add a client read-after-close probe)
- Create: `tests/integration_tests/http/test_http_drain.cpp`
- Modify: `tests/integration_tests/http/CMakeLists.txt` (add the source)

**Goal:** Spec §14.2 lifecycle/graceful-shutdown coverage on the real `TcpListener` (the wiring PR 5's
`graceful_shutdown` depends on): an **in-flight request completes** while drain waits, and an **idle keep-alive
connection is force-cancelled** at the drain deadline (the tracker's force-cancel reaching a real socket — the
end-to-end version of the Task 4 unit test).

- [ ] **Step 1: Refactor the fixture's shutdown into a callable method.** In
  `tests/integration_tests/http/http_test_fixture.hpp`, add a guard flag + a `graceful_shutdown` method and make
  `TearDown` delegate to it. Replace the existing `TearDown()` body with:

```cpp
        /// Emit the stop signal (stops accepting) and drain in-flight connections
        /// up to `drain`. Idempotent — safe to call from a test then again in
        /// TearDown. Mirrors Server::graceful_shutdown's drain phase (PR 5).
        void graceful_shutdown(std::chrono::milliseconds drain = std::chrono::seconds{2}) {
            if (shut_down_ || !listener_) {
                return;
            }
            shut_down_ = true;
            auto fut = asio::co_spawn(
                ioc_,
                [this, drain]() -> asio::awaitable<void> {
                    stop_signal_.emit(asio::cancellation_type::terminal);
                    co_await listener_->drain_until(std::chrono::steady_clock::now() + drain);
                },
                asio::use_future);
            fut.get();  // blocks the (non-io) test thread until drain completes (§9.7)
        }

        void TearDown() override {
            graceful_shutdown();
            ioc_.stop();
            if (worker_.joinable()) {
                worker_.join();
            }
        }

    private:
        bool shut_down_ = false;

    protected:
```

> Place the `private: bool shut_down_ = false; protected:` toggle carefully — the rest of the fixture's members stay
> `protected`. (Simplest: declare `bool shut_down_ = false;` alongside the other protected members and drop the extra
> access labels — it need not be private.)

- [ ] **Step 2: Add a read-after-close probe to `TcpClient`.** In the same header, add this method to `TcpClient`:

```cpp
        /// Attempt to read a response; returns the resulting error_code. After the
        /// server force-cancels + half-closes, this returns a non-empty ec
        /// (end_of_stream / connection_reset).
        beast::error_code read_after_close() {
            ParsedResponse res;
            beast::error_code ec;
            bhttp::read(socket_, buffer_, res, ec);
            return ec;
        }
```

- [ ] **Step 3: Create the test** — `tests/integration_tests/http/test_http_drain.cpp`

```cpp
#include <chrono>
#include <memory>
#include <thread>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <request_context.hpp>

#include "http_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;
using namespace std::chrono_literals;

namespace {

    class DrainController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &DrainController::hello);
            Get("/slow", &DrainController::slow);
        }

    private:
        AsyncResponse hello(RequestContext ctx) {
            co_return ctx.ok("hello world");
        }
        AsyncResponse slow(RequestContext ctx) {
            auto ex = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer t{ex};
            t.expires_after(150ms);
            co_await t.async_wait(boost::asio::use_awaitable);
            co_return ctx.ok("slow done");
        }
    };

    class HttpDrainTest : public http_it::HttpIntegrationFixture {
    protected:
        void SetUp() override {
            add_controller(std::make_shared<DrainController>());
            start(make_tcp_listener());
        }
    };

}  // namespace

// An in-flight request finishes while drain waits (drain timeout is generous).
TEST_F(HttpDrainTest, InFlightRequestCompletesDuringDrain) {
    int status = 0;
    std::string body;
    std::thread caller{[&] {
        http_it::TcpClient client{port_};
        auto res = client.send(bhttp::verb::get, "/slow");
        status = static_cast<int>(res.result_int());
        body   = res.body();
    }};

    std::this_thread::sleep_for(50ms);   // let the request reach the slow handler
    graceful_shutdown(2s);               // drain must wait for the in-flight /slow
    caller.join();

    EXPECT_EQ(status, 200);
    EXPECT_EQ(body, "slow done");
}

// An idle keep-alive connection (driver blocked reading the next request) is
// force-cancelled at the drain deadline; the server half-closes our socket.
TEST_F(HttpDrainTest, IdleKeepAliveConnectionForceCancelledAtDeadline) {
    http_it::TcpClient client{port_};
    auto res = client.send(bhttp::verb::get, "/hello", {}, "text/plain", /*keep_alive=*/true);
    ASSERT_EQ(res.result_int(), 200u);
    ASSERT_TRUE(res.keep_alive());
    // Connection now idle: the driver is awaiting the next request header.

    graceful_shutdown(100ms);            // short drain → force-cancel the idle conn

    const auto ec = client.read_after_close();
    EXPECT_TRUE(ec) << "server should have closed the idle connection on force-cancel";
}

// After shutdown the accept loop has been cancelled AND the acceptor closed, so a
// fresh connection is REFUSED (not silently backlogged). This is the only test
// that asserts the accept-loop's co_spawn-slot cancellation actually fired —
// without it the fixture's ioc_.stop() would mask a propagation failure (spike S4).
TEST_F(HttpDrainTest, NewConnectionsRefusedAfterShutdown) {
    graceful_shutdown(100ms);

    boost::asio::io_context cioc;
    boost::asio::ip::tcp::socket sock{cioc};
    boost::system::error_code ec;
    sock.connect({boost::asio::ip::make_address("127.0.0.1"), port_}, ec);
    EXPECT_TRUE(ec) << "the closed acceptor must refuse new connections after shutdown";
}
```

- [ ] **Step 4: Add the source to the integration target** — in `tests/integration_tests/http/CMakeLists.txt`, append
  `test_http_drain.cpp` to the `${INTEGRATION_TESTING_TARGET}.Http.Tcp` source list (it shares the fixture + link set):

```cmake
add_integration_test(${INTEGRATION_TESTING_TARGET}.Http.Tcp
        test_http_tcp.cpp
        test_http_drain.cpp
        LINK_LIBS
        ...
```

- [ ] **Step 5: Build + run — expect pass**

```bash
cmake --build build/debug --target Demiplane.Tests.Integration.Http.Tcp -- -j4 2>&1 | tail -10
ctest --test-dir build/debug --output-on-failure -R Http.Tcp 2>&1 | tail -15
```

- [ ] **Step 6: Commit**

```bash
git add tests/integration_tests/http/http_test_fixture.hpp \
        tests/integration_tests/http/test_http_drain.cpp tests/integration_tests/http/CMakeLists.txt
git commit -m "test(http/integration): graceful drain completes in-flight; force-cancels idle conn

In-flight /slow finishes while drain waits; an idle keep-alive connection is
force-cancelled at the drain deadline and the server half-closes the socket."
```

---

## Task 8: `TlsConnection` — TLS stream + ALPN result (PR-3 D3 lands here)

**Files:**

- Create: `components/http/connection/tls_connection/tls_connection.hpp`
- Create: `components/http/connection/tls_connection/tls_connection.cpp`
- Create: `components/http/connection/tls_connection/CMakeLists.txt`
- Create: `tests/unit_tests/http/connection/test_tls_connection.cpp`
- Modify: `components/http/connection/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt` (add the new Connection unit source + OpenSSL links)

**Goal:** Spec §6.1's `TlsConnection`, deferred from PR 3 (D3): `ssl::stream<beast::tcp_stream>` + arena + cancel signal
+ the **negotiated protocol** (set after the handshake reads ALPN). Satisfies `StreamConnection`, so `Http11Driver::serve`
drives it unchanged (spike S2). `handshake()` and `async_close()` are TLS-specific (not in the concept) — the
`TlsListener` (Task 10) calls `handshake()` then dispatches by `negotiated_protocol()`. Verified here by concept
conformance + a construction smoke; the live handshake is integration (Task 11).

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/connection/test_tls_connection.cpp`

```cpp
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <gtest/gtest.h>

#include <connection_concepts.hpp>
#include <tls_connection.hpp>

using namespace demiplane::http;

static_assert(StreamConnection<TlsConnection>);

TEST(TlsConnectionTest, MetadataDefaults) {
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx{boost::asio::ssl::context::tls_server};
    boost::asio::ip::tcp::socket sock{ioc};
    TlsConnection conn{std::move(sock), ctx};
    EXPECT_TRUE(conn.is_secure());
    // Before the handshake, the negotiated protocol defaults to http1.
    EXPECT_EQ(conn.negotiated_protocol(), Protocol::http1);
    EXPECT_NE(conn.arena_alloc().resource(), nullptr);
    conn.reset_request_arena();  // does not throw on a fresh arena
    conn.cancel();               // no slot connected yet → safe no-op
}
```

- [ ] **Step 2: Register the test source + OpenSSL links** — add `connection/test_tls_connection.cpp` to the
  `Http.Connection` source list, and add `OpenSSL::SSL` + `OpenSSL::Crypto` to that target's link libraries:

```cmake
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Connection
        PRIVATE
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Types
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
)
```

- [ ] **Step 3: Build — expect failure** (`tls_connection.hpp` missing).

- [ ] **Step 4: Create `components/http/connection/tls_connection/tls_connection.hpp`**

```cpp
#pragma once

#include <chrono>
#include <cstddef>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/error.hpp>

#include <http_enums.hpp>
#include <request_arena.hpp>

namespace demiplane::http {

    /**
     * @brief TLS connection: ssl::stream<beast::tcp_stream> + per-connection
     *        arena + cancel signal + the ALPN-negotiated protocol (spec §6.1).
     *
     * Satisfies StreamConnection — Http11Driver::serve drives it unchanged. The
     * TlsListener (PR 4 Task 10) constructs it on a per-connection strand, calls
     * handshake() (which records the negotiated protocol from ALPN), then
     * dispatches to the driver whose id() matches negotiated_protocol().
     *
     * Non-movable (composes the immovable RequestArena + cancellation_signal).
     */
    class TlsConnection {
    public:
        using stream_type = boost::asio::ssl::stream<boost::beast::tcp_stream>;

        TlsConnection(boost::asio::ip::tcp::socket socket, boost::asio::ssl::context& ctx,
                      std::size_t arena_size = 8192)
            : stream_{std::move(socket), ctx}, arena_{arena_size} {}

        TlsConnection(const TlsConnection&)            = delete;
        TlsConnection& operator=(const TlsConnection&) = delete;
        TlsConnection(TlsConnection&&)                 = delete;
        TlsConnection& operator=(TlsConnection&&)      = delete;

        /// TLS handshake (server role). Records the ALPN-negotiated protocol.
        /// Returns the handshake error_code (empty on success). NOT in the
        /// Connection concept — the listener calls it before serve().
        boost::asio::awaitable<boost::beast::error_code> handshake(
            std::chrono::milliseconds timeout = std::chrono::seconds{10});

        [[nodiscard]] stream_type& stream() noexcept {
            return stream_;
        }
        [[nodiscard]] std::pmr::polymorphic_allocator<> arena_alloc() noexcept {
            return arena_.allocator();
        }
        void reset_request_arena() {
            arena_.reset();
        }
        void expires_after(std::chrono::milliseconds ms) {
            boost::beast::get_lowest_layer(stream_).expires_after(ms);
        }
        boost::asio::awaitable<void> async_close();
        [[nodiscard]] boost::asio::cancellation_slot cancel_slot() noexcept {
            return signal_.slot();
        }
        void cancel() noexcept {
            signal_.emit(boost::asio::cancellation_type::terminal);
        }
        [[nodiscard]] boost::asio::ip::address remote_address() const {
            return boost::beast::get_lowest_layer(stream_).socket().remote_endpoint().address();
        }
        [[nodiscard]] Protocol negotiated_protocol() const noexcept {
            return negotiated_protocol_;
        }
        [[nodiscard]] static bool is_secure() noexcept {
            return true;
        }

    private:
        stream_type stream_;
        RequestArena arena_;
        boost::asio::cancellation_signal signal_;
        Protocol negotiated_protocol_ = Protocol::http1;
    };

}  // namespace demiplane::http
```

> `get_lowest_layer(ssl::stream<tcp_stream>)` returns the `tcp_stream`, which provides `expires_after` and `.socket()`.
> `remote_address()` is `const` and uses `get_lowest_layer` on a `const` stream — Beast provides a const overload.

- [ ] **Step 5: Create `components/http/connection/tls_connection/tls_connection.cpp`**

```cpp
#include "tls_connection.hpp"

#include <string_view>

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <openssl/ssl.h>

namespace demiplane::http {

    namespace {
        Protocol protocol_from_alpn(std::string_view alpn) noexcept {
            if (alpn == "h2") {
                return Protocol::http2;
            }
            if (alpn == "h3") {
                return Protocol::http3;
            }
            return Protocol::http1;  // "http/1.1" or (defensively) none selected
        }
    }  // namespace

    boost::asio::awaitable<boost::beast::error_code> TlsConnection::handshake(
        std::chrono::milliseconds timeout) {
        boost::beast::get_lowest_layer(stream_).expires_after(timeout);

        boost::beast::error_code ec;
        co_await stream_.async_handshake(
            boost::asio::ssl::stream_base::server,
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) {
            co_return ec;
        }

        const unsigned char* proto = nullptr;
        unsigned int len           = 0;
        ::SSL_get0_alpn_selected(stream_.native_handle(), &proto, &len);
        negotiated_protocol_ = protocol_from_alpn(
            std::string_view{reinterpret_cast<const char*>(proto), len});
        co_return ec;  // empty (success)
    }

    boost::asio::awaitable<void> TlsConnection::async_close() {
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds{5});
        boost::beast::error_code ec;
        // Best-effort TLS close-notify; peer may already be gone.
        co_await stream_.async_shutdown(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        boost::beast::get_lowest_layer(stream_).socket().shutdown(
            boost::asio::ip::tcp::socket::shutdown_send, ec);
        co_return;
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Create `components/http/connection/tls_connection/CMakeLists.txt`**

```cmake
##############################################################################
# Http Connection — TlsConnection (ssl::stream<tcp_stream> + arena + ALPN)
##############################################################################
add_library(${DMP_HTTP}.Connection.Tls STATIC tls_connection.cpp)

target_include_directories(${DMP_HTTP}.Connection.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Connection.Tls
        PUBLIC
        ${DMP_HTTP}.Connection.RequestArena
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Types.Enums
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
```

- [ ] **Step 7: Wire into the connection aggregate** — in `components/http/connection/CMakeLists.txt`, add
  `add_subdirectory(tls_connection)` and `${DMP_HTTP}.Connection.Tls` to the `${DMP_HTTP}.Connection` INTERFACE link
  list (alongside `.RequestArena`, `.Concepts`, `.Tcp`, `.Quic`).

- [ ] **Step 8: Build + run — expect pass**

```bash
cmake --preset debug 2>&1 | tail -3
cmake --build build/debug --target Demiplane.Tests.Unit.Http.Connection -- -j4 2>&1 | tail -5
ctest --test-dir build/debug --output-on-failure -R Http.Connection 2>&1 | tail -5
```

- [ ] **Step 9: Commit**

```bash
git add components/http/connection/tls_connection components/http/connection/CMakeLists.txt \
        tests/unit_tests/http/connection/test_tls_connection.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/connection): TlsConnection — ssl::stream + arena + ALPN (PR3 D3)

Satisfies StreamConnection; handshake() records the ALPN-negotiated protocol.
The TlsListener handshakes then dispatches by negotiated_protocol()."
```

---

## Task 9: `build_ssl_context` + the shared test cert (spec §7.4)

**Files:**

- Create: `tests/integration_tests/http/test_tls_cert.hpp` (embedded self-signed cert + temp-file helper — single source of truth, used by Tasks 9 + 11)
- Create: `components/http/listeners/tls_listener/build_ssl_context.hpp`
- Create: `components/http/listeners/tls_listener/build_ssl_context.cpp`
- Create: `components/http/listeners/tls_listener/CMakeLists.txt`
- Create: `tests/unit_tests/http/listeners/test_build_ssl_context.cpp`
- Modify: `components/http/listeners/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt` (add source + include the shared-cert dir)

**Goal:** Spec §7.4: a hardened server `ssl::context` from a `TlsConfig` — protocol-version floor, disabled
legacy protocols, cert/key (+ optional passphrase/DH/client-cert), session cache, and the ALPN select callback whose
`arg` is the caller's advertised-wire buffer (D4, spike S2: `SSL_TLSEXT_ERR_ALERT_FATAL` on mismatch). Unit-tested with
the embedded cert written to a temp file (D5) and a bad-path throw.

- [ ] **Step 1: Create the shared test cert** — `tests/integration_tests/http/test_tls_cert.hpp` (self-signed,
  CN=localhost, SAN `IP:127.0.0.1`/`DNS:localhost`, valid until 2126; throwaway key for tests only)

```cpp
#pragma once

#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>

#include <unistd.h>  // getpid — per-process-unique temp paths (parallel ctest safe)

// NOT A SECRET: a throwaway self-signed cert + key generated solely for these
// localhost integration tests (CN=localhost, SAN IP:127.0.0.1, valid to 2126).
// It guards no real asset — present so the TLS tests are hermetic. If a CI secret
// scanner flags this file, allow-list it.
namespace http_tls_test {

    inline constexpr std::string_view kTestCertPem = R"PEM(
-----BEGIN CERTIFICATE-----
MIIDJzCCAg+gAwIBAgIUanuJILFxik0+52ml2z09lgvcxt8wDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MCAXDTI2MDYxNTE3NTAxNVoYDzIxMjYw
NTIyMTc1MDE1WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwggEiMA0GCSqGSIb3DQEB
AQUAA4IBDwAwggEKAoIBAQDASeExUpCEClO0XKr8FOZ9uT4s0P/KOp6g8T4WmL8M
DAB5y32B7r67bDP1Vvs/1Ryh0PVjmke8575a7pKclrbxOzoGz7hI057PhhMzexwn
KvhN5zm63ddSq+whkcOIfLoklYLKhGdAht8eLJI17J64o+KmYybT2Ln2YsZLj9bU
3+3YS5M1sgjohOJT2mb87w5C95jsxPgurMwLWxybujgRxTnl9hhhR0rWJr6ZuTYq
zPxDT5n8MR48P6BiY4j5ncIUSSR+jGzvDLztWBwKyKLOL4rAgyLowciR930t79L1
nN992WUWrhhW39a0Xmy8P0xGaHt3qvaEvqg8AY7tXqXfAgMBAAGjbzBtMB0GA1Ud
DgQWBBSXP9hvZ3ZWKO9N9jk19ImQ6VhvMjAfBgNVHSMEGDAWgBSXP9hvZ3ZWKO9N
9jk19ImQ6VhvMjAPBgNVHRMBAf8EBTADAQH/MBoGA1UdEQQTMBGHBH8AAAGCCWxv
Y2FsaG9zdDANBgkqhkiG9w0BAQsFAAOCAQEAMT2RYqCJtA2WLgsTM0LgJPo1i1Pn
B1P1V5hThhvkgaeRJHCB7Hnmv2MzdQUvLJTx/dDqvZkZQ1ZkIg7bVt9yzD+sN/9P
NLsJ5nHJl6LbmowztKwS2JPs3pdMYMjzPddq0rrOaZoqPzWNdAPjZrTwDsSOu4Ph
Db7eEXEjGyVcKlB0IV5fy5Oq5+qBezmiE1zjmidn+U3NFqacGauyGTkVfBasxwOe
9mFkshyoJ++T7crREDl6psvSZHpt2thk5EVC2nQfGWnrZUwxRrumFllmL6HgiIeR
ck99JTvJ+HqPD5IrcEV/UI8kkexGJWQAo1R+TXYVKsmgIHGwKYm1tLD0MA==
-----END CERTIFICATE-----
)PEM";

    inline constexpr std::string_view kTestKeyPem = R"PEM(
-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQDASeExUpCEClO0
XKr8FOZ9uT4s0P/KOp6g8T4WmL8MDAB5y32B7r67bDP1Vvs/1Ryh0PVjmke8575a
7pKclrbxOzoGz7hI057PhhMzexwnKvhN5zm63ddSq+whkcOIfLoklYLKhGdAht8e
LJI17J64o+KmYybT2Ln2YsZLj9bU3+3YS5M1sgjohOJT2mb87w5C95jsxPgurMwL
WxybujgRxTnl9hhhR0rWJr6ZuTYqzPxDT5n8MR48P6BiY4j5ncIUSSR+jGzvDLzt
WBwKyKLOL4rAgyLowciR930t79L1nN992WUWrhhW39a0Xmy8P0xGaHt3qvaEvqg8
AY7tXqXfAgMBAAECggEAONdeD0N13uJingVqsfvHqtCQlZTumCw96huGHA3pI7mE
hnxlzHvzu9mffl3JBbSMszTe5SOdIzVqKt0tT8apq6OzYoIS2sxbvMLIeEZjKxzj
q7u3cArV9OVHdyDsqTMdn2Tm9dCv6P41hGjui6w3uyMPA9p5htQhHLlUHtAVVHWd
rhxcMr6BAyUxERVL8EXHxiGOwHqanwKWRzcngmqXLv8QOGWt/rtFvmg1MoP/FOSI
nraQOnoFWLmj//dhZKD/Dw2eaHkjtqPMqaXS93CKIHZmoQfaTKgLIxi7erIJrZfx
DUqj5jrbfr7cNsuVfpouupFi2OG9rEhuBHTqzJyASQKBgQD17hJRFgYQoxa6jbfr
VOnrpMUUlWIy05WK+2/CjfbRE7xRPmyBYmUX4xMdTXSPva6RaseBlO4gz4pRXZDQ
GLtCzQAC0bap8uP2m/uWw6l5NHwhtxKzXTdwifViBgyfZ2oZyxihbjMi3K6UUImn
Gaha2zOg0ybuWhmrWU8y53G7tQKBgQDIKYT8NKWQaWWW9+h1ek1qqc7sLbdu8+yf
AUkSLH+UKgWW01t3g4m/6Qw1eSDHWC3l0IV3adXNg1gtmutBnpwsy1pzCL8H2PMe
6d/BsKGG6APKnlgrguTMWsi8PstRejkvlMK7Dq7DMGY1ncdDYS7qfA2aKqwkoTpb
oiH2upLfwwKBgEf/BFm8qtXgCN1gc8FvQHP97rxR50ed7Z+ccGFykhkvP+hA8B8I
oTPXBFeFv2P9Uce8jN+ArB3q5EFhtO1W8CtkPGaW4nTqaJZfn83JRin3lYeBQvZD
ieFmYfHqd3OLIOKgNHu9+TZxiKJe2Y2T01eV6I1ig3kv42foY2kxnHgpAoGAX4za
a97h7jcyBMhhUrtIe5OGMN5+A1wz54+ghylw2ZTZyC8rKblEJ7WjW19wU1j3yA4r
uF5wbsO1c0fR6ChEG2oTyngxYRiirm4sn3SnFxRowu+l3VeFyzvHOX2sZz+2Ts1v
zAXtTUYsdInWFocs80i24ZJfTLked6HFHtffxysCgYBea5HspclFpe2E9C/GUo8V
y/B99AQhfv0FuxWhhkcilwdtDNmvYMhWKowxE38ifITJzbjvggQTzLOOgM8cprZF
4EnmgaePmDrECghmrZQ8Rqm4q8/hDEndsGG5/ulVkQ8tgcMM+jvmCgkksh7n7rwy
Qt2yOuRaaXafFa8Zs9HObA==
-----END PRIVATE KEY-----
)PEM";

    /// Write `contents` to a per-process-unique temp file; return its path.
    inline std::string write_temp(std::string_view stem, std::string_view contents) {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path()
            / ("dmp_http_" + std::to_string(::getpid()) + "_" + std::string{stem});
        std::ofstream out{path, std::ios::binary | std::ios::trunc};
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return path.string();
    }

}  // namespace http_tls_test
```

> The leading newline inside each `R"PEM(...)` is harmless — OpenSSL's PEM reader skips to the `-----BEGIN` marker.

- [ ] **Step 2: Write the failing test** — `tests/unit_tests/http/listeners/test_build_ssl_context.cpp`

```cpp
#include <string>

#include <boost/asio/ssl/context.hpp>
#include <gtest/gtest.h>

#include <build_ssl_context.hpp>
#include <tls_config.hpp>

#include "test_tls_cert.hpp"

using namespace demiplane::http;

namespace {
    std::string alpn_wire_http11() {
        std::string wire;
        wire.push_back('\x08');   // length of "http/1.1"
        wire += "http/1.1";
        return wire;
    }
}  // namespace

TEST(BuildSslContextTest, BuildsFromValidCert) {
    TlsConfig cfg;
    cfg.cert_file = http_tls_test::write_temp("cert.pem", http_tls_test::kTestCertPem);
    cfg.key_file  = http_tls_test::write_temp("key.pem", http_tls_test::kTestKeyPem);

    const std::string advertised = alpn_wire_http11();
    auto ctx = build_ssl_context(cfg, advertised);  // must not throw
    EXPECT_NE(ctx.native_handle(), nullptr);
}

TEST(BuildSslContextTest, ThrowsOnMissingCert) {
    TlsConfig cfg;
    cfg.cert_file = "/nonexistent/path/cert.pem";
    cfg.key_file  = "/nonexistent/path/key.pem";
    const std::string advertised = alpn_wire_http11();
    EXPECT_ANY_THROW({ auto ctx = build_ssl_context(cfg, advertised); });
}
```

- [ ] **Step 3: Register the test source + shared-cert include dir** — add `listeners/test_build_ssl_context.cpp` to the
  `Http.Listeners` source list, and add the shared-cert directory to that target's include path:

```cmake
target_include_directories(${UNIT_TESTING_TARGET}.Http.Listeners PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/integration_tests/http
)
```

- [ ] **Step 4: Build — expect failure** (`build_ssl_context.hpp` missing).

- [ ] **Step 5: Create `components/http/listeners/tls_listener/build_ssl_context.hpp`**

```cpp
#pragma once

#include <string>

#include <boost/asio/ssl/context.hpp>

#include <tls_config.hpp>

namespace demiplane::http {

    /**
     * @brief Build a hardened server ssl::context from cfg (spec §7.4).
     *
     * Sets the protocol-version floor, disables legacy protocols/compression,
     * loads cert/key (+ optional passphrase/DH/client-cert verification),
     * configures the session cache, and installs the ALPN select callback.
     *
     * The ALPN callback's `arg` is `&advertised_alpn_wire`, so that STRING MUST
     * OUTLIVE the returned context (D4) — TlsListener passes its own member.
     * `advertised_alpn_wire` is the length-prefixed protocol list (e.g.
     * "\x08http/1.1"). Throws boost::system::system_error on cert/key/DH/CA load
     * failure.
     */
    boost::asio::ssl::context build_ssl_context(const TlsConfig& cfg,
                                                const std::string& advertised_alpn_wire);

}  // namespace demiplane::http
```

- [ ] **Step 6: Create `components/http/listeners/tls_listener/build_ssl_context.cpp`**

```cpp
#include "build_ssl_context.hpp"

#include <cstddef>

#include <openssl/ssl.h>
#include <openssl/tls1.h>

namespace demiplane::http {

    namespace {
        // OpenSSL ALPN select callback (spike S2). `arg` is the advertised
        // length-prefixed protocol list (the listener's long-lived buffer, D4).
        int alpn_select_cb(SSL* /*ssl*/, const unsigned char** out, unsigned char* outlen,
                           const unsigned char* in, unsigned int inlen, void* arg) {
            const auto* advertised = static_cast<const std::string*>(arg);
            if (::SSL_select_next_proto(
                    const_cast<unsigned char**>(out), outlen,
                    reinterpret_cast<const unsigned char*>(advertised->data()),
                    static_cast<unsigned int>(advertised->size()), in, inlen)
                == OPENSSL_NPN_NEGOTIATED) {
                return SSL_TLSEXT_ERR_OK;
            }
            // OpenSSL 3.6.3 has no SSL_TLSEXT_ERR_ALPN_FAILED — ALERT_FATAL aborts
            // the handshake with no_application_protocol (spike S2 / D4).
            return SSL_TLSEXT_ERR_ALERT_FATAL;
        }
    }  // namespace

    boost::asio::ssl::context build_ssl_context(const TlsConfig& cfg,
                                                const std::string& advertised_alpn_wire) {
        namespace ssl = boost::asio::ssl;

        ssl::context ctx{ssl::context::tls_server};

        auto options = ssl::context::default_workarounds | ssl::context::no_sslv2
                       | ssl::context::no_sslv3 | ssl::context::no_tlsv1
                       | ssl::context::no_tlsv1_1 | ssl::context::no_compression
                       | ssl::context::single_dh_use;
        if (cfg.min_version == TlsConfig::MinVersion::tls13) {
            options |= ssl::context::no_tlsv1_2;
        }
        ctx.set_options(options);

        // Modern TLS 1.2 cipher floor (TLS 1.3 suites use OpenSSL's safe defaults).
        ::SSL_CTX_set_cipher_list(
            ctx.native_handle(),
            "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
            "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
            "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305");

        if (!cfg.key_passphrase.empty()) {
            ctx.set_password_callback(
                [pass = cfg.key_passphrase](std::size_t, ssl::context::password_purpose) {
                    return pass;
                });
        }

        ctx.use_certificate_chain_file(cfg.cert_file);             // throws on failure
        ctx.use_private_key_file(cfg.key_file, ssl::context::pem);  // throws on failure

        if (!cfg.dh_params_file.empty()) {
            ctx.use_tmp_dh_file(cfg.dh_params_file);
        }

        if (cfg.require_client_cert) {
            ctx.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
            if (!cfg.ca_file.empty()) {
                ctx.load_verify_file(cfg.ca_file);
            }
        }

        ::SSL_CTX_set_session_cache_mode(
            ctx.native_handle(),
            cfg.session_cache ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);

        // ALPN: arg points at the CALLER'S buffer — must outlive ctx (D4).
        ::SSL_CTX_set_alpn_select_cb(ctx.native_handle(), &alpn_select_cb,
                                     const_cast<std::string*>(&advertised_alpn_wire));
        return ctx;
    }

}  // namespace demiplane::http
```

- [ ] **Step 7: Create `components/http/listeners/tls_listener/CMakeLists.txt`** (STATIC — `build_ssl_context.cpp`; the
  `TlsListener` template header is added to this leaf in Task 10)

```cmake
##############################################################################
# Http Listeners — TlsListener<Drivers...> + build_ssl_context (TCP+TLS+ALPN)
##############################################################################
add_library(${DMP_HTTP}.Listeners.Tls STATIC build_ssl_context.cpp)

target_include_directories(${DMP_HTTP}.Listeners.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Listeners.Tls
        PUBLIC
        ${DMP_HTTP}.Listeners.Base
        ${DMP_HTTP}.Listeners.ConnectionTracker
        ${DMP_HTTP}.Connection.Tls
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Drivers.Concept
        ${DMP_HTTP}.Routing.Router
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
```

- [ ] **Step 8: Wire into the aggregate** — `add_subdirectory(tls_listener)` + `${DMP_HTTP}.Listeners.Tls` in
  `components/http/listeners/CMakeLists.txt`.

- [ ] **Step 9: Build + run — expect pass**

```bash
cmake --preset debug 2>&1 | tail -3
cmake --build build/debug --target Demiplane.Tests.Unit.Http.Listeners -- -j4 2>&1 | tail -5
ctest --test-dir build/debug --output-on-failure -R Http.Listeners 2>&1 | tail -8
```

- [ ] **Step 10: Commit**

```bash
git add tests/integration_tests/http/test_tls_cert.hpp \
        components/http/listeners/tls_listener/build_ssl_context.hpp \
        components/http/listeners/tls_listener/build_ssl_context.cpp \
        components/http/listeners/tls_listener/CMakeLists.txt components/http/listeners/CMakeLists.txt \
        tests/unit_tests/http/listeners/test_build_ssl_context.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/listeners): build_ssl_context — hardened server ctx + ALPN (spec §7.4)

Protocol floor, legacy-protocol/compression off, cert/key/DH/client-cert,
session cache, ALPN select cb (arg = caller's advertised buffer, D4).
Shared embedded test cert (valid to 2126) drives the unit + integration tests."
```

---

## Task 10: `TlsListener<Drivers...>` — TCP+TLS+ALPN dispatch

**Files:**

- Create: `components/http/listeners/tls_listener/tls_listener.hpp`
- Create: `tests/unit_tests/http/listeners/test_tls_listener.cpp`
- Modify: `components/http/listeners/tls_listener/CMakeLists.txt` (add the header to the leaf)
- Modify: `tests/unit_tests/http/CMakeLists.txt` (add the unit source)

**Goal:** Spec §7.3: a listener carrying a tuple of drivers; it advertises the union of their ALPN ids, builds the
hardened context in `bind()`, and after each handshake dispatches to the driver whose `id()` matches the
ALPN-negotiated protocol (compile-time tuple walk — spike S2). Construction/conformance is unit-checked here; the live
handshake + dispatch is integration (Task 11).

- [ ] **Step 1: Write the failing unit test** — `tests/unit_tests/http/listeners/test_tls_listener.cpp`

```cpp
#include <concepts>

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <listener_base.hpp>
#include <tls_config.hpp>
#include <tls_listener.hpp>

using namespace demiplane::http;

static_assert(std::derived_from<TlsListener<Http11Driver>, ListenerBase>);

TEST(TlsListenerTest, ConstructsWithoutBinding) {
    boost::asio::io_context ioc;
    TlsConfig tls;  // empty cert paths — fine, bind() (which builds the ctx) is not called here
    TlsListener<Http11Driver> listener{ioc.get_executor(), "127.0.0.1", 0, tls,
                                       Http11Driver{Http11Config{}}};
    EXPECT_EQ(listener.bind_address(), "127.0.0.1");
}
```

- [ ] **Step 2: Register the test source** — add `listeners/test_tls_listener.cpp` to the `Http.Listeners` source list.

- [ ] **Step 3: Build — expect failure** (`tls_listener.hpp` missing).

- [ ] **Step 4: Create `components/http/listeners/tls_listener/tls_listener.hpp`**

```cpp
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/error.hpp>

#include <build_ssl_context.hpp>
#include <connection_tracker.hpp>
#include <http_driver_concept.hpp>
#include <listener_base.hpp>
#include <router.hpp>
#include <tls_config.hpp>
#include <tls_connection.hpp>

namespace demiplane::http {

    /**
     * @brief TLS listener for one or more drivers, multiplexed by ALPN (spec §7.3).
     *
     * Advertises the union of the drivers' accepted_alpns(); build_ssl_context
     * installs the ALPN select callback (D4). On each accept it constructs a
     * TlsConnection on a per-connection strand (D7), handshakes, and dispatches
     * to the driver whose id() matches the negotiated protocol (compile-time
     * tuple walk — spike S2). Server-preference order = template arg order.
     *
     * LIFETIME CONTRACT: drain_until(...) before destroying the listener (the
     * spawned coroutines hold a tracker Handle into this listener + serve via
     * `this`). Arena size is fixed at the 8 KB default in v1 (PR 6 wires
     * request_arena_size).
     */
    template <HttpDriver... Drivers>
    class TlsListener final : public ListenerBase {
        static_assert(sizeof...(Drivers) >= 1, "TlsListener needs at least one driver");

    public:
        TlsListener(boost::asio::any_io_executor exec, std::string host, std::uint16_t port,
                    TlsConfig tls, Drivers... drivers)
            : exec_{std::move(exec)},
              host_{std::move(host)},
              port_{port},
              tls_config_{std::move(tls)},
              drivers_{std::move(drivers)...},
              advertised_alpn_{build_alpn_wire()},
              acceptor_{exec_} {}

        void bind() override {
            // Build the context FIRST — a cert/key error throws before we bind a
            // socket (no half-open listener left behind).
            ctx_.emplace(build_ssl_context(tls_config_, advertised_alpn_));

            const boost::asio::ip::tcp::endpoint ep{boost::asio::ip::make_address(host_), port_};
            acceptor_.open(ep.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(ep);
            acceptor_.listen(boost::asio::socket_base::max_listen_connections);
        }

        boost::asio::awaitable<void> run(Router& router) override {
            namespace asio = boost::asio;
            for (;;) {
                auto strand = asio::make_strand(exec_);
                boost::beast::error_code ec;
                asio::ip::tcp::socket sock = co_await acceptor_.async_accept(
                    strand, asio::redirect_error(asio::use_awaitable, ec));
                if (ec == asio::error::operation_aborted) {
                    break;
                }
                if (ec) {
                    continue;
                }
                auto conn   = std::make_shared<TlsConnection>(std::move(sock), *ctx_);
                auto handle = tracker_.register_connection(conn, strand);
                asio::co_spawn(
                    strand,
                    [this, &router, conn, h = std::move(handle)]() -> asio::awaitable<void> {
                        const boost::beast::error_code hec = co_await conn->handshake();
                        if (hec) {                       // ALPN mismatch / TLS failure → close
                            co_await conn->async_close();
                            co_return;
                        }
                        const bool handled = co_await try_serve<0>(
                            *conn, router, conn->negotiated_protocol());
                        if (!handled) {                  // unreachable after a successful ALPN
                            co_await conn->async_close();
                        }
                    },
                    asio::detached);
            }
            // Refuse new connections during shutdown (spec §14.2; spike S4) — see
            // TcpListener::run for the rationale.
            boost::beast::error_code ignore;
            acceptor_.close(ignore);
            co_return;
        }

        boost::asio::awaitable<void> drain_until(
            std::chrono::steady_clock::time_point deadline) override {
            co_await tracker_.drain_until(exec_, deadline);
        }

        [[nodiscard]] std::string bind_address() const override {
            return host_;
        }
        [[nodiscard]] std::uint16_t bound_port() const override {
            return acceptor_.local_endpoint().port();
        }

    private:
        // Concatenate every driver's ALPN ids into the wire format (len-prefixed).
        static std::string build_alpn_wire() {
            std::string wire;
            (append_alpns<Drivers>(wire), ...);
            return wire;
        }
        template <typename D>
        static void append_alpns(std::string& wire) {
            for (const std::string_view alpn : D::accepted_alpns()) {
                wire.push_back(static_cast<char>(alpn.size()));
                wire.append(alpn);
            }
        }

        // Walk the driver tuple; the first whose id() == proto serves the conn.
        template <std::size_t I>
        boost::asio::awaitable<bool> try_serve(TlsConnection& conn, Router& router, Protocol proto) {
            if constexpr (I < sizeof...(Drivers)) {
                auto& drv = std::get<I>(drivers_);
                if (std::remove_reference_t<decltype(drv)>::id() == proto) {
                    co_await drv.serve(conn, router);
                    co_return true;
                }
                co_return co_await try_serve<I + 1>(conn, router, proto);
            } else {
                co_return false;
            }
        }

        boost::asio::any_io_executor exec_;
        std::string host_;
        std::uint16_t port_;
        TlsConfig tls_config_;
        std::tuple<Drivers...> drivers_;
        std::string advertised_alpn_;            // outlives ctx_ (ALPN cb arg, D4)
        std::optional<boost::asio::ssl::context> ctx_;
        boost::asio::ip::tcp::acceptor acceptor_;
        ConnectionTracker tracker_;
    };

}  // namespace demiplane::http
```

> Member declaration order matters: `advertised_alpn_` is declared **before** `ctx_` so it is constructed first and
> destroyed last — guaranteeing the ALPN callback's `arg` (`&advertised_alpn_`) outlives the context (D4).

- [ ] **Step 5: Add the header to the Tls leaf** — in `components/http/listeners/tls_listener/CMakeLists.txt`, add
  `tls_listener.hpp` to the `add_library(${DMP_HTTP}.Listeners.Tls STATIC build_ssl_context.cpp ...)` source list (so
  IDEs/grep see it; it is header-only otherwise):

```cmake
add_library(${DMP_HTTP}.Listeners.Tls STATIC
        build_ssl_context.cpp
        tls_listener.hpp
)
```

- [ ] **Step 6: Build + run — expect pass**

```bash
cmake --build build/debug --target Demiplane.Tests.Unit.Http.Listeners -- -j4 2>&1 | tail -8
ctest --test-dir build/debug --output-on-failure -R Http.Listeners 2>&1 | tail -8
```

- [ ] **Step 7: Commit**

```bash
git add components/http/listeners/tls_listener/tls_listener.hpp \
        components/http/listeners/tls_listener/CMakeLists.txt \
        tests/unit_tests/http/listeners/test_tls_listener.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/listeners): TlsListener<Drivers...> — TCP+TLS+ALPN dispatch (spec §7.3)

Advertises the union of driver ALPN ids; handshakes then dispatches to the
driver whose id() matches the negotiated protocol (compile-time tuple walk).
advertised_alpn_ declared before ctx_ so the ALPN cb arg outlives it (D4)."
```

---

## Task 11: Integration — h1-over-TLS handshake, ALPN, round trip, mismatch-closes

**Files:**

- Create: `tests/integration_tests/http/test_http_tls.cpp`
- Modify: `tests/integration_tests/http/CMakeLists.txt` (add the `Http.Tls` target)

**Goal:** Spec §14.2 TLS coverage end to end: a real `TlsListener<Http11Driver>` on `127.0.0.1:0` using the embedded
cert (D5); a Beast TLS client handshakes, ALPN negotiates `http/1.1`, and a request round-trips; a client offering only
`h2` has its handshake **rejected** (the spec's "listener without h2 driver → connection closes" — spike S2 negative).
This is the first exercise of the full `build_ssl_context` → `TlsConnection::handshake` → tuple-dispatch path.

- [ ] **Step 1: Create the test** — `tests/integration_tests/http/test_http_tls.cpp`

```cpp
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <openssl/ssl.h>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <request_context.hpp>
#include <tls_config.hpp>
#include <tls_listener.hpp>

#include "http_test_fixture.hpp"
#include "test_tls_cert.hpp"

using namespace demiplane::http;
namespace asio  = boost::asio;
namespace bhttp = boost::beast::http;

namespace {

    class TlsApiController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &TlsApiController::hello);
        }

    private:
        AsyncResponse hello(RequestContext ctx) {
            co_return ctx.ok("hello tls");
        }
    };

    // Synchronous Beast TLS client. connect_handshake() sets the ALPN offer and
    // returns the handshake error_code (empty = success).
    class TlsClient {
    public:
        TlsClient() : stream_{ioc_, ctx_} {
            ctx_.set_verify_mode(asio::ssl::verify_none);  // self-signed test cert
        }

        boost::beast::error_code connect_handshake(std::uint16_t port,
                                                   const std::vector<std::string_view>& alpns) {
            std::string wire;
            for (const auto a : alpns) {
                wire.push_back(static_cast<char>(a.size()));
                wire.append(a);
            }
            ::SSL_set_alpn_protos(stream_.native_handle(),
                                  reinterpret_cast<const unsigned char*>(wire.data()),
                                  static_cast<unsigned int>(wire.size()));
            stream_.next_layer().connect({asio::ip::make_address("127.0.0.1"), port});
            boost::beast::error_code ec;
            stream_.handshake(asio::ssl::stream_base::client, ec);
            return ec;
        }

        std::string negotiated_alpn() const {
            const unsigned char* proto = nullptr;
            unsigned int len           = 0;
            ::SSL_get0_alpn_selected(stream_.native_handle(), &proto, &len);
            return std::string{reinterpret_cast<const char*>(proto), len};
        }

        http_it::ParsedResponse get(const std::string& target) {
            bhttp::request<bhttp::string_body> req{bhttp::verb::get, target, 11};
            req.set(bhttp::field::host, "127.0.0.1");
            req.keep_alive(false);
            req.prepare_payload();
            bhttp::write(stream_, req);
            http_it::ParsedResponse res;
            boost::beast::flat_buffer buf;
            bhttp::read(stream_, buf, res);
            return res;
        }

    private:
        asio::io_context ioc_;
        asio::ssl::context ctx_{asio::ssl::context::tls_client};
        asio::ssl::stream<asio::ip::tcp::socket> stream_;
    };

    class HttpTlsTest : public http_it::HttpIntegrationFixture {
    protected:
        void SetUp() override {
            const std::string cert = http_tls_test::write_temp("cert.pem", http_tls_test::kTestCertPem);
            const std::string key  = http_tls_test::write_temp("key.pem", http_tls_test::kTestKeyPem);
            TlsConfig tls;
            tls.cert_file = cert;
            tls.key_file  = key;

            add_controller(std::make_shared<TlsApiController>());
            start(std::make_unique<TlsListener<Http11Driver>>(
                ioc_.get_executor(), "127.0.0.1", 0, tls, Http11Driver{Http11Config{}}));
        }
    };

}  // namespace

TEST_F(HttpTlsTest, HandshakeNegotiatesHttp11AndRoundTrips) {
    TlsClient client;
    const auto ec = client.connect_handshake(port_, {"h2", "http/1.1"});
    ASSERT_FALSE(ec) << ec.message();
    EXPECT_EQ(client.negotiated_alpn(), "http/1.1");

    auto res = client.get("/hello");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "hello tls");
}

TEST_F(HttpTlsTest, ClientOfferingOnlyH2IsRejected) {
    TlsClient client;
    // The h1-only listener advertises only "http/1.1"; an h2-only offer has no
    // overlap → server returns ALERT_FATAL → handshake fails (spike S2 / D4).
    const auto ec = client.connect_handshake(port_, {"h2"});
    EXPECT_TRUE(ec) << "handshake should fail when no ALPN protocol overlaps";
}
```

- [ ] **Step 2: Add the `Http.Tls` integration target** — append to `tests/integration_tests/http/CMakeLists.txt`:

```cmake
add_integration_test(${INTEGRATION_TESTING_TARGET}.Http.Tls
        test_http_tls.cpp
        LINK_LIBS
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
target_include_directories(${INTEGRATION_TESTING_TARGET}.Http.Tls PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
```

- [ ] **Step 3: Build + run — expect pass**

```bash
cmake --preset debug 2>&1 | tail -3
cmake --build build/debug --target Demiplane.Tests.Integration.Http.Tls -- -j4 2>&1 | tail -10
ctest --test-dir build/debug --output-on-failure -R Http.Tls 2>&1 | tail -15
```

Expected: handshake negotiates `http/1.1`, `GET /hello` → 200 "hello tls"; the h2-only client's handshake fails.

- [ ] **Step 4: Commit**

```bash
git add tests/integration_tests/http/test_http_tls.cpp tests/integration_tests/http/CMakeLists.txt
git commit -m "test(http/integration): h1-over-TLS handshake, ALPN, round trip, mismatch closes

Real TLS via the embedded cert: client offering h2+http/1.1 negotiates
http/1.1 and round-trips GET /hello; an h2-only client is rejected (no ALPN
overlap → ALERT_FATAL)."
```

---

## Task 12: `QuicListener<Http3Driver>` scaffold (D6)

**Files:**

- Create: `components/http/listeners/quic_listener/quic_listener.hpp`
- Create: `components/http/listeners/quic_listener/CMakeLists.txt`
- Create: `tests/unit_tests/http/listeners/test_quic_listener.cpp`
- Modify: `components/http/listeners/CMakeLists.txt`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Goal:** Spec §7.3: a compiling QUIC listener scaffold — `bind()` succeeds (no-op), `run()` returns immediately, a
`static_assert` enforces pairing with `Http3Driver` only. Links no ngtcp2/nghttp3 symbols (D6 / PR 3 D4). The real UDP +
QUIC impl lands in the h3 PR inside these same methods.

- [ ] **Step 1: Write the failing test** — `tests/unit_tests/http/listeners/test_quic_listener.cpp`

```cpp
#include <concepts>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

#include <http3_driver.hpp>
#include <listener_base.hpp>
#include <quic_listener.hpp>
#include <route_registry.hpp>
#include <router.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;

static_assert(std::derived_from<QuicListener<Http3Driver>, ListenerBase>);

TEST(QuicListenerTest, ScaffoldBindsAndRunsImmediately) {
    boost::asio::io_context ioc;
    QuicListener<Http3Driver> listener{ioc.get_executor(), "127.0.0.1", 8443, TlsConfig{},
                                       Http3Driver{}};
    listener.bind();  // no-op success
    EXPECT_EQ(listener.bind_address(), "127.0.0.1");
    EXPECT_EQ(listener.bound_port(), 8443u);

    RouteRegistry registry;
    ASSERT_TRUE(registry.freeze().empty());
    Router router{registry};
    boost::asio::co_spawn(ioc, listener.run(router), boost::asio::detached);
    ioc.run();  // run() co_returns immediately → io_context drains and returns
    SUCCEED();
}
```

- [ ] **Step 2: Register the test source** — add `listeners/test_quic_listener.cpp` to the `Http.Listeners` source list.

- [ ] **Step 3: Build — expect failure** (`quic_listener.hpp` missing).

- [ ] **Step 4: Create `components/http/listeners/quic_listener/quic_listener.hpp`**

```cpp
#pragma once

#include <chrono>
#include <concepts>
#include <cstdint>
#include <string>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <demiplane/scroll>

#include <http3_driver.hpp>
#include <listener_base.hpp>
#include <router.hpp>
#include <tls_config.hpp>

namespace demiplane::http {

    /**
     * @brief QUIC listener — SCAFFOLD (spec §7.3, D6).
     *
     * bind() succeeds as a no-op; run() logs a warning and returns. Pairs with
     * Http3Driver ONLY (QUIC is the h3 transport). The UDP socket + ngtcp2
     * handshake land inside these methods in the h3 PR — no surrounding change.
     * Links no ngtcp2/nghttp3 symbols (continues PR 3 D4).
     */
    template <typename Driver>
    class QuicListener final : public ListenerBase {
        static_assert(std::same_as<Driver, Http3Driver>,
                      "QuicListener pairs with Http3Driver only (spec §7.3)");

    public:
        QuicListener(boost::asio::any_io_executor exec, std::string host, std::uint16_t port,
                     TlsConfig tls, Driver driver)
            : host_{std::move(host)}, port_{port} {
            static_cast<void>(exec);
            static_cast<void>(tls);
            static_cast<void>(driver);
        }

        void bind() override {
            // Scaffold: no socket yet. The h3 PR opens the UDP socket here.
        }

        boost::asio::awaitable<void> run(Router& /*router*/) override {
            COMPONENT_LOG_WRN() << "QuicListener::run() not implemented (scaffold)";
            co_return;
        }

        boost::asio::awaitable<void> drain_until(
            std::chrono::steady_clock::time_point /*deadline*/) override {
            co_return;
        }

        [[nodiscard]] std::string bind_address() const override {
            return host_;
        }
        [[nodiscard]] std::uint16_t bound_port() const override {
            return port_;
        }

    private:
        std::string host_;
        std::uint16_t port_;
        SCROLL_COMPONENT_PREFIX("QuicListener");
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Create `components/http/listeners/quic_listener/CMakeLists.txt`**

```cmake
##############################################################################
# Http Listeners — QuicListener<Http3Driver> (SCAFFOLD; no ngtcp2/nghttp3, D6)
##############################################################################
add_library(${DMP_HTTP}.Listeners.Quic INTERFACE quic_listener.hpp)

target_include_directories(${DMP_HTTP}.Listeners.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Listeners.Quic INTERFACE
        ${DMP_HTTP}.Listeners.Base
        ${DMP_HTTP}.Drivers.Http3
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Routing.Router
        Demiplane::Common::Scroll
        Boost::asio
)
##############################################################################
```

- [ ] **Step 6: Wire into the aggregate** — `add_subdirectory(quic_listener)` + `${DMP_HTTP}.Listeners.Quic` in
  `components/http/listeners/CMakeLists.txt`.

- [ ] **Step 7: Build + run — expect pass**

```bash
cmake --preset debug 2>&1 | tail -3
cmake --build build/debug --target Demiplane.Tests.Unit.Http.Listeners -- -j4 2>&1 | tail -5
ctest --test-dir build/debug --output-on-failure -R Http.Listeners 2>&1 | tail -8
```

- [ ] **Step 8: Commit**

```bash
git add components/http/listeners/quic_listener components/http/listeners/CMakeLists.txt \
        tests/unit_tests/http/listeners/test_quic_listener.cpp tests/unit_tests/http/CMakeLists.txt
git commit -m "feat(http/listeners): QuicListener<Http3Driver> scaffold (spec §7.3, D6)

bind() no-op, run() logs + returns; static_assert pairs with Http3Driver only.
No ngtcp2/nghttp3 symbols — the h3 PR fills these methods in place."
```

---

## Task 13: Full-layer verification + sanitizer pass

**Files:**

- Verify (no new files): `components/http/listeners/CMakeLists.txt` aggregate links all five leaves.

**Goal:** Prove the whole layer builds and every HTTP test (unit + integration) passes under the strict flags, and that
the concurrency-sensitive paths are clean under ASan (the tracker force-cancel UAF guard + the real socket/TLS paths).
No code changes expected — this is the integration checkpoint.

- [ ] **Step 1: Confirm the listeners aggregate is complete.** `components/http/listeners/CMakeLists.txt` should now read:

```cmake
add_subdirectory(listener_base)
add_subdirectory(connection_tracker)
add_subdirectory(tcp_listener)
add_subdirectory(tls_listener)
add_subdirectory(quic_listener)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Listeners INTERFACE)

target_link_libraries(${DMP_HTTP}.Listeners INTERFACE
        ${DMP_HTTP}.Listeners.Base
        ${DMP_HTTP}.Listeners.ConnectionTracker
        ${DMP_HTTP}.Listeners.Tcp
        ${DMP_HTTP}.Listeners.Tls
        ${DMP_HTTP}.Listeners.Quic
)
##############################################################################
```

- [ ] **Step 2: Clean configure + build every HTTP target (debug)**

```bash
cmake --preset debug 2>&1 | tail -5
cmake --build build/debug --target \
    Demiplane.Tests.Unit.Http.Connection \
    Demiplane.Tests.Unit.Http.Listeners \
    Demiplane.Tests.Unit.Http.Drivers \
    Demiplane.Tests.Unit.Http.Routing \
    Demiplane.Tests.Unit.Http.Types \
    Demiplane.Tests.Integration.Http.Tcp \
    Demiplane.Tests.Integration.Http.Tls \
    -- -j4 2>&1 | tail -15
```

Expected: all targets build clean under `-Werror -Wconversion -Wshadow -Wthread-safety …` (no warnings).

- [ ] **Step 3: Run the full HTTP test suite**

```bash
ctest --test-dir build/debug --output-on-failure -R "Http\." 2>&1 | tail -25
```

Expected: every `Http.*` unit + integration test passes; no hangs (each integration fixture joins its worker on
graceful shutdown).

- [ ] **Step 4: ASan pass on the concurrency-sensitive targets** (the tracker UAF guard + the real-socket/TLS paths). If
  the `asan` preset configures:

```bash
cmake --preset asan 2>&1 | tail -3 && \
  cmake --build build/asan --target \
      Demiplane.Tests.Unit.Http.Listeners \
      Demiplane.Tests.Integration.Http.Tcp \
      Demiplane.Tests.Integration.Http.Tls -- -j4 2>&1 | tail -5 && \
  ctest --test-dir build/asan --output-on-failure -R "Http\.(Listeners|Tcp|Tls)" 2>&1 | tail -20 || \
  echo "asan preset unavailable — debug coverage stands (note in the PR)"
```

Expected: clean (no leak/UAF reports). The force-cancel-on-drain path (Tasks 4, 7) is the specific thing ASan guards
here (spike S1).

- [ ] **Step 5: TSan pass if it configures** (the multi-thread accept/strand path). The `tsan` preset is `common/`-only
  per its description — if it builds the HTTP targets, run them; otherwise record that the N-worker/TSan concurrency
  matrix is deferred to PR 5 (the Server owns the thread topology, spec §14.2):

```bash
cmake --preset tsan 2>&1 | tail -3 && \
  cmake --build build/tsan --target Demiplane.Tests.Integration.Http.Tcp -- -j4 2>&1 | tail -5 && \
  ctest --test-dir build/tsan --output-on-failure -R "Http\.Tcp" 2>&1 | tail -10 || \
  echo "tsan preset does not build components — N-worker/TSan matrix is a PR 5 item"
```

- [ ] **Step 6: Commit (verification checkpoint — typically no code change)**

```bash
# If Step 1 required an aggregate fix, commit it; otherwise this task is a checkpoint only.
git add components/http/listeners/CMakeLists.txt 2>/dev/null || true
git commit -m "build(http/listeners): finalize the Listeners aggregate; full HTTP suite green" --allow-empty
```

---

## Self-Review (run against the spec before declaring done)

**1. Spec coverage (§7 + §12.2 PR 4):**

| Spec item                                         | Task(s)         |
|---------------------------------------------------|-----------------|
| §7.1 `ListenerBase` (bind/run/drain/addr/port)    | Task 3          |
| §7.2 `ConnectionTracker` (in-flight + force-cancel)| Tasks 4, 7      |
| §7.3 `TcpListener<Driver>`                        | Tasks 5, 6      |
| §7.3 `TlsListener<Drivers...>` + ALPN dispatch    | Task 10, 11     |
| §7.3 `QuicListener<Http3Driver>` scaffold         | Task 12         |
| §7.4 `build_ssl_context` (hardened ctx + ALPN)    | Task 9          |
| §6.1 `TlsConnection` (PR-3 D3 deferral)           | Task 8          |
| §10.1 `TlsConfig` (plain struct, D1)              | Task 2          |
| §14.2 integration: h1-over-TCP, h1-over-TLS, ALPN | Tasks 6, 11      |
| §14.2 graceful shutdown: in-flight completes, idle force-cancelled, **new connections refused** | Task 7 |

**2. Placeholder scan:** every task ships complete code — no "TBD"/"add error handling"/"similar to Task N". The
embedded cert and `build_ssl_context` body are full. ✅

**3. Type/name consistency** (checked against the landed code + within this plan):
- `Connection.Tls` STATIC leaf (Task 8) ↔ linked by `Listeners.Tls` (Task 9) ↔ in the `Connection` aggregate (Task 8). ✅
- `cancel()` added to `TcpConnection` (Task 5) + `TlsConnection` (Task 8); required by `ConnectionTracker::register_connection`
  (Task 4) and emitted via the tracker's strand dispatch. ✅
- `build_ssl_context(const TlsConfig&, const std::string&)` (Task 9) ↔ called by `TlsListener::bind()` (Task 10) with the
  `advertised_alpn_` member; member-order keeps the ALPN-cb arg alive (D4). ✅
- `TlsConnection::handshake()` returns `awaitable<beast::error_code>` (Task 8) ↔ awaited in `TlsListener::run` (Task 10). ✅
- `HttpIntegrationFixture::graceful_shutdown` (added Task 7) ↔ called by TLS test's TearDown via the base (Task 11). ✅
- `${DMP_HTTP}.Listeners.{Base,ConnectionTracker,Tcp,Tls,Quic}` ↔ aggregate link list (Task 13). ✅
- ALPN failure return is `SSL_TLSEXT_ERR_ALERT_FATAL` everywhere (spike S2). ✅

**4. Known risks already retired by spikes (Reconciliation table):** tracker force-cancel UAF (S1, ASan-clean); first
`asio::ssl` use compiles + links + handshakes (S2); ALPN arg lifetime (S2 + D4 member-order); compile-time tuple dispatch
(S2). Residual risk is mechanical (CMake link lists, include names) and caught by the per-task build steps.

**5. Deferred-and-documented (not gaps):** N-worker/TSan concurrency matrix and `run_standalone`/per-listener stop
signals → PR 5 (Server owns the thread topology + cancellation fan-out); `ConfigInterface`-backed `TlsConfig` + cipher
configurability → PR 6; real h2/h3/QUIC bodies → future impl PRs; cert-chain/OCSP/session-resumption TLS edge cases →
out of v1 scope (spec §14.3).

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-06-15-http-listeners-tls-layer.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — dispatch a fresh subagent per task, review between tasks, fast iteration.

**2. Inline Execution** — execute tasks in this session using executing-plans, batch execution with checkpoints.

**Which approach?**

- **If Subagent-Driven:** REQUIRED SUB-SKILL `superpowers:subagent-driven-development` — fresh subagent per task + two-stage review.
- **If Inline Execution:** REQUIRED SUB-SKILL `superpowers:executing-plans` — batch execution with checkpoints.

> **Sequencing note for the executor:** Tasks are ordered TCP-green-first — Tasks 1–7 produce a working, wire-tested
> h1-over-TCP server before any TLS code (Tasks 8–12). The two load-bearing mechanisms were spike-validated
> (Reconciliation → "Validated mechanisms"); if a task's "complete code" fails to compile, the deviation is mechanical
> (a link/include name) — fix it against the landed target names, not by redesigning the mechanism. Per the user's
> standing preference, **the executor does not run git operations** — the per-task `git` blocks are the recommended
> grouping for when the user commits.
