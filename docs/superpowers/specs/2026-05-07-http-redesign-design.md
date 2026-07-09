# HTTP Component Redesign — Design Spec

**Date:** 2026-05-07
**Branch:** `component/http-1.1/v1.7`
**Status:** COMPLETE — all 7 PRs landed. PR 7 (umbrella + cleanup) plan:
`docs/superpowers/plans/2026-07-09-http-cleanup-umbrella.md`; earlier per-PR plans under `docs/superpowers/plans/`.
Reviewed + reconciled 2026-06-09; final reconciliation 2026-07-09.
**Component:** `components/http/`

## 1. Context

The current `components/http/` module is a thin Boost.Beast HTTP/1.1 server. It introduces a useful idea — controllers
compile their own routes locally and then merge them into a server-wide registry — but the surrounding scaffolding has
accumulated structural and correctness problems that block both production use and the planned multi-protocol future (
HTTP/2 + HTTP/3).

A code review of the current module identified, among others:

- A use-after-free in async error callbacks (reference-captured `std::exception&` outliving its scope).
- Async stop callbacks scheduled onto a stopped `io_context` and silently dropped.
- Handler exceptions producing a dropped TCP connection rather than an HTTP 500 response.
- A fully stubbed `parse_multipart_body` that returns empty success indistinguishable from "no parts."
- No URL decoding anywhere — query and form parsing return raw `%XX`.
- No body-size limit, no read/write timeouts (slow-loris and giant-body DoS are wide open).
- No synchronization on shared mutable state under N io threads.
- `find_handler` throws on every 404, control-flow-via-exceptions on a normal HTTP outcome.
- Half the config layer (`load_from_yaml`, `tls_settings`, `ip_rule`, `route_table`) is declared but never consumed.
- `Server` is a god class owning io_context, threads, accept loop, session loop, request handling, query parsing, route
  registry, controllers, middleware, ten event-callback vectors, signal handling, and lifecycle state.

Those bugs are symptoms. The architectural problems are deeper: there is no layering between transport, framing,
routing, and application; `Server` does too much; the event/callback bus is reinvented and ten-times-duplicated; the
sync-vs-async DSL combinatorially explodes (12 overloads per HTTP verb); there is no Connection abstraction; there is no
request lifecycle hook. The two parallel "Server" concepts (`router_config::server` data class vs `http::Server` runtime
class) are unrelated — one of them is dead code. Tests do not exist.

This spec proposes a clean redesign that fixes the bugs as a side effect of getting the architecture right, sets up the
protocol-driver boundary cleanly so HTTP/2 and HTTP/3 can be filled in later, and keeps the controller-merge idea (the
strongest part of the existing design) at the center.

## 2. Goals and Non-Goals

### Goals

1. **Layered architecture** with one-way dependencies: application → routing → HTTP semantics → protocol drivers →
   connections → transport. Each layer has a focused job.
2. **Multi-protocol-ready** through a clean driver interface: HTTP/1.1 implemented in v1; HTTP/2 and HTTP/3 ship as
   compiling scaffolds with vcpkg deps wired so future-you can fill them in without touching the build.
3. **Controller-merge as the primary application abstraction** — `HttpController` subclasses with `configure_routes()`
   stay the canonical pattern. Groups, middleware, prefixes, and per-route Outcome→Response error mapping all compose
   around it.
4. **Zero-additional-allocation invariant.** Per-request arena in *both* directions: header views into Beast's parsed
   storage with Beast's allocator pointed at our arena, and responses built through the `RequestContext` so their
   headers + body land in the same arena. No `std::regex`, no exception-driven control flow on routing misses. On a
   success request the only heap allocations are the user's response-body bytes plus a small **counted budget of
   coroutine frames** (≈3 + middleware depth, §11.1). This is an **enforced invariant** (operator-new-counting test
   gate, §14), not an aspiration; cold-path 4xx/5xx error responses are the one documented exception (§5.5).
5. **Typed error model** via `gears::Outcome<Response, Errors...>` for handlers, ADL-found `to_http_response(const E&)`
   for conversion, exception catch-all for unexpected errors only.
6. **Honest lifecycle on an injected executor.** The `Server` is *handed* an executor (`any_io_executor`); it owns no
   `io_context` and no threads. `setup()` binds synchronously (surfacing failures immediately) and `co_spawn`s the
   accept loops onto the caller's executor; `stop()` is non-blocking, idempotent, and **never stops the executor** (it
   may be shared with other subsystems); `wait_until_stopped()` blocks the caller until graceful shutdown completes.
   Async shutdown observers run on the still-driven executor *before* completion is reported. A
   `run_standalone(threads)` convenience owns the context+threads for trivial apps (§9).
7. **Real TLS and config.** `TlsConfig` consumed by `TlsListener`, `build_ssl_context` produces a hardened OpenSSL
   context with ALPN, `ServerConfig` loaded from JSON via the project's existing `serialization::ConfigInterface`
   pattern.
8. **Real test coverage.** Unit tests for pure logic; integration tests on `127.0.0.1:0` via real Beast clients
   exercising the wire.

### Non-goals (v1)

1. **Implementing HTTP/2 and HTTP/3.** Drivers ship as scaffolds. vcpkg manifests carry `nghttp2`, `ngtcp2`, `nghttp3`
   so the implementations land as future PRs without build-system changes.
2. **YAML config.** JSON via `ConfigInterface` is the v1 format. YAML is a strictly additive follow-up using the same
   `ConfigInterface` pattern with a different format type parameter.
3. **Hot-reload.** `reload_if_changed` is removed (not stubbed). Restart-on-change is the v1 deployment model.
   Hot-reload would need its own design pass and probably a `Server::reconfigure(...)` API.
4. **Server push** (h2 PUSH_PROMISE) and **Early Hints** (103). Both require the response abstraction to support
   multi-response semantics; deferred.
5. **Path-flag gating** (the dead `route_table` from current config). Not a server-core concern; if anyone wants it,
   it's a middleware.
6. **Performance regression tests.** Separate benchmarks; not part of the unit/integration suite.

## 3. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│ Application                                                     │
│   HttpController subclasses, handler methods, error types       │
├─────────────────────────────────────────────────────────────────┤
│ Routing                                                         │
│   RouteRegistry — exact + parametric, group prefixes, baked     │
│   middleware chains. Mutable at startup, immutable at runtime.  │
├─────────────────────────────────────────────────────────────────┤
│ HTTP Semantics (protocol-agnostic)                              │
│   Request, Response, Headers, Body (stream + buffered helpers), │
│   RequestContext, gears::Outcome<Response, Errors...>           │
├─────────────────────────────────────────────────────────────────┤
│ Protocol Drivers (IsHttpDriver concept)                         │
│   Http11Driver — implemented (Boost.Beast under the hood)       │
│   Http2Driver  — scaffold (nghttp2 vcpkg manifest in place)     │
│   Http3Driver  — scaffold (ngtcp2 + nghttp3 in place)           │
├─────────────────────────────────────────────────────────────────┤
│ Connections (IsConnection concept)                              │
│   TcpConnection, TlsConnection, QuicConnection (stub)           │
│   each owns its byte stream + lifecycle for one peer            │
├─────────────────────────────────────────────────────────────────┤
│ Transport / Listeners                                           │
│   TcpListener<Driver>           — plain TCP                     │
│   TlsListener<Drivers...>       — TCP+TLS+ALPN dispatch         │
│   QuicListener<Http3Driver>     — UDP+QUIC (scaffold)           │
└─────────────────────────────────────────────────────────────────┘
```

**Key principles:**

- **One-way dependencies, top to bottom.** Application doesn't know which driver served the request. Drivers don't know
  which controllers exist; they only know `Router&`. Listeners don't know which drivers run on them at the source
  level — pairing is enforced via templates.
- **Routes baked at startup, immutable at runtime.** `RouteRegistry::find_route()` returns a single composed callable —
  group-prefix-matched, middlewares pre-attached, Outcome→Response conversion already wired. Zero per-request route
  composition. Registration after `setup()` is `std::logic_error`.
- **`Server` is a thin orchestrator.** Owns the io_context, listeners, drivers (held inside listeners), the router,
  observer list, controller list. Doesn't loop sessions itself — drivers do that.
- **The build/buy line is the driver's `serve()` method.** Whatever happens inside `serve()` is the driver's problem.
  `Http11Driver::serve()` uses Beast and looks like a session loop. Future `Http2Driver::serve()` will wrap nghttp2; the
  impedance mismatch lives there, not in the public interface.
- **No virtual base for `IsConnection` or `IsHttpDriver` conformers.** Concept-based duck typing; polymorphism only at
  the listener
  layer (where `Server` holds `unique_ptr<ListenerBase>`). No `dynamic_cast` anywhere in the runtime path.
- **Per-request arena, both directions.** Beast's `request_parser<..., pmr_allocator>` parses headers/path/body into our
  request-scoped `monotonic_buffer_resource`; `Headers` wraps Beast's parsed `fields` for incoming and arena-owned
  strings for outgoing. **Responses are arena-backed too** — built through the `RequestContext` (§5.4) so headers and
  the response's *stored* allocator point at the arena, which keeps even post-handler middleware mutation off the global
  heap. The framework path holds a **zero-additional-allocation invariant**: beyond a counted coroutine-frame budget (
  §11.1), the only heap alloc on a success request is the user's response-body bytes. Enforced by a replaced-global-
  `operator new` counting test gate (§14), not by convention. Cold-path 4xx/5xx error responses are the one documented
  exception (§5.5).
- **The Server is handed an executor; it never makes one.** `Server(cfg, any_io_executor)` runs its accept loops and
  shutdown coroutines on the caller's executor and owns no `io_context` and no threads (§9). The caller decides the
  thread↔context topology — one dedicated context for HTTP, a shared pool, or per-core `io_context`s + `SO_REUSEPORT`
  for scale-out — so HTTP coexists with the logger, DB pool, and S3 client instead of competing for threads. (This also
  subsumes the per-thread-`io_context` throughput lesson: with one thread per context there is no shared-scheduler-lock
  contention.)

## 4. Directory Structure

Co-located per-thing directories. Each "thing" (a class, a function family, a small set) gets its own subdirectory
containing both header and source. No separate `include/`/`source/` trees.

```
components/http/
├─ types/
│  ├─ url_decode/        {url_decode.hpp, url_decode.cpp}   # shared RFC 3986 decoder
│  ├─ request/           {request.hpp}
│  ├─ response/          {response.hpp}
│  ├─ headers/           {headers.hpp, headers.cpp}
│  ├─ body/              {body.hpp, body.cpp}
│  ├─ request_context/   {request_context.hpp, request_context.cpp}
│  ├─ response_factory/  {response_factory.hpp, response_factory.cpp}
│  ├─ errors/            {errors.hpp, errors.cpp}
│  ├─ async_outcome/     {async_outcome.hpp}   # header-only alias
│  └─ enums/             {http_enums.hpp}      # Protocol, HttpMethod, HttpStatus, HttpVersion
├─ routing/
│  ├─ route_registry/    {route_registry.hpp, route_registry.cpp}
│  ├─ controller/        {controller.hpp, controller.cpp}
│  ├─ middleware/        {middleware.hpp}
│  ├─ group/             {group.hpp}
│  └─ router/            {router.hpp, router.cpp}
├─ connection/
│  ├─ concepts/          {connection_concepts.hpp}   # the layer's contract
│  ├─ request_arena/     {request_arena.hpp, request_arena.cpp}
│  ├─ tcp_connection/    {tcp_connection.hpp, tcp_connection.cpp}
│  ├─ tls_connection/    {tls_connection.hpp, tls_connection.cpp}
│  └─ quic_connection/   {quic_connection.hpp}    # scaffold
├─ drivers/
│  ├─ http_driver_concept/  {http_driver_concept.hpp}
│  ├─ http11/               {http11_driver.hpp, http11_driver.cpp, http11_config.hpp}
│  ├─ http2/                {http2_driver.hpp}    # scaffold
│  └─ http3/                {http3_driver.hpp}    # scaffold
├─ listeners/
│  ├─ listener_base/        {listener_base.hpp}
│  ├─ connection_tracker/   {connection_tracker.hpp, connection_tracker.cpp}
│  ├─ tcp_listener/         {tcp_listener.hpp}
│  ├─ tls_listener/         {tls_listener.hpp, build_ssl_context.hpp, build_ssl_context.cpp}
│  └─ quic_listener/        {quic_listener.hpp}    # scaffold
├─ server/
│  ├─ server/                    {server.hpp, server.cpp}
│  ├─ server_observer/           {server_observer.hpp}
│  ├─ run_standalone/            {run_standalone.hpp, run_standalone.cpp}
│  └─ attach_default_listeners/  {attach_default_listeners.hpp, attach_default_listeners.cpp}
├─ config/
│  ├─ server_config/         {server_config.hpp}
│  ├─ listener_config/       {listener_config.hpp}
│  ├─ tls_config/            {tls_config.hpp}
│  ├─ timeouts/              {timeouts.hpp}
│  └─ load_server_config/    {load_server_config.hpp, load_server_config.cpp}
└─ export/demiplane/http     # umbrella header (all 40 public leaf headers)
```

Tests live outside the component: `tests/unit_tests/http/` and `tests/integration_tests/http/`.

CMake `target_include_directories` points at each layer's root (e.g. `types`), so includes from sister layers look like
`<request/request.hpp>`, `<route_registry/route_registry.hpp>`. Within a layer, a `.cpp` includes its own header via
`"foo.hpp"` (relative); cross-thing-within-the-same-layer includes use the rooted form for grep-ability.

## 5. Core Types

### 5.1 `Headers`

Multi-value capable, case-insensitive lookup, stable insertion order. Internal tagged-union backing: views into Beast's
parsed `fields` for incoming requests, arena-owned strings for outgoing responses.

```cpp
class Headers {
    struct BeastBacking { const beast::http::fields* fields; };
    struct OwnedBacking {
        std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>> entries;
    };
    std::variant<BeastBacking, OwnedBacking> backing_;

public:
    static Headers owned(std::pmr::polymorphic_allocator<> alloc);     // empty, mutable
    static Headers view_of_beast(const beast::http::fields& fields);   // read-only view

    // Move-only. Move-ASSIGN is user-defined: it adopts the source's backing
    // wholesale (variant emplace → pmr::vector move-construct steals buffer +
    // allocator). A defaulted move-assign hits the pmr POCMA=false trap —
    // element-wise COPY into the destination's old allocator, so
    // `Response r; r = co_await next(ctx);` would silently copy arena headers
    // onto the global heap.
    Headers(Headers&&) = default;
    Headers& operator=(Headers&&) noexcept;

    void add(std::string_view name, std::string_view value);   // requires OwnedBacking
    void set(std::string_view name, std::string_view value);   // replaces all of name
    void remove(std::string_view name);

    /// Copy a BeastBacking into a fresh OwnedBacking in `alloc`; no-op if
    /// already owned. MUST precede mutation of a viewing Headers (mutators
    /// assert the owned state).
    void promote_to_owned(std::pmr::polymorphic_allocator<> alloc);

    std::optional<std::string_view> get(std::string_view name) const;
    std::string get_or(std::string_view name, std::string_view fallback) const;

    // Multi-value reads collect into a caller-allocated (arena) vector — there
    // is no contiguous string_view storage a span could point into.
    std::pmr::vector<std::string_view> get_all(
        std::string_view name, std::pmr::polymorphic_allocator<> alloc) const;

    bool contains(std::string_view name) const;

    const_iterator begin() const;   // insertion order; O(1) per step — the
    const_iterator end() const;     // iterator carries the backing's native one
};
```

`std::visit` over the 2-element variant compiles to a tag check + direct call; modern compilers inline through it.

**Construction / empty state.** A `Headers` is always constructed *with an allocator* — `Headers::owned(alloc)` for an
empty, mutable set (the request arena for responses), or `Headers::view_of_beast(fields)` for an incoming read-only
view. There is **no default/null state**: an "empty" `Headers` is an empty `OwnedBacking` bound to its allocator, never
a `BeastBacking{nullptr}` (which would make `get`/`add` a null deref). Mutators (`add`/`set`/`remove`) require the owned
backing — mutating a Beast view asserts; call `promote_to_owned(alloc)` first. Mutation and promotion allocate through
the explicitly bound allocator — **never the global heap**.

### 5.2 `Body` — streaming truth, buffered helpers

```cpp
class Body {   // value type, ~48-byte SBO, internal-vtable type erasure — NOT a virtual base
public:
    Body() noexcept;                        // EmptyBody
    static Body empty() noexcept;
    static Body owned(std::string bytes);   // OwnedBufferBody
    // PR3+ payloads: BeastRequestBody (zero-copy request), StreamingProducerBody.

    // Streaming read — next chunk, std::nullopt when done. Payloads implement.
    asio::awaitable<std::optional<std::span<const std::byte>>> read_chunk();

    // Set when Content-Length present; absent for chunked-encoded bodies.
    std::optional<std::size_t> size_hint() const;

    // Whole-body view for non-streaming payloads — the driver fast-path and
    // the test-inspection idiom (replaces dynamic_cast<StringBody*>).
    // nullopt for streaming payloads; "" for EmptyBody.
    std::optional<std::string_view> buffered_view() const;

    // Buffered helpers — built on read_chunk(). All take a limit; failing the
    // limit short-circuits with BodyLimitExceeded. Results are handler-local
    // user data and allocate on the global heap by design (§11.3).
    AsyncOutcome<std::string, BodyLimitExceeded>
        read_to_string(std::size_t limit);
    AsyncOutcome<Json::Value, JsonParseError, BodyLimitExceeded>
        read_json(std::size_t limit);
    AsyncOutcome<std::unordered_map<std::string, std::string>,
                 FormParseError, BodyLimitExceeded>
        read_form(std::size_t limit);
    AsyncOutcome<std::vector<MultipartField>,
                 MultipartParseError, BodyLimitExceeded>
        read_multipart(std::size_t limit, std::string_view boundary);   // real impl; boundary comes from the Content-Type header
};
```

Concrete implementations:

- `BeastRequestBody` — zero-copy span over Beast's parsed body buffer.
- `OwnedBufferBody` — for outgoing responses with a known string body.
- `StreamingProducerBody` — for outgoing responses built incrementally via a producer closure.
- `EmptyBody` — for `GET`, `HEAD`, `204` responses, etc. Inline-empty kind, 0 allocs.

`Body` is held inline as a value type with small-buffer-optimized, type-erased storage (SBO budget sized to hold a
`std::string` by value, ~48 bytes). The common bodies — `EmptyBody`, `OwnedBufferBody`/`StringBody`,
`BeastRequestBody` — live entirely inline: **zero** heap nodes. Only `StreamingProducerBody`'s closure can spill, and it
spills **into the request arena**, never the global heap. No `unique_ptr<Body>` indirection anywhere. (The driver writes
a body either by driving `read_chunk()` uniformly or, for non-streaming payloads, in one shot via `buffered_view()` —
the value-SBO type-erasure dispatches internally, so there is still **no `dynamic_cast` on the runtime path**, per §3.)

**Lifetime invariant (see §11).** A request `Body` views connection-owned buffers; a response `Body`/`Headers` is
arena-backed. Neither may outlive the arena `reset()` at the top of the next keep-alive iteration — the h1 driver (PR3)
must finish writing the response *before* it resets the arena for the next read.

### 5.3 `Request` / `Response`

```cpp
struct Request {
    HttpMethod method;
    std::string_view target;     // view into receive buffer (Beast's flat_buffer)
    HttpVersion version;
    Headers headers;
    Body body;
};

struct Response {
    // The allocator is STORED, not just used at construction. Built on the hot
    // path through the RequestContext (§5.4/§5.7), it points at the request
    // arena — so middleware that mutates the response AFTER the handler returns
    // (`auto r = co_await next(ctx); r.add_header(...)`) keeps those allocations
    // in the arena too. Defaults to new_delete for the ctx-less / error path.
    std::pmr::polymorphic_allocator<> alloc{};

    HttpStatus  status     = HttpStatus::ok;
    HttpVersion version    = HttpVersion::http_1_1;
    bool        keep_alive = true;          // carried over from the request
    Headers     headers    = Headers::owned(alloc);
    Body        body;                        // value type, SBO (§5.2); default EmptyBody

    // Fluent setters (deducing this; chain on lvalues + rvalues, allocate via
    // `alloc`). set_header replaces; add_header appends (multi-value) — the
    // distinction the old single `with_header` lacked. with_body moves owned
    // bytes in (no copy).
    template <typename Self> auto&& with_status(this Self&&, HttpStatus);
    template <typename Self> auto&& set_header (this Self&&, std::string_view n, std::string_view v);
    template <typename Self> auto&& add_header (this Self&&, std::string_view n, std::string_view v);
    template <typename Self> auto&& with_body  (this Self&&, std::string body);
};
```

`HttpMethod`, `HttpStatus`, `HttpVersion`, `Protocol` are enums in `types/http_enums.hpp`. Drivers translate to/from
wire format internally.

### 5.4 `RequestContext`

The handler's view of one in-flight request. Lazy header lookup, type-keyed data bag, pre-decoded path/query params from
the request arena.

```cpp
class RequestContext {
public:
    // Request data
    HttpMethod method() const;
    std::string_view target() const;
    std::string_view path() const;          // raw (undecoded) — split at '?' lazily; decoding policy §8.6
    std::string_view query_string() const;
    HttpVersion version() const;
    const Headers& headers() const;
    Body& body();

    // Path parameters (pre-decoded)
    template <typename T> std::optional<T> path_param(std::string_view name) const;
    template <typename T> T path_param_or(std::string_view name, T fallback) const;

    // Query parameters (pre-decoded)
    template <typename T> std::optional<T> query(std::string_view name) const;
    template <typename T> T query_or(std::string_view name, T fallback) const;

    // Header convenience
    std::optional<std::string_view> header(std::string_view name) const;
    bool is_json() const;
    bool is_form() const;
    bool is_multipart() const;
    bool accepts_json() const;

    // Middleware data bag (type-keyed)
    template <typename T> void set(T value);
    template <typename T> T*   get();
    template <typename T> bool has() const;

    // Arena access (for handler-allocated short-lived data)
    std::pmr::polymorphic_allocator<> arena_alloc() const;

    // Response construction (arena-bound) — the hot-path factory. These build
    // the Response IN this request's arena (headers + the response's stored
    // allocator), so a success response costs zero global-heap allocations
    // beyond the user's body bytes (§11). This is the canonical way to build a
    // response; the static ResponseFactory (§5.7) is only for ctx-less / error
    // contexts. (Option A from the design review — the arena is threaded in
    // automatically, so it can't be forgotten.)
    Response ok        (std::string body = "", std::string_view ct = "text/plain");
    Response json      (std::string body);
    Response created   (std::string body = "", std::string_view ct = "application/json");
    Response no_content();
    Response redirect  (std::string_view location, HttpStatus = HttpStatus::found);
    Response status    (HttpStatus, std::string body = "", std::string_view ct = "text/plain");
    Response stream    (HttpStatus, std::function<asio::awaitable<void>(Body::Writer&)> producer);   // PR3+ (needs the driver's Body::Writer)

private:
    Request request_;
    // Arena-backed small_vector<pair, 4> + linear scan — a handful of params;
    // a flat map buys nothing at this size. (Boost.Container has no
    // "small_flat_map"; this is the concrete shape.)
    boost::container::small_vector<
        std::pair<std::pmr::string, std::pmr::string>, 4,
        std::pmr::polymorphic_allocator<...>> path_params_;
    boost::container::small_vector<...> query_params_;
    TypedBag bag_;   // arena-backed, SBO of 4 entries inline
};
```

The `TypedBag` is an arena-backed `type_index`-keyed store with small-buffer optimization for up to 4 entries inline;
`std::any` is *not* used (it heap-allocates internally for non-SBO types). Setting and getting middleware-bag values is
one arena bump for the value plus an inline key entry. `RequestContext` is move-*constructed* through the chain; move-
*assignment* is deleted (a defaulted one would replace the bag without running the old payloads' destructors).

### 5.5 `errors.hpp` — built-in error types + ADL `to_http_response`

```cpp
namespace demiplane::http {

// Built-in error types. Plain data; HTTP-aware via paired free function.
struct BadRequestError    { std::string message; };
struct UnauthorizedError  { std::string message; };
struct ForbiddenError     { std::string message; };
struct NotFoundError      { std::string resource; std::string id; };
struct ConflictError      { std::string message; };
struct UnprocessableEntityError { std::string message; std::vector<FieldError> fields; };
struct PayloadTooLargeError { std::size_t limit; };
struct MethodNotAllowedError { std::vector<HttpMethod> allowed; };

struct JsonParseError      { std::string detail; };
struct FormParseError      { std::string detail; };
struct MultipartParseError { std::string detail; };
struct BodyLimitExceeded   { std::size_t limit; };

// ADL conversions — paired with each type, easy to grep, easy to override per-domain.
Response to_http_response(const BadRequestError&);
Response to_http_response(const UnauthorizedError&);
Response to_http_response(const ForbiddenError&);
Response to_http_response(const NotFoundError&);
Response to_http_response(const ConflictError&);
Response to_http_response(const UnprocessableEntityError&);
Response to_http_response(const PayloadTooLargeError&);
Response to_http_response(const MethodNotAllowedError&);
Response to_http_response(const JsonParseError&);
Response to_http_response(const FormParseError&);
Response to_http_response(const MultipartParseError&);
Response to_http_response(const BodyLimitExceeded&);

}  // namespace demiplane::http
```

User-defined error types follow the same pattern. If a handler's `Outcome<Response, Errors...>` lists an error type
without a `to_http_response` overload, the controller bind layer fails to instantiate — compile error pointing at the
missing overload.

`to_http_response(const E&)` is intentionally **arena-free**: error responses (4xx/5xx) are the cold path, so they
construct on the global heap via the static `ResponseFactory` (§5.7). This keeps the extension point a clean
one-argument free function — no allocator threading through every user override. The zero-additional-allocation
invariant (§11) covers the **hot path** (2xx via `ctx.json(...)` etc.); the rare error path is the documented exception.

### 5.6 `AsyncOutcome` alias

```cpp
template <typename T, typename... Es>
using AsyncOutcome = boost::asio::awaitable<gears::Outcome<T, Es...>>;
```

Lives in `types/async_outcome.hpp`.

### 5.7 `ResponseFactory` (ctx-less / cold path)

The **hot-path factory is on `RequestContext`** (§5.4): `ctx.json(...)`, `ctx.ok(...)`, etc. thread the request arena
into the response automatically, so you cannot accidentally build a success response on the global heap. A *static*
`ResponseFactory` remains only for the two contexts that have no `ctx`: `to_http_response(const E&)` error conversions (
§5.5) and library/test code constructing a synthetic response. These build on the global heap — the cold path, accepted
per §11.

```cpp
class ResponseFactory {   // global-heap; cold path only (errors, tests, synthetic)
public:
    static Response ok          (std::string body = "", std::string_view ct = "text/plain");
    static Response json        (std::string body);
    static Response created     (std::string body = "", std::string_view ct = "application/json");
    static Response no_content();
    static Response redirect    (std::string_view location, HttpStatus = HttpStatus::found);

    static Response not_found   (std::string body = "Not Found");
    static Response bad_request (std::string body = "Bad Request");
    static Response unauthorized(std::string body = "Unauthorized");
    static Response forbidden   (std::string body = "Forbidden");
    static Response method_not_allowed(std::span<const HttpMethod> allow);
    static Response internal_error(std::string body = "Internal Server Error");

    static Response custom(HttpStatus, std::string body,
                           std::string_view ct = "text/plain");
};
```

Both paths converge on the same `Response` shape and yield identical wire output for a given status/body; the only
difference is which allocator the headers/body land in (arena vs global heap). Neither sets `Server`/`Date` headers —
drivers stamp those uniformly right before write, so there is no per-construction-path helper to forget. (Streaming
responses are built via `ctx.stream(...)`, which needs the arena and so lives on `RequestContext`, not here.)

## 6. Connection + Driver Interface

### 6.1 IsConnection — concept, not class hierarchy

```cpp
namespace demiplane::http {

class RequestArena {
    // One heap block per CONNECTION (ServerConfig::request_arena_size, default
    // 8KB), reused across every keep-alive request on the connection —
    // amortized, not per-request. (A fixed std::array member could not honor
    // the runtime config value.)
    std::unique_ptr<std::byte[]> initial_block_;
    std::pmr::monotonic_buffer_resource resource_;
public:
    explicit RequestArena(std::size_t size = 8192)
        : initial_block_{std::make_unique<std::byte[]>(size)},
          resource_{initial_block_.get(), size} {}
    std::pmr::polymorphic_allocator<> allocator() { return {&resource_}; }
    void reset() { resource_.release(); }   // rewinds to the initial block
};

template <typename T>
concept IsConnection = requires (T& t, std::chrono::milliseconds ms) {
    { t.arena_alloc() }          -> std::same_as<std::pmr::polymorphic_allocator<>>;
    { t.reset_request_arena() }  -> std::same_as<void>;
    { t.expires_after(ms) }      -> std::same_as<void>;
    { t.async_close() }          -> std::same_as<asio::awaitable<void>>;
    { t.cancel_slot() }          -> std::same_as<asio::cancellation_slot>;
    { t.remote_address() }       -> std::same_as<asio::ip::address>;
    { t.negotiated_protocol() }  -> std::same_as<Protocol>;
    { t.is_secure() }            -> std::same_as<bool>;
};

template <typename T>
concept IsStreamConnection = IsConnection<T> && requires (T& t) {
    typename T::stream_type;
    { t.stream() } -> std::same_as<typename T::stream_type&>;
};

}  // namespace demiplane::http
```

Concrete connection types are value classes that compose a `RequestArena` and any per-transport state:

- `TcpConnection` — `asio::ip::tcp::socket` + arena + cancel signal.
- `TlsConnection` — `asio::ssl::stream<tcp::socket>` + arena + cancel signal + ALPN result.
- `QuicConnection` — scaffold only; ngtcp2 state when filled in.

### 6.2 IsHttpDriver — concept with templated `serve()`

```cpp
template <typename T>
concept IsHttpDriver = requires {
    { T::id() }             -> std::same_as<Protocol>;
    { T::accepted_alpns() } -> std::same_as<std::span<const std::string_view>>;
    // serve() is templated on connection type — checked at the listener level.
};
```

Each driver class has a templated `serve()` method. The connection type is deduced at the call site (in the listener),
so duck typing on `.stream()` chooses Beast's stream type at compile time — no `dynamic_cast`, no per-byte vcall.

### 6.3 `Http11Driver`

```cpp
struct Http11Config {
    std::size_t max_header_bytes = 16 * 1024;
    std::size_t max_body_bytes   = 16 * 1024 * 1024;
    std::chrono::milliseconds header_timeout = std::chrono::seconds{10};
    std::chrono::milliseconds body_timeout   = std::chrono::seconds{30};
    std::chrono::milliseconds idle_timeout   = std::chrono::seconds{60};
};

class Http11Driver {
public:
    explicit Http11Driver(Http11Config cfg);

    static constexpr Protocol id() { return Protocol::http1; }
    static constexpr std::span<const std::string_view> accepted_alpns() {
        static constexpr std::string_view kAlpns[] = {"http/1.1"};
        return kAlpns;
    }

    template <IsStreamConnection ConnT>
    asio::awaitable<void> serve(ConnT& conn, Router& router) {
        auto& stream = conn.stream();   // typed via template deduction
        beast::flat_buffer buffer;

        while (true) {
            beast::get_lowest_layer(stream).expires_after(cfg_.header_timeout);
            conn.reset_request_arena();
            std::pmr::polymorphic_allocator<char> alloc{conn.arena_alloc().resource()};

            // Beast's allocator points at the request arena.
            beast::http::request_parser<beast::http::buffer_body, decltype(alloc)>
                parser{std::piecewise_construct, std::tuple{}, std::tuple{alloc}};
            parser.header_limit(cfg_.max_header_bytes);
            parser.body_limit(cfg_.max_body_bytes);

            beast::error_code ec;
            co_await beast::http::async_read_header(
                stream, buffer, parser,
                asio::bind_cancellation_slot(conn.cancel_slot(),
                    asio::redirect_error(asio::use_awaitable, ec)));

            if (ec == beast::http::error::end_of_stream) break;
            if (ec) co_return;

            beast::get_lowest_layer(stream).expires_after(cfg_.body_timeout);
            auto ctx = build_request_context(parser, conn);

            Response response;
            try {
                response = co_await router.dispatch(std::move(ctx));
            } catch (...) {
                response = ResponseFactory::internal_error("Internal Server Error");
            }
            stamp_common_headers(response);
            const bool keep_alive = response.keep_alive;   // read BEFORE the move
            co_await write_response(stream, std::move(response), conn.cancel_slot());

            if (!keep_alive) break;
        }
        co_await conn.async_close();
    }

private:
    Http11Config cfg_;
};
```

Key bug-fixes that land here:

- **Body limit + per-phase timeouts** real (`parser.body_limit`, `expires_after`).
- **URL decoding**: query parameters decode lazily in `RequestContext`; path *parameter captures* decode at route
  match (§8.6). The path itself stays raw — `%2F` can never become a separator.
- **Handler exceptions → 500** via the catch-all around `router.dispatch`. Outcome-typed errors are already converted in
  `router.dispatch` — the catch-all handles only escapes that bypassed the Outcome system.
- **`Date` and `Server` headers** stamped in `stamp_common_headers` — one place, every protocol driver does it
  identically.
- **Cancellation-aware I/O** via `bind_cancellation_slot` — graceful shutdown actually unwinds.

### 6.4 Http2Driver / Http3Driver — scaffolds

```cpp
class Http2Driver {
public:
    static constexpr Protocol id() { return Protocol::http2; }
    static constexpr std::span<const std::string_view> accepted_alpns() {
        static constexpr std::string_view kAlpns[] = {"h2"};
        return kAlpns;
    }

    template <IsStreamConnection ConnT>
        requires requires(ConnT c) { { c.is_secure() } -> std::same_as<bool>; }
    asio::awaitable<void> serve(ConnT& conn, Router& router) {
        // Scaffold — fill in with nghttp2.
        COMPONENT_LOG_WRN() << "Http2Driver::serve() not implemented";
        co_await conn.async_close();
    }
};

class Http3Driver { /* mirror, takes QuicConnection */ };
```

vcpkg manifest pulls `nghttp2`, `ngtcp2`, `nghttp3` so headers are available. When the implementations land later, they
go inside `serve()` with no surrounding changes.

## 7. Listeners, TLS, ALPN

### 7.1 ListenerBase

```cpp
class ListenerBase {
public:
    virtual ~ListenerBase() = default;

    virtual void bind() = 0;       // sync; throws on failure
    virtual asio::awaitable<void> run(Router& router) = 0;   // accept loop until cancelled
    virtual asio::awaitable<void> drain_until(
        std::chrono::steady_clock::time_point deadline) = 0;

    virtual std::string bind_address() const = 0;
    virtual std::uint16_t bound_port() const = 0;   // for tests on :0
};
```

### 7.2 Connection tracking

```cpp
class ConnectionTracker {
    std::atomic<std::size_t> in_flight_{0};
    std::list<asio::cancellation_signal> per_conn_signals_;
    std::mutex signals_mu_;

public:
    struct Handle {
        ConnectionTracker* tracker;
        std::list<asio::cancellation_signal>::iterator slot_it;
        ~Handle();   // RAII: erase from list, decrement counter, notify
        asio::cancellation_slot slot();
    };

    Handle register_connection();

    asio::awaitable<void> drain_until(std::chrono::steady_clock::time_point deadline);
    // Polls the atomic; on deadline expiration, force-emits cancellation on
    // every remaining per-conn signal.
};
```

**Cancellation + threading.** `asio::cancellation_signal::emit` is not thread-safe against concurrent operations on the
connection it targets, and a `cancellation_slot` holds **at most one handler**. Consequences: (a) when the injected
executor is multi-threaded, every accepted connection is served on its own strand —
`co_spawn(asio::make_strand(exec), driver.serve(conn, router), ...)` — and the tracker's force-cancel `asio::dispatch`es
each `emit` onto that connection's strand (the `Handle` records the executor); (b) shutdown uses one signal *per
listener* (§9.3) — a single shared signal would cancel only the last-spawned accept loop.

### 7.3 TcpListener / TlsListener / QuicListener

`TcpListener<Driver>` — single driver, no ALPN. `bind()` opens an acceptor, `run()` accept loop, each connection gets a
tracker handle and a `TcpConnection`, calls `driver.serve(conn, router)`.

`TlsListener<Drivers...>` — tuple of drivers, ALPN selects at handshake. OpenSSL's `SSL_CTX_set_alpn_select_cb` picks
from the union of advertised ALPN strings (accumulated across the driver tuple at compile time). After handshake,
`dispatch_alpn` walks the tuple and calls the matching driver. Server preference order = driver template param order ("
first listed wins on tie").

`QuicListener<Http3Driver>` — scaffold. `bind()` is a stub that succeeds, `run()` returns immediately. Constructor
compile-time-asserts that the driver is `Http3Driver` (QUIC pairs with h3 only).

### 7.4 build_ssl_context

A helper in `listeners/tls_listener/build_ssl_context.cpp` that takes a `TlsConfig` and the union of advertised ALPN
strings, returns a fully configured `asio::ssl::context`:

1. Construct with `tls_server` method.
2. Set min protocol version per `TlsConfig::min_version`.
3. Disable bad protocols/ciphers (
   `SSL_OP_NO_SSLv2 | NO_SSLv3 | NO_TLSv1 | NO_TLSv1_1 | NO_COMPRESSION | SINGLE_DH_USE`).
4. Set a hardened cipher list (modern defaults; configurable later).
5. Load cert chain + private key (with passphrase callback if set).
6. Optional DH params, optional client cert verification.
7. Configure session cache.
8. Set ALPN advertise + selection callback that walks client offers and picks the first server-advertised protocol.

## 8. Routing

### 8.1 Three-phase lifecycle

1. **Build** — controllers constructed, `configure_routes()` populates local registries, middleware lists filled.
2. **Bake** — `server.add_controller(...)` (or `in_group(...).add_controller(...)`) drains the controller's local
   registry into the server's, applying prefix + composing middleware chain + wiring Outcome→Response. Conflicts
   detected here, fail at setup if present.
3. **Frozen** — after `server.setup()`. All registries immutable. `find_route()` is the only operation. Any registration
   attempt is `std::logic_error`.

### 8.2 HttpController

```cpp
class HttpController : public std::enable_shared_from_this<HttpController> {
public:
    NEXUS_REGISTER(nexus::Resettable);

    virtual ~HttpController() = default;
    // Controllers are RAII (ctor acquires, dtor releases) — no framework
    // lifecycle hooks (initialize()/shutdown() removed 2026-07-06; async
    // shutdown-time work belongs in ServerObserver::on_shutdown_started).
    virtual void configure_routes() = 0;

    template <typename Mw>
    HttpController& add_middleware(Mw&& mw);

protected:
    // Verb DSL — async-only handlers, RequestContext by value, two return shapes.
    // Drops the current code's 12-overloads-per-verb (sync x async x value x const-ref x ...)
    // down to four: member-fn x {AsyncResponse, AsyncOutcome<Response, Es...>}, plus
    // the free-function variant for each.

    // Member fn, no typed errors
    template <typename Controller>
    void Get(std::string path, AsyncResponse (Controller::*method)(RequestContext));

    // Member fn, typed errors via Outcome
    template <typename Controller, typename... Errors>
    void Get(std::string path, AsyncOutcome<Response, Errors...> (Controller::*method)(RequestContext));

    // Free function / lambda
    template <typename F> requires IsRouteHandler<F>
    void Get(std::string path, F&& handler);

    // Same for Post, Put, Patch, Delete, Head, Options.

private:
    SCROLL_COMPONENT_PREFIX("HttpController");

    struct LocalRoute {
        HttpMethod method;
        std::string path;
        std::function<ContextHandler(std::shared_ptr<HttpController>,
                                     std::span<const Middleware>,
                                     std::string_view prefix)> bake;
    };

    std::vector<LocalRoute> local_routes_;
    std::vector<Middleware> middlewares_;

    friend class detail::ControllerBaker;
};
```

The verb-DSL templates capture the typed handler signature (which knows its `Outcome` error types) and produce a `bake`
closure. At merge time, the closure runs and produces the final `ContextHandler` with Outcome→Response baked in via ADL
`to_http_response`.

### 8.3 Bake step (Outcome→Response wiring + middleware composition)

For a member function returning `AsyncOutcome<Response, Errors...>`, the bake closure produces:

```cpp
ContextHandler inner = [self, method](RequestContext ctx) -> AsyncResponse {
    auto outcome = co_await (self.get()->*method)(std::move(ctx));
    co_return std::move(outcome).visit(
        [](Response&& r) -> Response { return std::move(r); },
        [](auto&& err)   -> Response {
            return to_http_response(std::forward<decltype(err)>(err));
        });
};

// Wrap with middleware right-to-left.
for (auto it = middlewares.rbegin(); it != middlewares.rend(); ++it) {
    ContextHandler next = std::move(inner);
    inner = [mw = *it, next = std::move(next)](RequestContext ctx) -> AsyncResponse {
        co_return co_await mw(std::move(ctx), next);
    };
}
return inner;
```

For each `E` in the `Errors...` pack, ADL must find `to_http_response(const E&)`. Missing overload = compile error
pointing at the offending error type.

The `AsyncResponse` (no Outcome) variant is the same loop without the visit.

Middleware shape:

```cpp
using NextHandler = std::function<AsyncResponse(RequestContext)>;
using Middleware  = std::function<AsyncResponse(RequestContext, const NextHandler&)>;
```

Middleware can short-circuit (return without calling `next`), modify the context (`ctx.set<T>(...)` then call `next`),
or wrap the response (`auto r = co_await next(ctx); modify(r); co_return r;`).

`next` is passed by **reference**, never by value: the composed `NextHandler` is a capturing `std::function`, and
copying it at every layer on every request would heap-allocate per layer (silently breaking §11). The referenced object
lives in the frozen registry's baked chain — closures own their captures and outlive any request — so the reference,
including the one held by a middleware's suspended coroutine frame, is always valid.

### 8.4 Group binding

```cpp
class GroupBinding {
public:
    template <std::derived_from<HttpController> C>
    GroupBinding& add_controller(std::shared_ptr<C> ctrl);   // bakes + merges

    GroupBinding in_group(std::string sub_prefix);   // nested groups via combined prefix
};

class Server {
    /* ... */
    template <std::derived_from<HttpController> C>
    Server& add_controller(std::shared_ptr<C> ctrl) {
        return in_group("").add_controller(std::move(ctrl));
    }

    GroupBinding in_group(std::string prefix);
};
```

User wiring:

```cpp
auto users = std::make_shared<UserController>(user_service);
add_basic_middleware(*users, log_mw, request_id_mw, tracer_mw);   // free helper
users->add_middleware(json_only_mw);
server.in_group("/api/v1").add_controller(users);
```

### 8.5 RouteRegistry storage and lookup

```cpp
class RouteRegistry {
public:
    void add_route(HttpMethod method, std::string path, ContextHandler handler);
    std::vector<RouteConflictError> freeze();   // returns conflicts; empty = OK
    bool is_frozen() const;

    gears::Outcome<ResolvedRoute, NotFoundError, MethodNotAllowedError>
        find_route(HttpMethod method, std::string_view path,
                   std::pmr::polymorphic_allocator<> arena_alloc) const;

private:
    bool frozen_ = false;

    boost::unordered::unordered_flat_map<std::string,
        std::array<ContextHandler, num_methods>> exact_;

    struct ParamTemplate {
        std::vector<PathSegment> segments;
        std::vector<std::string> param_names;
        std::array<ContextHandler, num_methods> by_method;
    };
    std::vector<ParamTemplate> parametric_;
};
```

**No `std::regex`.** Parametric paths parse at registration into a vector of segments (literal or parameter). At lookup,
split incoming path by `/` and walk both lists. URL-decode captured segments into the request arena (one bump per
param).

**404 vs 405** — `find_route` distinguishes based on whether any verb exists at the matched path:

```cpp
gears::Outcome<ResolvedRoute, NotFoundError, MethodNotAllowedError>
RouteRegistry::find_route(HttpMethod method, std::string_view path,
                          std::pmr::polymorphic_allocator<> alloc) const {
    auto normalized = normalize_path(path);

    if (auto it = exact_.find(normalized); it != exact_.end()) {
        if (auto& h = it->second[std::to_underlying(method)]; h) {
            return ResolvedRoute{h, /* no path params */ {}};
        }
        return gears::err(MethodNotAllowedError{allowed_methods_for(it->second)});
    }

    for (const auto& tmpl : parametric_) {
        auto match = match_segments(tmpl, normalized, alloc);
        if (!match) continue;
        if (auto& h = tmpl.by_method[std::to_underlying(method)]; h) {
            return ResolvedRoute{h, std::move(*match)};
        }
        return gears::err(MethodNotAllowedError{allowed_methods_for(tmpl.by_method)});
    }

    return gears::err(NotFoundError{"route", std::string{normalized}});
}
```

`MethodNotAllowedError` carries the allowed-verb set; its `to_http_response` produces a 405 with the `Allow:` header
populated.

Per-request hygiene (the §11 invariant depends on all four):

- `exact_` uses a **transparent (heterogeneous) hash/equality** so lookup takes the incoming `string_view` directly — no
  per-lookup `std::string` construction.
- `ResolvedRoute` carries a **pointer to the stored `ContextHandler`**, never a copy (copying a `std::function` per
  request can heap-allocate).
- `normalize_path` is allocation-free: it returns the input view unchanged when already normal (the common case) and
  otherwise rewrites into the request arena.
- `num_methods` counts the `HttpMethod` enum *including* `unknown`. `add_route(HttpMethod::unknown, ...)` throws at
  registration, so an unrecognized incoming verb (Beast `verb::unknown`) can never match a slot — it falls out as 405
  with the path's `Allow` set (or 404 on an unknown path).

### 8.6 Path normalization

Default policy: collapse trailing slash internally (`/users/` and `/users` are the same route), collapse multi-slash (
`/users//42` → `/users/42`). Case preserved (RFC 3986).

Configurable via `ServerConfig::path_normalization`:

- `none` — exact match (original current-code behavior).
- `collapse_trailing_slash` — default.
- `collapse_multi_slash` — extends trailing-slash collapse with multi-slash collapse.

Applied at:

1. Controller-side `Get/Post/...` registration.
2. Group prefix concatenation.
3. `find_route()` lookup on incoming target.

**Decoding policy (split before decode).** Lookup splits the *raw* path on `/` first, so an encoded `%2F` can never
create or destroy a segment boundary. Literal segments compare raw bytes (zero decode work on the common no-`%` path);
to keep that asymmetry harmless, registration rejects `%` inside literal route segments. Captured parameter segments are
percent-decoded into the request arena with `plus_is_space = false` — `+` is a literal in paths, unlike in query
strings, which decode with `plus_is_space = true` in `RequestContext`. `RequestContext::path()` returns the raw,
undecoded path view.

### 8.7 Conflict detection

`RouteRegistry::add_route` checks before insert; on duplicate `(method, path)` builds a `RouteConflictError`. `freeze()`
aggregates and returns them. `Server::setup()` throws a `RouteConflictAggregateError` containing all conflicts at once.
Misconfigurations are visible immediately, not piecemeal.

### 8.8 Router

Thin facade over RouteRegistry that drivers call:

```cpp
class Router {
public:
    explicit Router(RouteRegistry& registry);

    asio::awaitable<Response> dispatch(RequestContext ctx) const {
        auto path  = ctx.path();
        auto verb  = ctx.method();
        auto alloc = ctx.arena_alloc();

        auto resolved = registry_.find_route(verb, path, alloc);
        if (!resolved) {
            co_return std::move(resolved).visit(
                [](ResolvedRoute&&) -> Response { std::unreachable(); },
                [](auto&& err) -> Response {
                    return to_http_response(std::forward<decltype(err)>(err));
                });
        }

        ctx.set_path_params(std::move(resolved.value().path_params));
        co_return co_await resolved.value().handler(std::move(ctx));
    }

private:
    RouteRegistry& registry_;
};
```

Drivers see `Response` only — both routing-level errors and handler typed errors are already mapped here. The
driver-level catch-all is for unexpected exceptions only.

## 9. Server Orchestration + Lifecycle

### 9.1 Server class

```cpp
class Server {
public:
    NEXUS_REGISTER(nexus::Immortal);

    // Executor injection is the primary model. The caller owns the
    // io_context(s) and the threads that run them; the Server runs its accept
    // loops and shutdown coroutines ON `exec` and NEVER creates, runs, or stops
    // an executor. This lets HTTP share a process with a logger, DB pool, S3
    // client, etc., each pinned to whatever executor/threads the caller chose.
    Server(ServerConfig cfg, asio::any_io_executor exec);

    ~Server();   // RAII backstop: if still active, logs WRN and runs
                 // stop() + wait_until_stopped() itself (§9.6). Requires a
                 // driven executor and an off-executor destruction site.

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // (host, port) split matches the landed listener ctors (PR 5 D7); the
    // config-driven path is attach_default_listeners (PR 6, landed).
    template <IsHttpDriver Driver>
    Server& add_tcp_listener(std::string host, std::uint16_t port, Driver driver);

    template <IsHttpDriver... Drivers>
    Server& add_tls_listener(std::string host, std::uint16_t port, TlsConfig tls, Drivers... drivers);

    template <IsHttpDriver Driver>
    Server& add_quic_listener(std::string host, std::uint16_t port, TlsConfig tls, Driver driver);

    template <std::derived_from<HttpController> C>
    Server& add_controller(std::shared_ptr<C> ctrl);

    GroupBinding in_group(std::string prefix);

    Server& add_observer(std::shared_ptr<ServerObserver> obs);

    void setup();   // freeze registry + bind ALL listeners (failures aggregated
                    // into ListenerBindError; bound acceptors released) + AWAIT
                    // the on_setup_complete observer barrier + co_spawn accept
                    // loops on exec_. Spawns no threads; blocks ONLY on the
                    // barrier — with observers, the executor must already be
                    // driven and setup() must run off-executor (§9.3).
    void stop();    // request graceful shutdown; non-blocking, idempotent,
                    // callable from executor threads. Latched if racing setup()
                    // (§9.4). Runs drain + observers on exec_. NEVER stops the
                    // executor.

    // Block the calling thread until graceful shutdown has fully completed.
    // CONTRACT (§9.7): the caller MUST keep running the injected executor until
    // this returns, and only THEN tear the executor down. The awaitable form is
    // for callers already running on exec_.
    void                  wait_until_stopped();
    asio::awaitable<void> async_wait_stopped();

    bool is_running() const noexcept;
    std::span<const std::unique_ptr<ListenerBase>> listeners() const;
    const ServerConfig& config() const noexcept;

private:
    // `starting` covers setup()'s live window (observer barrier + loop spawn):
    // a stop() arriving there LATCHES (stop_requested_) and is honored at
    // setup()'s tail instead of being silently lost.
    enum class State : std::uint8_t { build, starting, running, stopping, stopped };
    std::atomic<State> state_{State::build};
    std::atomic<bool> stop_requested_{false};

    ServerConfig cfg_;
    asio::any_io_executor exec_;   // injected; NOT owned, never stopped

    std::vector<std::unique_ptr<ListenerBase>> listeners_;
    std::vector<std::shared_ptr<HttpController>> controllers_;
    std::vector<std::shared_ptr<ServerObserver>> observers_;

    RouteRegistry registry_;
    Router router_{registry_};

    // One stop signal PER listener: a cancellation_slot holds at most one
    // handler, so a single shared signal would cancel only the last-spawned
    // accept loop. cancellation_signal is immovable; shared_ptr because the
    // phase-1 emit lambdas are queued in io_context-owned strand queues and
    // can outlive the Server — each keeps its signal alive (a late emit fires
    // on a live-but-orphaned signal, harmlessly).
    std::vector<std::shared_ptr<asio::cancellation_signal>> listener_stop_signals_;
    std::mutex shutdown_mutex_;
    std::condition_variable shutdown_cv_;
    bool shutdown_complete_ = false;

    asio::awaitable<void> graceful_shutdown();
    SCROLL_COMPONENT_PREFIX("Server");
};

// Convenience for the "HTTP owns the process" case. Creates an internal
// io_context + `threads` worker threads, calls `configure(server)` for
// listener/controller/observer wiring, installs SIGINT/SIGTERM -> stop(),
// blocks until graceful shutdown completes, then joins threads and stops the
// context. Exactly equivalent to wiring an executor by hand and obeying the
// §9.7 shutdown-ordering contract — provided so trivial apps need not.
void run_standalone(ServerConfig cfg, std::size_t threads,
                    const std::function<void(Server&)>& configure);
```

### 9.2 ServerObserver

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

Per-request hooks are wired by `Server::setup()` into the `Router` as plain nullable `std::function`s (PR 5 D2) —
the routing layer carries no server-layer dependency and no observer inheritance. `Router::dispatch` fires
`on_request` at entry, `on_response` for handler successes and routing-miss 404/405s, and
`on_unhandled_exception` (then **rethrows**) for handler escapes — so the driver-synthesized 500 and driver-level
early responses (malformed 400, limit 4xx) are not observed. Hooks may fire concurrently from any executor thread;
implementations must be thread-safe and allocation-free on the hot path.

### 9.3 setup()

1. Validate state is `build`; validate at least one listener. (Thread count is the caller's concern — the Server
   owns no threads.)
2. `registry_.freeze()` → if conflicts, throw `RouteConflictAggregateError` (all conflicts in one throw).
3. Call `bind()` on **every** listener (best-effort-all, so the operator sees every bad endpoint at once); on any
   failure clear the listener set (RAII closes the acceptors that DID bind — no LISTENING socket is left with no
   accept loop) and throw `ListenerBindError` aggregating all failures. Controllers need no symmetric unwind —
   they are RAII.
4. Wire the observer fan-out hooks into the Router (D2; skipped when no observers).
5. Set `state_` to `starting` — a `stop()` arriving from here on latches `stop_requested_` instead of no-oping.
6. **Await** the observers' `on_setup_complete()` as a **barrier** on `exec_` (sequential, add order; per-observer
   throws fanned to `on_unhandled_exception`): observer-owned resources exist before the first request is served.
   With observers registered, `setup()` therefore **blocks** and the executor must already be driven by another
   thread (revised 2026-07-06; connections arriving meanwhile queue in the kernel backlog — `bind()` already
   `listen()`s — delayed, never lost).
7. `co_spawn` each listener's `run(router_)` on **its own strand** of `exec_` (D5 — the stop emit must be
   serialized with the loop's turns), each bound to **its own** `listener_stop_signals_[i]->slot()` — a slot holds
   a single handler, so the signal cannot be shared across listeners (§7.2).
8. Set `state_` to `running`; if `stop_requested_` was latched, invoke `stop()` now (no lost stops).

No work guard, no worker threads. The only blocking point is the step-6 observer barrier.

### 9.4 stop()

Idempotent, thread-safe, callable from executor threads (signal handlers, request handlers). CAS `running` →
`stopping` → `co_spawn(graceful_shutdown(), detached)` on `exec_`. During `starting` it latches `stop_requested_`
(honored at setup()'s tail); in `build`/`stopping`/`stopped` it is a no-op. Returns immediately. **Never stops the
executor** — it may be shared with other subsystems that must outlive HTTP (§9.7).

Caveat (2026-07-06, TSan-found): the executor must outlive the `stop()` *call* itself — `stop()` posts into it, and
the post's scheduler-signal tail races an executor teardown that begins the instant shutdown completes. Irrelevant in
injected mode (the §9.7 orchestrator owns both); in `run_standalone` mode it means external threads must not call
`stop()` — use SIGINT/SIGTERM or call it from within a handler/observer.

### 9.5 graceful_shutdown()

```cpp
asio::awaitable<void> Server::graceful_shutdown() {
    // Phase 1: cancel accept loops — one signal per listener (§9.3), each
    // emit DISPATCHED onto the strand its run() executes on (D5). The lambda
    // is queued in an io_context-owned strand queue and can outlive the
    // Server, so it owns the signal via shared_ptr.
    for (std::size_t i = 0; i < listeners_.size(); ++i)
        asio::dispatch(run_strands_[i],
                       [sig = listener_stop_signals_[i]] { sig->emit(asio::cancellation_type::terminal); });

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

    // Phase 3: notify async shutdown observers. These run on exec_ — the caller
    // MUST still be driving it (§9.7), so observers can do real async work
    // (flush a buffer, close a pool) before completion is reported.
    for (auto& obs : observers_) {
        try {
            co_await obs->on_shutdown_started();
        } catch (...) {
            for (auto& other : observers_)
                other->on_unhandled_exception(std::current_exception());
        }
    }

    // Phase 4: notify shutdown_complete observers (sync, noexcept).
    // (Controllers have no shutdown hook — they are RAII and unwind with the
    // Server; removed 2026-07-06.)
    for (auto& obs : observers_) obs->on_shutdown_complete();

    // Phase 5: report completion. We do NOT stop or drain the executor — the
    // caller owns it. wait_until_stopped() / async_wait_stopped() unblock here.
    // Latch + notify UNDER the lock: a §9.7-compliant caller may destroy the
    // Server the moment it observes shutdown_complete_ — a caller reaching
    // wait_until_stopped() between an unlocked latch and the notify would sail
    // through the predicate and free the cv under us. With the notify inside
    // the critical section, this block is the coroutine's LAST touch of *this*.
    {
        std::lock_guard lk{shutdown_mutex_};
        shutdown_complete_ = true;
        state_.store(State::stopped);
        shutdown_cv_.notify_all();
    }
}
```

Phases 1.5–4 run inside a catch-all: an exception escaping any of them is fanned to `on_unhandled_exception` and the
coroutine still falls through to phase 5 — a detached shutdown that dies early would otherwise strand
`wait_until_stopped()` forever.

The phase ordering keeps the original review's C2 fix (async stop callbacks no longer dropped): observers run while the
caller's threads still drive `exec_`, and only after they finish is completion reported.

### 9.6 ~Server()

RAII backstop (revised 2026-07-06): if the Server is destroyed while active (`starting`/`running`/`stopping`), the
destructor logs a WRN and runs `stop()` + `wait_until_stopped()` itself. Rationale: detached coroutine frames (accept
loops, in-flight `serve()`s, a spawned `graceful_shutdown`) live in the *caller's* executor queues and reference the
Server's members — no destructor can recall them, only completion unwinds them, so the destructor must either wait for
that completion or free members under live frames (use-after-free). The typical `main()` declares the executor before
the Server, so the Server dies first while the executor is still driven and this shuts down cleanly. If the executor is
not driven — or the destructor runs ON an executor thread — this blocks forever, loudly, which is strictly better than
heap corruption. A correct caller still runs the §9.7 sequence itself (`run_standalone` does it for you); the
destructor path is defense for everyone else.

### 9.7 Shutdown-ordering contract (injected mode)

The inversion moves one responsibility onto the caller, and it is the single easiest thing to get wrong:

> **The drain and the async `on_shutdown_started` observers run *on the injected executor*. After calling `stop()`, the
caller must keep running that executor until `wait_until_stopped()` / `async_wait_stopped()` returns, and only THEN
stop/destroy the executor.**

- Stop the executor too early → drain and observer coroutines are killed mid-flight (truncated responses, un-flushed
  work).
- The Server **never** stops the executor itself — precisely because the executor is typically shared with the logger /
  DB pool / S3 client, which must outlive HTTP's shutdown.

The canonical caller sequence (injected mode):

```cpp
server.setup();                 // accept loops live on the caller's executor
// ... caller's threads run exec / io_context; SIGINT handler calls server.stop() ...
server.wait_until_stopped();    // returns only after graceful_shutdown completes
ioc.stop();                     // NOW it is safe to tear the executor down
for (auto& t : my_threads) t.join();
```

Two corollaries: `wait_until_stopped()` must be called from a thread that is **not** driving `exec_` — on an executor
thread it would block the very shutdown it waits for (use `async_wait_stopped()` there); and `stop()` before `setup()`
is a documented no-op.

`run_standalone` exists so trivial apps never reason about this — it owns the context and threads and performs the
stop → wait → stop-context → join sequence internally.

## 10. Configuration

All config types inherit `serialization::ConfigInterface<Self, Json::Value>`, declare fields via static `fields()`
tuple, ship a fluent `Builder`, use a private default constructor for the framework. Same shape as the project's
existing `FileSinkConfig`. Validation via `validate()`. Errors surface through Outcome at load time.

### 10.1 Config types

**Landed shape (PR 6):** the sketches below predate implementation; the landed code deviates in a few
recorded ways. `port` is `std::uint16_t` (not `int`). `ServerConfig` and `ListenerConfig` are Builder-only;
`TlsConfig` keeps a public no-validation full-ctor escape hatch (scaffold unit tests deliberately build empty
configs); `Timeouts` keeps its public 3-arg ctor (D6). `tls.key_passphrase` is `FieldPolicy::Secret` — read from
JSON, never emitted by `dump_server_config` (D3). Every enum field (`ListenerConfig::Transport`, `Protocol`,
`TlsConfig::MinVersion`, `ServerConfig::PathNormalization`) is string-encoded via non-template ADL
`read_field`/`write_field` overloads declared next to the enum (`"tcp"|"tls"|"quic"`,
`"http1"|"http2"|"http3"`, `"tls12"|"tls13"`, `"none"|"collapse_trailing_slash"|"collapse_multi_slash"`);
unknown strings throw `std::invalid_argument` naming the field (D2) — supersedes this section's original
int-encoding allowance. The `Protocol` codec lives in `listener_config.hpp` (config layer), not
`http_enums.hpp`, so the types layer stays free of a serialization/jsoncpp dependency. `ListenerConfig` gained
`effective_protocols()` (empty `protocols` ⇒ the transport's default: `http1`, or `http3` on `quic`).
`ServerConfig::drain_timeout` keeps the JSON field name `drain_timeout_ms`.

```cpp
class Timeouts final : public serialization::ConfigInterface<Timeouts, Json::Value> {
public:
    constexpr Timeouts(std::chrono::milliseconds header,
                       std::chrono::milliseconds body,
                       std::chrono::milliseconds idle) noexcept;

    constexpr void validate() const override;

    [[nodiscard]] constexpr auto header() const noexcept;
    [[nodiscard]] constexpr auto body()   const noexcept;
    [[nodiscard]] constexpr auto idle()   const noexcept;

    static constexpr auto fields() {
        return std::tuple{
            serialization::Field<&Timeouts::header_, "header_ms">{},
            serialization::Field<&Timeouts::body_,   "body_ms">{},
            serialization::Field<&Timeouts::idle_,   "idle_ms">{},
        };
    }

    class Builder;

private:
    friend class ConfigInterface;
    constexpr Timeouts() = default;

    std::chrono::milliseconds header_ = std::chrono::seconds{10};
    std::chrono::milliseconds body_   = std::chrono::seconds{30};
    std::chrono::milliseconds idle_   = std::chrono::seconds{60};
};

class TlsConfig final : public serialization::ConfigInterface<TlsConfig, Json::Value> {
public:
    enum class MinVersion : std::uint8_t { tls12, tls13 };

    constexpr void validate() const override;

    [[nodiscard]] /* ... accessors ... */;

    static constexpr auto fields() {
        return std::tuple{
            serialization::Field<&TlsConfig::cert_file_,            "cert_file">{},
            serialization::Field<&TlsConfig::key_file_,             "key_file">{},
            serialization::Field<&TlsConfig::key_passphrase_,       "key_passphrase">{},
            serialization::Field<&TlsConfig::dh_params_file_,       "dh_params_file">{},
            serialization::Field<&TlsConfig::ca_file_,              "ca_file">{},
            serialization::Field<&TlsConfig::min_version_,          "min_version">{},
            serialization::Field<&TlsConfig::session_cache_,        "session_cache">{},
            serialization::Field<&TlsConfig::require_client_cert_,  "require_client_cert">{},
        };
    }

    class Builder;

private:
    friend class ConfigInterface;
    constexpr TlsConfig() = default;

    std::string cert_file_, key_file_, key_passphrase_, dh_params_file_, ca_file_;
    MinVersion min_version_     = MinVersion::tls12;
    bool session_cache_         = true;
    bool require_client_cert_   = false;
};

class ListenerConfig final : public serialization::ConfigInterface<ListenerConfig, Json::Value> {
public:
    enum class Transport : std::uint8_t { tcp, tls, quic };

    constexpr void validate() const override;
    /* accessors */

    static constexpr auto fields() {
        return std::tuple{
            serialization::Field<&ListenerConfig::bind_address_, "bind">{},
            serialization::Field<&ListenerConfig::port_,         "port">{},
            serialization::Field<&ListenerConfig::transport_,    "transport">{},
            serialization::Field<&ListenerConfig::protocols_,    "protocols">{},
            serialization::Field<&ListenerConfig::tls_,          "tls">{},
        };
    }

    class Builder;

private:
    friend class ConfigInterface;
    constexpr ListenerConfig() = default;

    std::string bind_address_ = "0.0.0.0";
    std::uint16_t port_       = 8080;
    Transport transport_      = Transport::tcp;
    std::vector<Protocol> protocols_;
    std::optional<TlsConfig> tls_;
};

class ServerConfig final : public serialization::ConfigInterface<ServerConfig, Json::Value> {
public:
    enum class PathNormalization : std::uint8_t {
        none,
        collapse_trailing_slash,
        collapse_multi_slash,
    };

    void validate() const override;
    /* accessors */

    static constexpr auto fields() {
        return std::tuple{
            serialization::Field<&ServerConfig::listeners_,           "listeners">{},
            serialization::Field<&ServerConfig::threads_,             "threads">{},
            serialization::Field<&ServerConfig::timeouts_,            "timeouts">{},
            serialization::Field<&ServerConfig::body_limit_,          "body_limit">{},
            serialization::Field<&ServerConfig::request_arena_size_,  "request_arena_size">{},
            serialization::Field<&ServerConfig::drain_timeout_,       "drain_timeout_ms">{},
            serialization::Field<&ServerConfig::path_normalization_,  "path_normalization">{},
        };
    }

    class Builder;

private:
    friend class ConfigInterface;
    ServerConfig() = default;

    std::vector<ListenerConfig> listeners_;
    std::size_t threads_              = 1;
    Timeouts timeouts_;
    std::size_t body_limit_           = 16 * 1024 * 1024;
    std::size_t request_arena_size_   = 8192;
    std::chrono::milliseconds drain_timeout_ = std::chrono::seconds{30};
    PathNormalization path_normalization_ = PathNormalization::collapse_trailing_slash;
};
```

The old `firewall_config.hpp` data types (`rate_limit`, `ip_rule`) are dropped in v1 (PR 7 D1): they were deleted
with the legacy config, nothing consumes them, and they return only alongside a real rate-limit middleware.

### 10.2 Loading

```cpp
struct ConfigFileError    { std::string path; std::string reason; };
struct ConfigParseError   { std::string path; std::size_t line; std::string detail; };
struct ConfigSchemaError  { std::string path; std::string field_path; std::string detail; };

gears::Outcome<ServerConfig,
               ConfigFileError,
               ConfigParseError,
               ConfigSchemaError>
    load_server_config(std::string_view path);

Json::Value dump_server_config(const ServerConfig&);   // round-trip

// ADL conversions — for rare cases where config errors land on a response (admin endpoints).
Response to_http_response(const ConfigFileError&);
Response to_http_response(const ConfigParseError&);
Response to_http_response(const ConfigSchemaError&);
```

**Landed shape (PR 6):** `ServerConfig::deserialize<Json::Value>` — the machinery this section describes — had
never been instantiated before this PR; it did not compile when exercised (two-phase lookup could not reach the
format overloads from the extension point's dependent calls). Repaired and locked by
`tests/unit_tests/serialization/test_config_interface.cpp` (`Demiplane.Tests.Unit.Serialization`, 9 tests) — see
§16 (D1). `ConfigSchemaError::field_path` is best-effort: today it is always empty (no code path populates it), and
`detail` names the offending field instead (every `validate()` message and enum-codec throw embeds the field name). The
YAML-pointer-style
`/listeners/1/tls/cert_file` paths this section originally sketched would need a path stack threaded through every
`read_field` overload's signature — deferred as a strictly additive follow-up; the `field_path` member stays so the
API doesn't break when it lands (D4).

Implementation:

1. Open and read file → on failure, `ConfigFileError`. Rejects non-regular files (a directory would otherwise
   surface as a confusing parse error) (D9).
2. Parse JSON via the existing `Json::CharReader` pipeline → on failure, `ConfigParseError`; the line number is
   best-effort, extracted from jsoncpp's formatted error message (`0` when unextractable) (D9).
3. Walk `ServerConfig::fields()` via the framework's parse machinery — a missing key keeps the field's declared
   default; a type mismatch or an unknown enum string → `ConfigSchemaError` (`detail` names the field; `field_path`
   best-effort, D4). A well-formed non-object top-level JSON value (e.g. `"42"`) is also a `ConfigSchemaError`
   rather than silently producing an all-defaults config (D9). Unknown JSON keys are ignored (D9).
4. Call `validate()` on populated config (via `Builder::finalize()`). `std::invalid_argument` → caught, rethrown as
   `ConfigSchemaError`.

Also resolved in this PR: `build_ssl_context`'s cipher-list TODO — an `SSL_CTX_set_cipher_list` failure now throws
`boost::system::system_error` carrying the OpenSSL error code and the asio SSL error category (D9).

### 10.3 Wiring config to runtime

`ServerConfig` *plus an executor* are the inputs to `Server`'s constructor. Listeners aren't auto-spawned from config
alone — driver instances with their per-protocol config come from code. `ServerConfig::threads` is consumed only by
`run_standalone`; the injected path takes its threads from whatever runs the executor.

Injected-executor path (HTTP shares the process with other subsystems):

```cpp
auto cfg = load_server_config("server.json");
if (!cfg) { /* report and exit */ }

// Caller owns the io_context + threads — e.g. one context dedicated to HTTP,
// separate ones for the DB pool / logger / S3 client.
asio::io_context http_ioc;
std::jthread http_thread{[&]{ http_ioc.run(); }};

Server server{cfg.value(), http_ioc.get_executor()};
attach_default_listeners(server);            // helper for the common case

auto users = std::make_shared<UserController>(user_service);
add_basic_middleware(*users, log_mw, request_id_mw);
server.in_group("/api/v1").add_controller(users);

server.setup();                              // accept loops go live on http_ioc
// ... install SIGINT -> server.stop() ...
server.wait_until_stopped();                 // §9.7: keep driving http_ioc until this returns
http_ioc.stop();                             // only now is it safe to tear down
```

Standalone path (HTTP owns the process) collapses all of that:

```cpp
run_standalone(cfg.value(), cfg.value().threads(), [&](Server& server) {
    attach_default_listeners(server);
    server.in_group("/api/v1").add_controller(users);
});   // blocks until graceful shutdown; owns context + threads + signal handling
```

`attach_default_listeners(Server&)` — landed with no `DefaultDrivers` parameter (D5) — walks `cfg.listeners()` and
dispatches by transport/protocol set, deriving `Http11Config` from server-level `timeouts`/`body_limit`
(`max_header_bytes` keeps its struct default; no `ServerConfig` field maps to it in v1). v1-supported
`(transport, protocols)` combinations: `tcp+[http1]`; `tls+` any non-empty duplicate-free subset/order of
`{http1, http2}`, where **JSON order = ALPN server-preference order = template-argument order**; `quic+[http3]`.
An empty `protocols` list defaults per transport (`http1`; `http3` on `quic`). Any other combination throws
`std::invalid_argument` (notably `tcp+[http2]` — h2c is unsupported). An empty `listeners` array is a no-op —
programmatic `add_*_listener` calls compose freely with config-driven ones. `ServerConfig::threads` is forwarded
by the caller (`run_standalone(cfg, cfg.threads(), …)`, as sketched above); for per-listener tuning beyond what
`ServerConfig` expresses, the user iterates `cfg.listeners()` manually.

### 10.4 YAML follow-up (out of v1 scope)

Same `ConfigInterface` pattern; new format type parameter (`Yaml::Value` or whatever the eventual lib uses). Config
types themselves don't change. Strictly additive.

### 10.5 Hot-reload removed

`reload_if_changed` is removed (not stubbed). Listeners can't hot-rebind without socket churn; routes are baked at
startup; timeouts are baked into driver configs; threads/arena can't change without restart. The honest answer: config
changes require server restart in v1. systemd/k8s-orchestrated graceful shutdown + relaunch is the deployment pattern;
the `drain_timeout`-driven graceful shutdown makes this transparent to clients.

## 11. Allocation Strategy (Performance Notes)

### 11.1 Per-request hot path

Target: framework-internal hot path stays at ~zero heap allocations per request.

For one `GET /users/42` with 6 headers and 0-byte body:

| Source                                                                                       | Heap allocs                           |
|----------------------------------------------------------------------------------------------|---------------------------------------|
| Receive buffer (`beast::flat_buffer`)                                                        | 0 (per-connection reuse)              |
| Beast parsing into `fields`                                                                  | 0 (allocator points at request arena) |
| `Headers` (BeastBacking view)                                                                | 0                                     |
| `Body` (zero-copy span over Beast's body buffer)                                             | 0                                     |
| Path/query params (arena `small_vector` + URL-decoded values)                                | 0                                     |
| `RequestContext::TypedBag` (untouched in this path)                                          | 0                                     |
| Response body (`std::string` user-built)                                                     | 1 — fundamental, user data            |
| Response headers (3 entries, arena)                                                          | 0                                     |
| Coroutine frames (`router.dispatch` + baked handler + user handler; +1 per middleware layer) | ≈3 + M — counted budget, see below    |

**Result: 1 user-data alloc + a fixed, counted coroutine-frame budget per request.**

**Coroutine frames are real heap allocations.** `asio::awaitable` frames allocate via global `operator new`, and HALO
cannot elide them across `std::function` boundaries. They are *excluded from the invariant and gated as an exact
budget*: the PR3 wire gate asserts the measured count **equals** frame budget + the user body alloc — an accidental
extra allocation still fails the gate even though frames exist. Shrinking the budget (frame pooling, a custom task type)
is a possible future optimization, not v1.

**This is now an enforced invariant, not an aspiration.** It holds because responses are built through `ctx` (§5.4) —
headers and the response's *stored* allocator point at the arena, so even post-handler middleware mutation stays off the
global heap — and `Body` is an SBO value type (§5.2) with no `unique_ptr` node. The one heap alloc is the user's body
`std::string`; a handler that serializes into an arena `std::pmr::string` removes even that.

**Verification scope (important).** A replaced global `operator new`/`operator delete` pair, thread-locally armed around
the measured region, gates the invariant (§14) — a pmr default-resource counter cannot see plain `std::string`/
`std::vector`/coroutine-frame allocations, which are exactly the accidental classes the gate exists to catch. In **PR1
** (types only — no driver, no connection arena, no `BeastRequestBody`) the gate can verify only the **response** side:
`ctx.json(...)` / `ctx.ok(...)` perform zero heap allocations (the measured regions contain no coroutines, so the budget
there is exactly zero). The full wire-path invariant — including the zero-copy *request* side — is only verifiable once
the h1 driver and connection arena exist (**PR3**), where the gate runs against a real request on the wire. The §11.1
table above is therefore a PR3 acceptance target, partially gated from PR1.

### 11.2 Per-connection

- `RequestArena` initial buffer: one per-connection heap block of `request_arena_size` bytes (default 8KB), allocated
  once per connection and reused across every keep-alive request on it (`reset()` rewinds to the block). Grows via
  upstream blocks if a request exceeds it.
- `beast::flat_buffer` reused across requests on a keep-alive connection.
- Per-connection cancel signal lives in `ConnectionTracker`'s list.

### 11.3 What remains user-controllable

Response body content: if a handler builds JSON via `Json::Value::toStyledString()`, that's the user's allocation chain.
Framework moves it into the `Response`. For large payloads, the user can opt into `ctx.stream(...)` (§5.4) — the
producer closure writes chunks directly to the `Body::Writer` the driver hands it; no buffering buried in the framework.

## 12. Migration Plan

### 12.1 File-by-file

| Path (current)                                                    | Disposition                                                                                                                                                                                                                                               |
|-------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `components/http/CMakeLists.txt`                                  | Replaced — see §13                                                                                                                                                                                                                                        |
| `components/http/config/include/firewall_config.hpp`              | Deleted (pre-PR 1). NOT moved to `routing/firewall/` — dropped in v1 (PR 7 D1); returns with a real rate-limit middleware.                                                                                                                                |
| `components/http/config/include/router_config.hpp`                | Deleted. `server` struct, `route_table`, `route_flags`, `load_from_yaml`, `dump_to_yaml`, `reload_if_changed` all gone. Replaced by `ServerConfig` (§10).                                                                                                 |
| `components/http/config/include/tls_config.hpp`                   | Replaced by `config/tls_config/`. Type renamed (was `tls_settings`), now `TlsConfig` with `ConfigInterface`.                                                                                                                                              |
| `components/http/export/demiplane/http`                           | Kept as umbrella header; aggregate `#include`s updated.                                                                                                                                                                                                   |
| `components/http/http_server/include/aliases.hpp`                 | Deleted. Beast types no longer leak.                                                                                                                                                                                                                      |
| `components/http/http_server/include/controller.hpp`              | Replaced by `routing/controller/`.                                                                                                                                                                                                                        |
| `components/http/http_server/include/request_context.hpp`         | Replaced by `types/request_context/`.                                                                                                                                                                                                                     |
| `components/http/http_server/include/response_factory.hpp`        | Replaced by `types/response_factory/`.                                                                                                                                                                                                                    |
| `components/http/http_server/include/route_registry.hpp`          | Replaced by `routing/route_registry/`.                                                                                                                                                                                                                    |
| `components/http/http_server/include/server.hpp`                  | Replaced by `server/server/`.                                                                                                                                                                                                                             |
| `components/http/http_server/source/*.cpp`                        | All deleted; replaced by per-layer source files.                                                                                                                                                                                                          |
| `tests/manual_tests/http/handler/manual_example_http_handler.cpp` | Deleted (pre-PR 1). Behaviour covered by `tests/integration_tests/http/` (PRs 4-5); example pattern lives at `examples/http/minimal_http_server.cpp` (PR 7).                                                                                              |
| `benchmarks/http/*`                                               | Deleted (pre-PR 1); restored in PR 7 — `bench_server.cpp` on the new API, `http_bomber.cpp` since rewritten as a saturating keep-alive generator (the verbatim restore slept between reconnects), plus `drogon_bench_server.cpp` as the reference server. |

### 12.2 Phased PRs

| PR | Scope                                                                                                                                                                                                                                                                                                                                                   | Self-contained?                                                            |
|----|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------|
| 1  | **Types layer** — `Request`, `Response`, `Headers`, `Body`, `RequestContext`, `errors.hpp`, `ResponseFactory`, `AsyncOutcome`, `http_enums.hpp`. + unit tests.                                                                                                                                                                                          | Yes — no driver, no server, just types.                                    |
| 2  | **Routing layer** — `RouteRegistry`, `HttpController` evolution, `Middleware` shape, `GroupBinding`, `Router`, the bake step, conflict detection. + unit tests. Old `Server` still wires it.                                                                                                                                                            | Yes — registry/controller logic testable with synthetic `RequestContext`s. |
| 3  | **Connection + Driver interface + `Http11Driver`** — concept-based connections, templated `serve()`, bug-fix battery (URL decode, body limits, timeouts, exception → 500). h2/h3 scaffolds. vcpkg deps for nghttp2/ngtcp2/nghttp3 added. + driver-level tests.                                                                                          | Yes — drivers testable against fake connections.                           |
| 4  | **Listeners + TLS** — `ListenerBase`, `TcpListener`, `TlsListener`, `QuicListener` scaffold, `ConnectionTracker`, `build_ssl_context`. + integration tests for h1-over-TCP and h1-over-TLS.                                                                                                                                                             | Yes — first end-to-end testable layer.                                     |
| 5  | **`Server` rewrite + observers + lifecycle** — executor injection (`Server(cfg, any_io_executor)`, owns no threads), `setup`/`stop`/`wait_until_stopped`, `graceful_shutdown`, §9.7 shutdown-ordering contract, `run_standalone` convenience, observer interface. + lifecycle integration tests.                                                        | Yes — replaces old `Server`; architecture goes "live" here.                |
| 6  | **Config (JSON + `ConfigInterface`)** — all config types, `load_server_config`, `attach_default_listeners` helper. + config tests.                                                                                                                                                                                                                      | Yes — JSON-loaded server is then a one-liner.                              |
| 7  | **Umbrella + cleanup** — populate the `Demiplane::Component::Http` combined library + `<demiplane/http>` umbrella header (all 7 layers), umbrella smoke test, restore `http_bomber` + add `bench_server`, create `examples/http/`, final spec reconciliation. (The old-code deletions this row originally listed landed pre-PR 1 with d3c40ef/150b666.) | Yes — the redesign's closing PR.                                           |

Each PR builds and tests cleanly. The old code was deleted just before PR 1 (d3c40ef) rather than coexisting
through PRs 1-6 as originally sketched; PR 7 closed the redesign by aggregating the public umbrella and restoring
the benchmark/example surface.

## 13. CMake Reorganization

Each layer becomes its own static library; the public umbrella aggregates them via the existing `add_combined_library`
macro. Skeleton:

```cmake
set(DMP_HTTP ${DMP_COMPONENT}.HTTP)   # matches the existing file; the public alias stays Demiplane::Component::Http

# Types (foundation; depended on by everything)
add_library(${DMP_HTTP}.Types STATIC
    types/request/request.cpp
    types/response/response.cpp
    types/headers/headers.cpp
    types/body/body.cpp
    types/request_context/request_context.cpp
    types/response_factory/response_factory.cpp
    types/errors/errors.cpp)
target_include_directories(${DMP_HTTP}.Types PUBLIC types)
target_link_libraries(${DMP_HTTP}.Types PUBLIC
    Demiplane::Common::Gears
    Demiplane::Common::Scroll
    Boost::beast Boost::asio Boost::container
    JsonCpp::JsonCpp)
# Landed shape (PR 7): tests keep per-layer dotted links (D4) — no per-layer
# :: aliases exist; umbrella propagation is guarded by a dedicated smoke test.

# Routing
add_library(${DMP_HTTP}.Routing STATIC
    routing/route_registry/route_registry.cpp
    routing/controller/controller.cpp
    routing/group/group.cpp
    routing/middleware/middleware.cpp
    routing/router/router.cpp)
target_include_directories(${DMP_HTTP}.Routing PUBLIC routing)
target_link_libraries(${DMP_HTTP}.Routing PUBLIC
    ${DMP_HTTP}.Types
    Demiplane::Common::Nexus)   # NEXUS_REGISTER lives on HttpController

# Connection (mostly header-only — concepts + value types)
add_library(${DMP_HTTP}.Connection STATIC
    connection/request_arena/request_arena.cpp
    connection/tcp_connection/tcp_connection.cpp
    connection/tls_connection/tls_connection.cpp
    connection/quic_connection/quic_connection.cpp)
target_include_directories(${DMP_HTTP}.Connection PUBLIC connection)
target_link_libraries(${DMP_HTTP}.Connection PUBLIC
    ${DMP_HTTP}.Types
    Boost::asio
    OpenSSL::SSL OpenSSL::Crypto)

# Drivers
add_library(${DMP_HTTP}.Drivers STATIC
    drivers/http11/http11_driver.cpp
    drivers/http2/http2_driver.cpp     # scaffold
    drivers/http3/http3_driver.cpp)    # scaffold
target_include_directories(${DMP_HTTP}.Drivers PUBLIC drivers)
target_link_libraries(${DMP_HTTP}.Drivers PUBLIC
    ${DMP_HTTP}.Types ${DMP_HTTP}.Routing ${DMP_HTTP}.Connection
    Boost::beast nghttp2::nghttp2 ngtcp2::ngtcp2 nghttp3::nghttp3)

# Listeners
add_library(${DMP_HTTP}.Listeners STATIC
    listeners/tcp_listener/tcp_listener.cpp
    listeners/tls_listener/tls_listener.cpp
    listeners/tls_listener/build_ssl_context.cpp
    listeners/quic_listener/quic_listener.cpp     # scaffold
    listeners/connection_tracker/connection_tracker.cpp)
target_include_directories(${DMP_HTTP}.Listeners PUBLIC listeners)
target_link_libraries(${DMP_HTTP}.Listeners PUBLIC
    ${DMP_HTTP}.Connection ${DMP_HTTP}.Drivers
    OpenSSL::SSL OpenSSL::Crypto)

# Config
add_library(${DMP_HTTP}.Config STATIC
    config/server_config/server_config.cpp
    config/tls_config/tls_config.cpp
    config/listener_config/listener_config.cpp
    config/timeouts/timeouts.cpp
    config/load_server_config/load_server_config.cpp)
target_include_directories(${DMP_HTTP}.Config PUBLIC config)
target_link_libraries(${DMP_HTTP}.Config PUBLIC
    ${DMP_HTTP}.Types
    Demiplane::Common::Serialization
    JsonCpp::JsonCpp)

# Server
add_library(${DMP_HTTP}.Server STATIC
    server/server/server.cpp
    server/server_observer/server_observer.cpp)
target_include_directories(${DMP_HTTP}.Server PUBLIC server)
target_link_libraries(${DMP_HTTP}.Server PUBLIC
    ${DMP_HTTP}.Routing ${DMP_HTTP}.Listeners ${DMP_HTTP}.Config)

# Public umbrella
add_combined_library(${DMP_HTTP}
    DIRECTORIES export/
    LIBRARIES
        ${DMP_HTTP}.Server
        ${DMP_HTTP}.Listeners
        ${DMP_HTTP}.Drivers
        ${DMP_HTTP}.Routing
        ${DMP_HTTP}.Types
        ${DMP_HTTP}.Config
        ${DMP_HTTP}.Connection)

add_library(Demiplane::Component::Http ALIAS ${DMP_HTTP})
```

vcpkg manifest additions to project-level `vcpkg.json`, all landing in PR 3 alongside the driver scaffolds:

- `nghttp2` — h2 driver scaffold links it; full impl is a future self-contained PR.
- `ngtcp2` — h3 driver QUIC layer.
- `nghttp3` — h3 driver HTTP semantics layer.
- `openssl` — already present (Boost.Asio uses it).
- `gtest` — already present.
- `yaml-cpp` — **not** added in v1 (deferred per §10.4).

## 14. Test Strategy

### 14.1 Unit tests (`tests/unit_tests/http/`)

No I/O, no network, no threads. Each runs in milliseconds.

- `route_registry_test.cpp` — add/freeze/lookup; conflict detection with batched aggregation; 404 vs 405 with Allow
  header; parametric segment matching; URL-decoded param capture; path normalization (trailing slash, multi-slash, case
  preservation).
- `headers_test.cpp` — multi-value get/get_all, case-insensitive lookup, `add` vs `set` semantics, BeastBacking ↔
  OwnedBacking interchange.
- `body_parsing_test.cpp` — `read_to_string` / `read_json` / `read_form` / `read_multipart` against synthetic bodies,
  including limit-exceed paths, malformed payload paths, URL decoding edge cases (`+`, `%XX`, invalid escapes).
- `outcome_to_response_test.cpp` — every built-in error type round-trips through `to_http_response`; user-defined error
  type with custom conversion compiles and dispatches correctly; missing conversion produces a clear compile error.
- `request_context_test.cpp` — set/get/has type-keyed bag, type collisions, params accessed before set return nullopt.
- `config_load_test.cpp` — valid JSON loads; missing required field surfaces field path; type mismatch surfaces field
  path; round-trip via `dump_server_config` produces equivalent config; enum string mappings round-trip.
- `allocation_gate_test.cpp` — replaced global `operator new`/`operator delete` (thread-locally armed around the
  measured region) assert that building a success response via `ctx.json(...)` / `ctx.ok(...)` performs **zero** heap
  allocations (headers + body wrapper); the user's body string is constructed before the region. A pmr default-resource
  counter is deliberately *not* used — it cannot see plain-container or coroutine-frame allocations. **PR1 scope =
  response side only** (no driver/connection arena yet); extended to the full request+response wire path in PR3 (§11.1),
  where the assertion becomes *exact-budget*: coroutine frames + user body, nothing else. This test is what keeps the
  zero-additional-allocation invariant from silently rotting.

### 14.2 Integration tests (`tests/integration_tests/http/`)

Per project preference: real systems, no mocks. Each test binds `127.0.0.1:0`, captures `bound_port()`, issues real HTTP
via Beast client, asserts on the wire response, then `stop()`s and joins.

Coverage:

- All HTTP verbs (GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS) with handler dispatch.
- Path parameters: single, multiple, URL-decoded values.
- Query parameters: single, multiple, URL-decoded, missing-and-defaulted.
- JSON body: round-trip; malformed → 400 via `JsonParseError`; oversize → 413 via `BodyLimitExceeded`.
- Form body: same battery.
- Multipart body: small, file upload (10 KB), boundary edge cases.
- Headers: case-insensitive lookup, multi-value `Set-Cookie` round-trip.
- Outcome error mapping: user-defined error types with custom `to_http_response`.
- Middleware: single, chain (3 deep), short-circuit (auth returns 401 without calling next), modify-after.
- Groups: single-level, nested, multiple controllers in same group, conflict detection across controllers.
- 404 vs 405: known path, unknown verb → 405 + correct `Allow`.
- Lifecycle: `setup()` failure on port-in-use; on an injected executor, `setup()` goes live and `wait_until_stopped()`
  returns only after `stop()` + graceful shutdown; `stop()` idempotent and does **not** stop the caller's executor (
  §9.7); SIGINT (via `run_standalone`) triggers graceful shutdown; registration after `setup()` throws.
- Graceful shutdown: in-flight completes; new connections refused; `on_shutdown_started` runs and is awaited; drain
  timeout force-cancels remaining.
- Concurrency: 1000 concurrent requests across N workers; correct responses; clean under TSan.
- TLS: handshake against self-signed cert (test fixture); ALPN negotiates h1; client offering only `h2` + listener
  without h2 driver → connection closes.
- Body streaming: `Response::stream(...)` correct chunked transfer; client receives chunks.
- Body large: 16 MB streamed download succeeds; 17 MB request → 413.
- Observer: `on_request` / `on_response` fire in order; `on_unhandled_exception` fires for non-Outcome handler throws.

### 14.3 Out of scope for v1 tests

- h2 / h3 wire-level tests (drivers are stubbed).
- TLS edge cases (cert chain validation, OCSP, session resumption).
- Performance regression tests (separate benchmarks).
- Hot-reload (out of scope).

## 15. Decisions Log

| Decision                              | Choice                                                                                                                                                                                                                                                                                                                    | Why                                                                                                                                                                                                                                                                                |
|---------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Build vs. buy h2/h3                   | Hybrid: build h1 (Beast), buy h2 (nghttp2) and h3 (ngtcp2 + nghttp3); v1 ships h1 only with stubs                                                                                                                                                                                                                         | Matches user intent to write h2/h3 themselves later; nglibs reduce attack surface and bugs                                                                                                                                                                                         |
| Body model                            | Streaming truth, buffered convenience helpers                                                                                                                                                                                                                                                                             | Maps to all three protocols; ergonomic for JSON-API common case; opt-in streaming for large payloads                                                                                                                                                                               |
| Driver interface                      | Single coroutine `serve(conn, router)`                                                                                                                                                                                                                                                                                    | One method per driver; protocol complexity stays inside; coroutine-native consistent with rest of codebase                                                                                                                                                                         |
| Listener model                        | ALPN-multiplexed TLS + separate UDP listener for h3                                                                                                                                                                                                                                                                       | Standard production pattern; h1+h2 share port 443 via ALPN; h3 is separate transport entirely                                                                                                                                                                                      |
| Routing API                           | Evolve B: keep controller-merge, add prefix + middleware + verbs                                                                                                                                                                                                                                                          | Controller-merge concept is the strongest part of current design; preserve and extend                                                                                                                                                                                              |
| Group/middleware scope                | Controller-only middleware (option A); group via `server.in_group(prefix).add_controller(...)`                                                                                                                                                                                                                            | User wants explicit control; repetition resolved via free helper functions                                                                                                                                                                                                         |
| Error model                           | `gears::Outcome<Response, Errors...>` + `catch(...) → 500`                                                                                                                                                                                                                                                                | Project's existing Outcome type; multiple typed errors per handler; catch-all for unexpected only                                                                                                                                                                                  |
| Error → Response                      | ADL `to_http_response(const E&)`                                                                                                                                                                                                                                                                                          | Compile-time checked (missing = build break); zero overhead; lives next to error type                                                                                                                                                                                              |
| Build-out scope                       | Option C: everything except protocol drivers                                                                                                                                                                                                                                                                              | TLS interface + impl, ALPN real, body limits, timeouts, graceful shutdown, JSON config; only h2/h3 protocol bodies stay stubbed                                                                                                                                                    |
| Allocation strategy                   | Per-request arena + Beast allocator redirect + Headers tagged-union facade                                                                                                                                                                                                                                                | Zero framework heap allocs on hot path; user-data allocs unchanged                                                                                                                                                                                                                 |
| Allocation policy                     | Zero-additional-alloc **invariant** (arena in both directions), gated by a counting `memory_resource` test                                                                                                                                                                                                                | User's #1 goal is minimal overhead; an invariant + gate keeps it from rotting, unlike an aspirational table. Body = SBO value type; Response stores its arena allocator                                                                                                            |
| Response construction                 | ctx-scoped factories (`ctx.json(...)`) — review option A                                                                                                                                                                                                                                                                  | Threads the arena in automatically; impossible to forget and silently fall back to global heap. Static `ResponseFactory` retained for ctx-less/error path only                                                                                                                     |
| Error → Response alloc                | `to_http_response` arena-free; 4xx/5xx on the global heap                                                                                                                                                                                                                                                                 | Cold path; keeps the extension point a clean one-arg free function                                                                                                                                                                                                                 |
| `target` representation               | `string_view` into the receive buffer (not an owned `std::string`)                                                                                                                                                                                                                                                        | Zero-copy; avoids the SSO-dangling cache bug; consistent with the rest of the views-into-buffers request model                                                                                                                                                                     |
| Executor ownership                    | Injected (`any_io_executor`); Server owns no `io_context`/threads; `run_standalone` convenience                                                                                                                                                                                                                           | HTTP coexists with logger/DB/S3; caller controls thread↔context topology; also subsumes the per-thread-`io_context` throughput lesson                                                                                                                                              |
| Shutdown ownership                    | `stop()` never stops the executor; caller drives it until `wait_until_stopped()` (§9.7)                                                                                                                                                                                                                                   | The executor is shared with subsystems that must outlive HTTP; stopping it would kill drain/observers mid-flight                                                                                                                                                                   |
| Connection abstraction                | Concept-based, no virtual hierarchy, no `dynamic_cast`                                                                                                                                                                                                                                                                    | Concrete connection types satisfy `IsConnection`; drivers templated on connection type; polymorphism only at listener layer                                                                                                                                                        |
| Lifecycle                             | Injected executor; `setup()` (sync bind + spawn loops) / `stop()` (non-blocking, never stops the executor) / `wait_until_stopped()` (block); `run_standalone` convenience                                                                                                                                                 | Honest failure surface; testable; coexists with other subsystems on caller-owned executors; see §9.7 contract                                                                                                                                                                      |
| Observer model                        | Single typed `ServerObserver` interface with `exception_ptr` for errors                                                                                                                                                                                                                                                   | Replaces 10-vector mess; UAF structurally impossible (heap-managed exception_ptr)                                                                                                                                                                                                  |
| Routing perf                          | Segment-vector parametric routing, not `std::regex`                                                                                                                                                                                                                                                                       | Original design's headline perf bug; segment vectors are 10x+ faster, simpler, swap-in trie later if needed                                                                                                                                                                        |
| Config format                         | JSON via `ConfigInterface` for v1; YAML follow-up                                                                                                                                                                                                                                                                         | Project's established pattern (FileSinkConfig); YAML adds the format param later                                                                                                                                                                                                   |
| Hot-reload                            | Removed (not stubbed)                                                                                                                                                                                                                                                                                                     | Genuinely hard; restart-on-change is honest; revisit when production deployment demands it                                                                                                                                                                                         |
| Directory structure                   | Co-located per-thing dirs (`types/request/{hpp,cpp}`), no include/source split                                                                                                                                                                                                                                            | User preference; cleaner colocation                                                                                                                                                                                                                                                |
| Middleware `next` passing             | `const NextHandler&` (non-owning)                                                                                                                                                                                                                                                                                         | A by-value `std::function` copy = heap alloc per layer per request; the referent lives in the frozen baked chain and outlives the request                                                                                                                                          |
| Coroutine frames                      | Excluded from the zero-alloc invariant; gated as an exact counted budget (≈3 + middleware depth)                                                                                                                                                                                                                          | `asio::awaitable` frames heap-allocate and HALO cannot elide across `std::function`; an *exact-count* gate still catches accidental extras                                                                                                                                         |
| Allocation-gate mechanism             | Replaced global `operator new`/`delete`, thread-locally armed                                                                                                                                                                                                                                                             | A pmr default-resource counter cannot see plain-container or coroutine-frame allocations — the accidental classes the gate exists for                                                                                                                                              |
| Path decoding                         | Split raw on `/`, decode captured segments only (`+` literal); literals match raw bytes; `%` rejected in route literals                                                                                                                                                                                                   | `%2F` can never become a separator; zero decode work on the no-`%` hot path                                                                                                                                                                                                        |
| Headers move-assign                   | User-defined: adopts the source backing (variant emplace)                                                                                                                                                                                                                                                                 | Defaulted pmr move-assign (POCMA=false) element-copies into the dest's old allocator — `Response r; r = co_await next(ctx);` would silently land arena headers on the global heap                                                                                                  |
| Cancellation fan-out                  | One `cancellation_signal` per listener + per-connection signals in the tracker; emits dispatched via the target's strand                                                                                                                                                                                                  | A `cancellation_slot` holds at most one handler; `emit` is not thread-safe against concurrent ops                                                                                                                                                                                  |
| Concept naming                        | `Is*` for role/type-identity concepts (`IsConnection`, `IsStreamConnection`, `IsHttpDriver`, `IsRouteHandler`); `Has*` for single-capability concepts (`HasToHttpResponse`)                                                                                                                                               | Repo-wide convention (codestyle §Concept Naming; DB precedent `IsSqlDialect`/`HasAcceptVisitor`); renamed in PR4's patch round to keep spec and code in lockstep before PR5                                                                                                        |
| Observer per-request hooks            | Fired in `Router::dispatch` via nullable `std::function` hooks the Server wires at `setup()` (PR 5 D2)                                                                                                                                                                                                                    | No observer interface below the server layer, no dynamic-inheritance split (user decision 2026-07-05); routing keeps zero server dependency; unset hooks cost one null check                                                                                                       |
| `on_response` signature               | `(const RequestInfo&, const Response&)` — snapshot of {method, target} taken at dispatch entry (PR 5 D3)                                                                                                                                                                                                                  | The spec's original `(const RequestContext&, …)` was unimplementable: the ctx is consumed by value by the handler chain                                                                                                                                                            |
| `setup()` observer barrier            | AWAITED on `exec_` BEFORE the accept loops spawn (revised 2026-07-06; supersedes PR 5 D4). `setup()` blocks; the executor must already be driven (`run_standalone` and the fixture start workers first)                                                                                                                   | A detached notification could outlive the Server (UAF) and lets shutdown hooks overtake setup hooks; the barrier also guarantees observer-owned resources exist before the first request                                                                                           |
| `starting` state + latched stop       | `setup()` sets `starting` before the barrier/spawn; a racing `stop()` latches `stop_requested_`, honored at setup()'s tail (2026-07-06)                                                                                                                                                                                   | Without it a stop() from an observer/early handler CASes against `build`, silently no-ops, and `wait_until_stopped()` hangs forever                                                                                                                                                |
| Destructor policy                     | Sync RAII backstop: WRN log + `stop()` + `wait_until_stopped()` when destroyed active (2026-07-06)                                                                                                                                                                                                                        | Frames in caller-owned executor queues cannot be recalled — only completion unwinds them; a loud block beats freeing members under live frames (UAF); typical `main()` destroys the Server before the executor                                                                     |
| Listener bind failure                 | Best-effort-all: bind every listener, aggregate into `ListenerBindError`, clear the listener set before throwing (2026-07-06)                                                                                                                                                                                             | Operator sees every bad endpoint at once; a bound-but-loopless acceptor would trap clients in the kernel backlog; controllers are RAII and need no unwind                                                                                                                          |
| Controller lifecycle hooks            | REMOVED — `initialize()`/`shutdown()` deleted; controllers are RAII (2026-07-06)                                                                                                                                                                                                                                          | Dead virtuals nobody overrode; ctor/dtor cover sync resources; async shutdown-time work belongs in `ServerObserver::on_shutdown_started`                                                                                                                                           |
| Stop-signal ownership                 | `shared_ptr<cancellation_signal>` captured by the phase-1 emit lambdas (2026-07-06)                                                                                                                                                                                                                                       | The lambdas queue in io_context-owned strand queues and can outlive the Server (shutdown can complete without yielding when loops are already dead); a late emit on an orphaned signal is harmless                                                                                 |
| `async_wait_stopped` cost             | Exponential-backoff poll, 5 ms → 320 ms cap (2026-07-06)                                                                                                                                                                                                                                                                  | A second Server-side completion primitive (event/channel) re-creates the phase-5 last-touch race; backoff keeps steady-state at ≈3 wakeups/s with bounded detection latency                                                                                                        |
| Accept-loop threading                 | One strand per listener `run()`; stop emits dispatched onto that strand (PR 5 D5)                                                                                                                                                                                                                                         | `cancellation_signal::emit` must be serialized with the loop's turns on a multi-threaded executor; closes the edge-lost-emit window                                                                                                                                                |
| Drain-unwind completion               | `graceful_shutdown` polls `live_accept_loops_ == 0` (phase 1.5) and `Σ in_flight() == 0` (phase 2.5) on a 5 ms tick (PR 5 D6)                                                                                                                                                                                             | Force-cancels are only dispatched; frames unwind in later turns. Polling is the fixture-proven pattern; a tracker completion event adds cross-strand surface for little gain                                                                                                       |
| `ServerConfig` staging                | Plain struct (`request_arena_size`, `drain_timeout`, `path_normalization`) at the final path; PR 6 rewrites in place (PR 5 D1, PR 4 D1)                                                                                                                                                                                   | Same staging pattern as `TlsConfig`/`Http11Config`; config layer keeps its own PathNormalization enum so it carries no routing dependency                                                                                                                                          |
| Serialization machinery repair (D1)   | Extension-point key becomes `serialization::FieldName` (a `string_view` wrapper) so ADL reaches format overloads from every dependent `read_field`/`write_field` call regardless of include order; `deserialize_one_field` reads directly into `builder.config_.*F::ptr`; new `std::vector<T>` and `std::uint16_t` codecs | The machinery had never been instantiated and did not compile when exercised: two-phase lookup froze at the template definition point, and ordinary ADL from scalar value types never reached `demiplane::serialization`; locked by `Demiplane.Tests.Unit.Serialization` (9 tests) |
| HTTP config enum encoding (D2)        | Every enum field (`Transport`, `Protocol`, `TlsConfig::MinVersion`, `PathNormalization`) is string-encoded via non-template ADL codecs declared next to the enum; unknown strings throw `std::invalid_argument` naming the field                                                                                          | Satisfies §14.1 "enum string mappings round-trip"; supersedes §16's "int-encoded is acceptable"; a silent fallback would turn a typo into weaker TLS; `Protocol` codec lives in `listener_config.hpp`, not `http_enums.hpp`, so the types layer stays free of jsoncpp              |
| `tls.key_passphrase` policy (D3)      | `FieldPolicy::Secret` — deserialize-only                                                                                                                                                                                                                                                                                  | `dump_server_config` never emits passphrases; §14.1's round-trip test uses passphrase-free configs, locked by a dedicated omission test                                                                                                                                            |
| `ConfigSchemaError::field_path` (D4)  | Best-effort — always empty today (no populating code path); `detail` names the offending field                                                                                                                                                                                                                            | Full JSON-pointer paths need a path stack threaded through every `read_field` overload signature; deferred as a strictly additive follow-up; member stays for API stability                                                                                                        |
| `attach_default_listeners` shape (D5) | `attach_default_listeners(Server&)`, no `DefaultDrivers` param; v1 combos `tcp+[http1]`, `tls+` non-empty duplicate-free subset/order of `{http1, http2}` (JSON order = ALPN preference = template-arg order), `quic+[http3]`; unsupported combos throw                                                                   | `Http11Config` is fully derivable from `ServerConfig` (body_limit + 3 phase timeouts); YAGNI beyond that until a second driver grows real config                                                                                                                                   |
| Config constructability split (D6)    | `ServerConfig`/`ListenerConfig` Builder-only; `TlsConfig` keeps a public no-validation full-ctor escape hatch; `Timeouts` keeps its public 3-arg ctor                                                                                                                                                                     | Every `ServerConfig` default is valid (`Builder{}.finalize()` replaces the old `ServerConfig{}` at all call sites); scaffold unit tests deliberately build empty `TlsConfig`s that `validate()` must reject on the loading path                                                    |
| `TlsListener` arena ctor (D7)         | Delegating ctor pair `{exec, host, port, tls, [request_arena_size,] drivers...}`; `Server::add_tls_listener` forwards `cfg_.request_arena_size()`                                                                                                                                                                         | A defaulted parameter cannot follow a variadic pack; `QuicListener` stays arena-less (scaffold owns no connections)                                                                                                                                                                |
| Moved-from ctor read fix (D8)         | `Server`'s ctor now initializes `registry_{map_normalization(cfg_.path_normalization())}` from the member, not the already-moved-from constructor parameter                                                                                                                                                               | Latent bug, benign only because moving a plain-struct-shaped config copied scalars; the accessor rewrite touched this line anyway                                                                                                                                                  |
| `load_server_config` hardening (D9)   | Rejects non-regular files and non-object top-level JSON; parse-error line numbers best-effort (0 if unextractable); unknown JSON keys ignored; `build_ssl_context`'s cipher-list check now throws `boost::system::system_error` (OpenSSL error code + asio SSL category) on `SSL_CTX_set_cipher_list` failure             | A directory path or a bare `"42"` top-level JSON would otherwise surface as a confusing parse error or silently produce an all-defaults config; the cipher-list TODO was the last staged PR 6 marker in the listener layer                                                         |
| Firewall data types (PR 7 D1)         | Dropped from v1 — `routing/firewall/` not created                                                                                                                                                                                                                                                                         | Deleted with the legacy config pre-PR 1; zero consumers; dead data types are YAGNI — they return with an actual rate-limit middleware                                                                                                                                              |
| HTTP benchmarks (PR 7 D2)             | `bench_server` on the redesigned API + a rewritten saturating `http_bomber` + a Drogon reference server                                                                                                                                                                                                                   | §12.1's "benchmark logic intact" holds for `bench_server`; the verbatim bomber was a rate-limited, reconnect-per-request pinger and could not measure serving throughput, so it was rewritten (see plan D2 SUPERSEDED)                                                             |
| Examples tree (PR 7 D3)               | New top-level `examples/` (`BUILD_EXAMPLES`, default ON); `examples/http/minimal_http_server.cpp` shows both wiring paths (programmatic + `server.json`)                                                                                                                                                                  | §12.1 promised the example pattern under `examples/http/`; living documentation of the §10.3 standalone path                                                                                                                                                                       |
| Test-link policy (PR 7 D4)            | Existing tests keep per-layer dotted links; a dedicated `...Http.Umbrella` unit target links ONLY the combined lib                                                                                                                                                                                                        | Per-layer links catch under-linking regressions in layer CMake; the smoke target isolates umbrella-propagation failures                                                                                                                                                            |

## 16. Open Questions

None blocking implementation. Things to revisit when their layer is touched:

- **`std::pmr::polymorphic_allocator<>`'s exact integration with Beast's `Allocator` template parameter** — should be
  straightforward (`pmr::polymorphic_allocator<char>` is allocator-compliant) but warrants a small spike at PR 3 start.
- **`ConfigInterface` field-type coverage — RESOLVED (PR 6, D1/D2).** Verification found the machinery had never
  been instantiated and did not compile when exercised: two-phase lookup could not reach the format overloads
  through the extension point's dependent calls. Repaired via a `serialization::FieldName` ADL-anchoring key type
  (§10.1 "Landed shape"); `deserialize_one_field` now reads directly into the field's storage; new
  `std::vector<T>` and `std::uint16_t` codecs cover `std::vector<ListenerConfig>` and `port`; nested configs with
  private framework constructors work held directly, in `std::optional`, and in `std::vector`. Locked by
  `tests/unit_tests/serialization/test_config_interface.cpp` (`Demiplane.Tests.Unit.Serialization`, 9 tests). Enum
  fields are string-encoded (D2), superseding this question's "int-encoded is acceptable for v1" allowance.
- **OpenSSL ALPN callback context lifetime** — the `arg` parameter must outlive the SSL context. Storing it as a member
  of `TlsListener` should suffice; verify with a brief test at PR 4 start.

## 17. References

- Existing `components/http/` source tree (the thing being replaced)
- `common/scroll/sink/file_sink/include/file_sink_config.hpp` — reference pattern for `ConfigInterface` usage
- `common/gears/outcome/gears_outcome.hpp` — reference for `gears::Outcome` API
- Code review feedback from `component/http-1.1/v1.1` branch ultrareview
