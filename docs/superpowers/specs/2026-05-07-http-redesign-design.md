# HTTP Component Redesign — Design Spec

**Date:** 2026-05-07
**Branch:** `component/http-1.1/v1.1`
**Status:** Draft (pending user review before implementation)
**Component:** `components/http/`

## 1. Context

The current `components/http/` module is a thin Boost.Beast HTTP/1.1 server. It introduces a useful idea — controllers compile their own routes locally and then merge them into a server-wide registry — but the surrounding scaffolding has accumulated structural and correctness problems that block both production use and the planned multi-protocol future (HTTP/2 + HTTP/3).

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
- `Server` is a god class owning io_context, threads, accept loop, session loop, request handling, query parsing, route registry, controllers, middleware, ten event-callback vectors, signal handling, and lifecycle state.

Those bugs are symptoms. The architectural problems are deeper: there is no layering between transport, framing, routing, and application; `Server` does too much; the event/callback bus is reinvented and ten-times-duplicated; the sync-vs-async DSL combinatorially explodes (12 overloads per HTTP verb); there is no Connection abstraction; there is no request lifecycle hook. The two parallel "Server" concepts (`router_config::server` data class vs `http::Server` runtime class) are unrelated — one of them is dead code. Tests do not exist.

This spec proposes a clean redesign that fixes the bugs as a side effect of getting the architecture right, sets up the protocol-driver boundary cleanly so HTTP/2 and HTTP/3 can be filled in later, and keeps the controller-merge idea (the strongest part of the existing design) at the center.

## 2. Goals and Non-Goals

### Goals

1. **Layered architecture** with one-way dependencies: application → routing → HTTP semantics → protocol drivers → connections → transport. Each layer has a focused job.
2. **Multi-protocol-ready** through a clean driver interface: HTTP/1.1 implemented in v1; HTTP/2 and HTTP/3 ship as compiling scaffolds with vcpkg deps wired so future-you can fill them in without touching the build.
3. **Controller-merge as the primary application abstraction** — `HttpController` subclasses with `configure_routes()` stay the canonical pattern. Groups, middleware, prefixes, and per-route Outcome→Response error mapping all compose around it.
4. **Performance-conscious from the start.** Per-request arena, header views into Beast's parsed storage with Beast's allocator pointed at our arena, no `std::regex`, no exception-driven control flow on routing misses. Framework-internal hot path stays at ~zero heap allocs per request; user-data allocations are explicitly the user's.
5. **Typed error model** via `gears::Outcome<Response, Errors...>` for handlers, ADL-found `to_http_response(const E&)` for conversion, exception catch-all for unexpected errors only.
6. **Honest lifecycle** — `setup()` binds synchronously and surfaces failures immediately; `start()` blocks until graceful shutdown completes; `stop()` is non-blocking and idempotent. Async shutdown observers run *before* the io_context stops.
7. **Real TLS and config.** `TlsConfig` consumed by `TlsListener`, `build_ssl_context` produces a hardened OpenSSL context with ALPN, `ServerConfig` loaded from JSON via the project's existing `serialization::ConfigInterface` pattern.
8. **Real test coverage.** Unit tests for pure logic; integration tests on `127.0.0.1:0` via real Beast clients exercising the wire.

### Non-goals (v1)

1. **Implementing HTTP/2 and HTTP/3.** Drivers ship as scaffolds. vcpkg manifests carry `nghttp2`, `ngtcp2`, `nghttp3` so the implementations land as future PRs without build-system changes.
2. **YAML config.** JSON via `ConfigInterface` is the v1 format. YAML is a strictly additive follow-up using the same `ConfigInterface` pattern with a different format type parameter.
3. **Hot-reload.** `reload_if_changed` is removed (not stubbed). Restart-on-change is the v1 deployment model. Hot-reload would need its own design pass and probably a `Server::reconfigure(...)` API.
4. **Server push** (h2 PUSH_PROMISE) and **Early Hints** (103). Both require the response abstraction to support multi-response semantics; deferred.
5. **Path-flag gating** (the dead `route_table` from current config). Not a server-core concern; if anyone wants it, it's a middleware.
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
│ Protocol Drivers (HttpDriver concept)                           │
│   Http11Driver — implemented (Boost.Beast under the hood)       │
│   Http2Driver  — scaffold (nghttp2 vcpkg manifest in place)     │
│   Http3Driver  — scaffold (ngtcp2 + nghttp3 in place)           │
├─────────────────────────────────────────────────────────────────┤
│ Connections (Connection concept)                                │
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

- **One-way dependencies, top to bottom.** Application doesn't know which driver served the request. Drivers don't know which controllers exist; they only know `Router&`. Listeners don't know which drivers run on them at the source level — pairing is enforced via templates.
- **Routes baked at startup, immutable at runtime.** `RouteRegistry::find_route()` returns a single composed callable — group-prefix-matched, middlewares pre-attached, Outcome→Response conversion already wired. Zero per-request route composition. Registration after `setup()` is `std::logic_error`.
- **`Server` is a thin orchestrator.** Owns the io_context, listeners, drivers (held inside listeners), the router, observer list, controller list. Doesn't loop sessions itself — drivers do that.
- **The build/buy line is the `HttpDriver::serve()` method.** Whatever happens inside `serve()` is the driver's problem. `Http11Driver::serve()` uses Beast and looks like a session loop. Future `Http2Driver::serve()` will wrap nghttp2; the impedance mismatch lives there, not in the public interface.
- **No virtual base for `Connection` or `HttpDriver`.** Concept-based duck typing; polymorphism only at the listener layer (where `Server` holds `unique_ptr<ListenerBase>`). No `dynamic_cast` anywhere in the runtime path.
- **Per-request arena + Beast-allocator-redirected.** Beast's `request_parser<..., pmr_allocator>` parses headers/path/body into our request-scoped `monotonic_buffer_resource`. `Headers` is a tagged-union facade that wraps Beast's parsed `fields` directly for incoming, owned arena strings for outgoing. ~1 heap alloc per request in the framework path; that one is the user-built response body string.

## 4. Directory Structure

Co-located per-thing directories. Each "thing" (a class, a function family, a small set) gets its own subdirectory containing both header and source. No separate `include/`/`source/` trees.

```
components/http/
├─ types/
│  ├─ request/           {request.hpp, request.cpp}
│  ├─ response/          {response.hpp, response.cpp}
│  ├─ headers/           {headers.hpp, headers.cpp}
│  ├─ body/              {body.hpp, body.cpp}
│  ├─ request_context/   {request_context.hpp, request_context.cpp}
│  ├─ response_factory/  {response_factory.hpp, response_factory.cpp}
│  ├─ errors/            {errors.hpp, errors.cpp}
│  ├─ async_outcome.hpp  {(header-only alias)}
│  └─ http_enums.hpp     {Protocol, HttpMethod, HttpStatus, HttpVersion}
├─ routing/
│  ├─ route_registry/    {route_registry.hpp, route_registry.cpp}
│  ├─ controller/        {controller.hpp, controller.cpp}
│  ├─ middleware/        {middleware.hpp, middleware.cpp}
│  ├─ group/             {group.hpp, group.cpp}
│  ├─ router/            {router.hpp, router.cpp}
│  └─ firewall/          {rate_limit.hpp, ip_rule.hpp}   # data types only
├─ connection/
│  ├─ connection.hpp     # the concept lives top-level; it's the layer's contract
│  ├─ request_arena/     {request_arena.hpp, request_arena.cpp}
│  ├─ tcp_connection/    {tcp_connection.hpp, tcp_connection.cpp}
│  ├─ tls_connection/    {tls_connection.hpp, tls_connection.cpp}
│  └─ quic_connection/   {quic_connection.hpp, quic_connection.cpp}    # scaffold
├─ drivers/
│  ├─ http_driver.hpp    # concept top-level
│  ├─ http11/            {http11_driver.hpp, http11_driver.cpp, http11_config.hpp}
│  ├─ http2/             {http2_driver.hpp, http2_driver.cpp, http2_config.hpp}    # scaffold
│  └─ http3/             {http3_driver.hpp, http3_driver.cpp, http3_config.hpp}    # scaffold
├─ listeners/
│  ├─ listener_base.hpp
│  ├─ connection_tracker/   {connection_tracker.hpp, connection_tracker.cpp}
│  ├─ tcp_listener/         {tcp_listener.hpp, tcp_listener.cpp}
│  ├─ tls_listener/         {tls_listener.hpp, tls_listener.cpp, build_ssl_context.cpp}
│  └─ quic_listener/        {quic_listener.hpp, quic_listener.cpp}    # scaffold
├─ server/
│  ├─ server/                {server.hpp, server.cpp}
│  └─ server_observer/       {server_observer.hpp, server_observer.cpp}
├─ config/
│  ├─ server_config/         {server_config.hpp, server_config.cpp}
│  ├─ listener_config/       {listener_config.hpp, listener_config.cpp}
│  ├─ tls_config/            {tls_config.hpp, tls_config.cpp}
│  ├─ timeouts/              {timeouts.hpp, timeouts.cpp}
│  └─ load_server_config/    {load_server_config.hpp, load_server_config.cpp}
├─ export/demiplane/http     # umbrella header
└─ tests/
```

CMake `target_include_directories` points at each layer's root (e.g. `types`), so includes from sister layers look like `<request/request.hpp>`, `<route_registry/route_registry.hpp>`. Within a layer, a `.cpp` includes its own header via `"foo.hpp"` (relative); cross-thing-within-the-same-layer includes use the rooted form for grep-ability.

## 5. Core Types

### 5.1 `Headers`

Multi-value capable, case-insensitive lookup, stable insertion order. Internal tagged-union backing: views into Beast's parsed `fields` for incoming requests, arena-owned strings for outgoing responses.

```cpp
class Headers {
    struct BeastBacking { const beast::http::fields* fields; };
    struct OwnedBacking {
        std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>> entries;
    };
    std::variant<BeastBacking, OwnedBacking> backing_;

public:
    void add(std::string_view name, std::string_view value);   // forces OwnedBacking
    void set(std::string_view name, std::string_view value);   // replaces all of name
    void remove(std::string_view name);

    std::optional<std::string_view> get(std::string_view name) const;
    std::string get_or(std::string_view name, std::string_view fallback) const;

    std::span<const std::string_view> get_all(std::string_view name) const;

    bool contains(std::string_view name) const;

    auto begin() const;   // iteration in insertion order
    auto end() const;
};
```

`std::visit` over the 2-element variant compiles to a tag check + direct call; modern compilers inline through it.

### 5.2 `Body` — streaming truth, buffered helpers

```cpp
class Body {
public:
    virtual ~Body() = default;

    // Streaming read — returns next chunk, std::nullopt when done. Drivers implement.
    virtual asio::awaitable<std::optional<std::span<const std::byte>>> read_chunk() = 0;

    // Set when Content-Length present; absent for chunked-encoded bodies.
    virtual std::optional<std::size_t> size_hint() const = 0;

    // Buffered helpers — built on read_chunk(). All take a limit; failing the
    // limit short-circuits with BodyLimitExceeded.
    AsyncOutcome<std::string, BodyLimitExceeded>
        read_to_string(std::size_t limit);
    AsyncOutcome<Json::Value, JsonParseError, BodyLimitExceeded>
        read_json(std::size_t limit);
    AsyncOutcome<std::pmr::unordered_map<std::pmr::string, std::pmr::string>,
                 FormParseError, BodyLimitExceeded>
        read_form(std::size_t limit);
    AsyncOutcome<std::vector<MultipartField>,
                 MultipartParseError, BodyLimitExceeded>
        read_multipart(std::size_t limit);    // real impl, not the current stub
};
```

Concrete implementations:
- `BeastRequestBody` — zero-copy span over Beast's parsed body buffer.
- `OwnedBufferBody` — for outgoing responses with a known string body.
- `StreamingProducerBody` — for outgoing responses built incrementally via a producer closure.
- `EmptyBody` — for `GET`, `HEAD`, `204` responses, etc. Inline-empty kind, 0 allocs.

`Body` is held inline as a value type with a small-buffer-optimized type-erased storage (~32 bytes inline, heap fallback only for `StreamingProducerBody`). No `unique_ptr<Body>` indirection.

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
    HttpStatus status   = HttpStatus::ok;
    HttpVersion version = HttpVersion::http_1_1;
    Headers headers;
    Body body;

    // Fluent setters for handler ergonomics
    Response& with_status(HttpStatus) &;            Response&& with_status(HttpStatus) &&;
    Response& with_header(std::string n, std::string v) &;
    Response&& with_header(std::string n, std::string v) &&;
    Response& with_body(std::string body) &;        Response&& with_body(std::string body) &&;
};
```

`HttpMethod`, `HttpStatus`, `HttpVersion`, `Protocol` are enums in `types/http_enums.hpp`. Drivers translate to/from wire format internally.

### 5.4 `RequestContext`

The handler's view of one in-flight request. Lazy header lookup, type-keyed data bag, pre-decoded path/query params from the request arena.

```cpp
class RequestContext {
public:
    // Request data
    HttpMethod method() const;
    std::string_view target() const;
    std::string_view path() const;          // pre-split, pre-decoded
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

private:
    Request request_;
    boost::container::small_flat_map<
        std::string_view, std::string_view, 4,
        std::less<>, std::pmr::polymorphic_allocator<...>> path_params_;
    boost::container::small_flat_map<...> query_params_;
    TypedBag bag_;   // arena-backed, SBO of 4 entries inline
};
```

The `TypedBag` is an arena-backed `type_index`-keyed store with small-buffer optimization for up to 4 entries inline; `std::any` is *not* used (it heap-allocates internally for non-SBO types). Setting and getting middleware-bag values is one arena bump for the value plus an inline key entry.

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

User-defined error types follow the same pattern. If a handler's `Outcome<Response, Errors...>` lists an error type without a `to_http_response` overload, the controller bind layer fails to instantiate — compile error pointing at the missing overload.

### 5.6 `AsyncOutcome` alias

```cpp
template <typename T, typename... Es>
using AsyncOutcome = boost::asio::awaitable<gears::Outcome<T, Es...>>;
```

Lives in `types/async_outcome.hpp`.

### 5.7 `ResponseFactory`

```cpp
class ResponseFactory {
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

    static Response stream(HttpStatus, Headers,
        std::function<asio::awaitable<void>(Body::Writer&)> producer);

    static Response custom(HttpStatus, std::string body,
                           std::string_view ct = "text/plain");
};
```

Factory does *not* set `Server` or `Date` headers — drivers stamp those uniformly right before write. Same handling across all protocol drivers, no helper to forget.

## 6. Connection + Driver Interface

### 6.1 Connection — concept, not class hierarchy

```cpp
namespace demiplane::http {

class RequestArena {
    std::array<std::byte, 8192> stack_buffer_;
    std::pmr::monotonic_buffer_resource resource_{
        stack_buffer_.data(), stack_buffer_.size()};
public:
    std::pmr::polymorphic_allocator<> allocator() { return {&resource_}; }
    void reset() { resource_.release(); }   // falls back to stack region
};

template <typename T>
concept Connection = requires (T& t, std::chrono::milliseconds ms) {
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
concept StreamConnection = Connection<T> && requires (T& t) {
    typename T::stream_type;
    { t.stream() } -> std::same_as<typename T::stream_type&>;
};

}  // namespace demiplane::http
```

Concrete connection types are value classes that compose a `RequestArena` and any per-transport state:

- `TcpConnection` — `asio::ip::tcp::socket` + arena + cancel signal.
- `TlsConnection` — `asio::ssl::stream<tcp::socket>` + arena + cancel signal + ALPN result.
- `QuicConnection` — scaffold only; ngtcp2 state when filled in.

### 6.2 HttpDriver — concept with templated `serve()`

```cpp
template <typename T>
concept HttpDriver = requires {
    { T::id() }             -> std::same_as<Protocol>;
    { T::accepted_alpns() } -> std::same_as<std::span<const std::string_view>>;
    // serve() is templated on connection type — checked at the listener level.
};
```

Each driver class has a templated `serve()` method. The connection type is deduced at the call site (in the listener), so duck typing on `.stream()` chooses Beast's stream type at compile time — no `dynamic_cast`, no per-byte vcall.

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

    template <StreamConnection ConnT>
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
            co_await write_response(stream, std::move(response), conn.cancel_slot());

            if (!response.keep_alive()) break;
        }
        co_await conn.async_close();
    }

private:
    Http11Config cfg_;
};
```

Key bug-fixes that land here:

- **Body limit + per-phase timeouts** real (`parser.body_limit`, `expires_after`).
- **URL decoding** in `build_request_context` (path/query into the arena before dispatch).
- **Handler exceptions → 500** via the catch-all around `router.dispatch`. Outcome-typed errors are already converted in `router.dispatch` — the catch-all handles only escapes that bypassed the Outcome system.
- **`Date` and `Server` headers** stamped in `stamp_common_headers` — one place, every protocol driver does it identically.
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

    template <StreamConnection ConnT>
        requires requires(ConnT c) { { c.is_secure() } -> std::same_as<bool>; }
    asio::awaitable<void> serve(ConnT& conn, Router& router) {
        // Scaffold — fill in with nghttp2.
        COMPONENT_LOG_WRN() << "Http2Driver::serve() not implemented";
        co_await conn.async_close();
    }
};

class Http3Driver { /* mirror, takes QuicConnection */ };
```

vcpkg manifest pulls `nghttp2`, `ngtcp2`, `nghttp3` so headers are available. When the implementations land later, they go inside `serve()` with no surrounding changes.

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

### 7.3 TcpListener / TlsListener / QuicListener

`TcpListener<Driver>` — single driver, no ALPN. `bind()` opens an acceptor, `run()` accept loop, each connection gets a tracker handle and a `TcpConnection`, calls `driver.serve(conn, router)`.

`TlsListener<Drivers...>` — tuple of drivers, ALPN selects at handshake. OpenSSL's `SSL_CTX_set_alpn_select_cb` picks from the union of advertised ALPN strings (accumulated across the driver tuple at compile time). After handshake, `dispatch_alpn` walks the tuple and calls the matching driver. Server preference order = driver template param order ("first listed wins on tie").

`QuicListener<Http3Driver>` — scaffold. `bind()` is a stub that succeeds, `run()` returns immediately. Constructor compile-time-asserts that the driver is `Http3Driver` (QUIC pairs with h3 only).

### 7.4 build_ssl_context

A helper in `listeners/tls_listener/build_ssl_context.cpp` that takes a `TlsConfig` and the union of advertised ALPN strings, returns a fully configured `asio::ssl::context`:

1. Construct with `tls_server` method.
2. Set min protocol version per `TlsConfig::min_version`.
3. Disable bad protocols/ciphers (`SSL_OP_NO_SSLv2 | NO_SSLv3 | NO_TLSv1 | NO_TLSv1_1 | NO_COMPRESSION | SINGLE_DH_USE`).
4. Set a hardened cipher list (modern defaults; configurable later).
5. Load cert chain + private key (with passphrase callback if set).
6. Optional DH params, optional client cert verification.
7. Configure session cache.
8. Set ALPN advertise + selection callback that walks client offers and picks the first server-advertised protocol.

## 8. Routing

### 8.1 Three-phase lifecycle

1. **Build** — controllers constructed, `configure_routes()` populates local registries, middleware lists filled.
2. **Bake** — `server.add_controller(...)` (or `in_group(...).add_controller(...)`) drains the controller's local registry into the server's, applying prefix + composing middleware chain + wiring Outcome→Response. Conflicts detected here, fail at setup if present.
3. **Frozen** — after `server.setup()`. All registries immutable. `find_route()` is the only operation. Any registration attempt is `std::logic_error`.

### 8.2 HttpController

```cpp
class HttpController : public std::enable_shared_from_this<HttpController> {
public:
    NEXUS_REGISTER(nexus::Resettable);

    virtual ~HttpController() = default;
    virtual void configure_routes() = 0;
    virtual void initialize() {}
    virtual void shutdown() {}

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
    template <typename F> requires Handler<F>
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

The verb-DSL templates capture the typed handler signature (which knows its `Outcome` error types) and produce a `bake` closure. At merge time, the closure runs and produces the final `ContextHandler` with Outcome→Response baked in via ADL `to_http_response`.

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

For each `E` in the `Errors...` pack, ADL must find `to_http_response(const E&)`. Missing overload = compile error pointing at the offending error type.

The `AsyncResponse` (no Outcome) variant is the same loop without the visit.

Middleware shape:

```cpp
using NextHandler = std::function<AsyncResponse(RequestContext)>;
using Middleware  = std::function<AsyncResponse(RequestContext, NextHandler)>;
```

Middleware can short-circuit (return without calling `next`), modify the context (`ctx.set<T>(...)` then call `next`), or wrap the response (`auto r = co_await next(ctx); modify(r); co_return r;`).

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

**No `std::regex`.** Parametric paths parse at registration into a vector of segments (literal or parameter). At lookup, split incoming path by `/` and walk both lists. URL-decode captured segments into the request arena (one bump per param).

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

`MethodNotAllowedError` carries the allowed-verb set; its `to_http_response` produces a 405 with the `Allow:` header populated.

### 8.6 Path normalization

Default policy: collapse trailing slash internally (`/users/` and `/users` are the same route), collapse multi-slash (`/users//42` → `/users/42`). Case preserved (RFC 3986).

Configurable via `ServerConfig::path_normalization`:
- `none` — exact match (original current-code behavior).
- `collapse_trailing_slash` — default.
- `collapse_multi_slash` — extends trailing-slash collapse with multi-slash collapse.

Applied at:
1. Controller-side `Get/Post/...` registration.
2. Group prefix concatenation.
3. `find_route()` lookup on incoming target.

### 8.7 Conflict detection

`RouteRegistry::add_route` checks before insert; on duplicate `(method, path)` builds a `RouteConflictError`. `freeze()` aggregates and returns them. `Server::setup()` throws a `RouteConflictAggregateError` containing all conflicts at once. Misconfigurations are visible immediately, not piecemeal.

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

Drivers see `Response` only — both routing-level errors and handler typed errors are already mapped here. The driver-level catch-all is for unexpected exceptions only.

## 9. Server Orchestration + Lifecycle

### 9.1 Server class

```cpp
class Server {
public:
    NEXUS_REGISTER(nexus::Immortal);

    explicit Server(ServerConfig cfg);
    ~Server();   // calls stop() if state_ == started; never throws

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    template <HttpDriver Driver>
    Server& add_tcp_listener(std::string bind, Driver driver);

    template <HttpDriver... Drivers>
    Server& add_tls_listener(std::string bind, TlsConfig tls, Drivers... drivers);

    template <HttpDriver Driver>
    Server& add_quic_listener(std::string bind, TlsConfig tls, Driver driver);

    template <std::derived_from<HttpController> C>
    Server& add_controller(std::shared_ptr<C> ctrl);

    GroupBinding in_group(std::string prefix);

    Server& add_observer(std::shared_ptr<ServerObserver> obs);

    void setup();   // bind listeners, spawn workers, freeze registry
    void start();   // blocks until SIGINT/SIGTERM/stop() and graceful shutdown completes
    void stop();    // request graceful shutdown; non-blocking, idempotent

    bool is_running() const noexcept;
    std::span<const std::unique_ptr<ListenerBase>> listeners() const;
    const ServerConfig& config() const noexcept;

private:
    enum class State : std::uint8_t { build, setup_done, started, stopping, stopped };
    std::atomic<State> state_{State::build};

    ServerConfig cfg_;
    asio::io_context ioc_;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> work_;
    std::vector<std::thread> workers_;

    std::vector<std::unique_ptr<ListenerBase>> listeners_;
    std::vector<std::shared_ptr<HttpController>> controllers_;
    std::vector<std::shared_ptr<ServerObserver>> observers_;

    RouteRegistry registry_;
    Router router_{registry_};

    asio::cancellation_signal shutdown_signal_;
    std::mutex shutdown_mutex_;
    std::condition_variable shutdown_cv_;
    bool shutdown_complete_ = false;

    asio::awaitable<void> graceful_shutdown();
    SCROLL_COMPONENT_PREFIX("Server");
};
```

### 9.2 ServerObserver

```cpp
class ServerObserver {
public:
    virtual ~ServerObserver() = default;

    virtual asio::awaitable<void> on_setup_complete() { co_return; }
    virtual asio::awaitable<void> on_shutdown_started() { co_return; }
    virtual void on_shutdown_complete() noexcept {}

    virtual void on_request(const RequestContext&) noexcept {}
    virtual void on_response(const RequestContext&, const Response&) noexcept {}

    // Captures std::exception_ptr (heap-managed) — fixes the original UAF.
    virtual void on_unhandled_exception(std::exception_ptr) noexcept {}
};
```

Single typed observer interface replaces the current 10 callback vectors. Per-request hooks are sync (request hot path doesn't await observer chains); shutdown hooks are async (shutdown awaits observer work).

`exception_ptr` instead of `const exception&` — heap-managed, has its own lifetime, safe to forward into spawned coroutines. The original UAF category is structurally impossible.

### 9.3 setup()

1. Validate state is `build`.
2. Validate at least one listener and `threads > 0`.
3. `registry_.freeze()` → if conflicts, throw `RouteConflictAggregateError`.
4. Call `bind()` on every listener synchronously. Bind failures throw.
5. Construct `work_` guard on the io_context.
6. Spawn `cfg_.threads` worker threads, each running `ioc_.run()`.
7. `co_spawn` each listener's `run(router_)` with `shutdown_signal_.slot()` as the cancellation slot.
8. Notify `on_setup_complete` observers (await as a barrier).
9. Set `state_` to `setup_done`.

### 9.4 start()

1. CAS state from `setup_done` to `started`.
2. Set up `signal_set` for SIGINT/SIGTERM that calls `stop()`.
3. Block main thread on `shutdown_cv_.wait()`.
4. Once notified, join all worker threads.
5. Set state to `stopped`.

### 9.5 stop()

Idempotent. CAS state to `stopping`. `co_spawn(graceful_shutdown(), detached)`. Returns immediately.

### 9.6 graceful_shutdown()

```cpp
asio::awaitable<void> Server::graceful_shutdown() {
    // Phase 1: cancel accept loops (new connections refused)
    shutdown_signal_.emit(asio::cancellation_type::terminal);

    // Phase 2: drain in-flight requests up to drain_timeout
    auto drain_deadline = std::chrono::steady_clock::now() + cfg_.drain_timeout;
    for (auto& l : listeners_) co_await l->drain_until(drain_deadline);

    // Phase 3: notify async shutdown observers (BEFORE work guard release —
    // io_context still running, so observers can do real work)
    for (auto& obs : observers_) {
        try {
            co_await obs->on_shutdown_started();
        } catch (...) {
            for (auto& other : observers_)
                other->on_unhandled_exception(std::current_exception());
        }
    }

    // Phase 4: controller shutdown (sync, reverse-add order)
    for (auto it = controllers_.rbegin(); it != controllers_.rend(); ++it) {
        try {
            (*it)->shutdown();
        } catch (...) {
            for (auto& obs : observers_)
                obs->on_unhandled_exception(std::current_exception());
        }
    }

    // Phase 5: notify shutdown_complete observers (sync, noexcept)
    for (auto& obs : observers_) obs->on_shutdown_complete();

    // Phase 6: release work guard. Workers' ioc.run() returns once queues drain.
    // We do NOT call ioc.stop() — that would discard pending work.
    work_.reset();

    // Phase 7: signal start() to unblock and join workers
    {
        std::lock_guard lk{shutdown_mutex_};
        shutdown_complete_ = true;
    }
    shutdown_cv_.notify_all();
}
```

The phase ordering is the explicit fix for the original review's C2 (async stop callbacks dropped). Observers run on a still-running `ioc_`; only after they're done do we release the work guard.

## 10. Configuration

All config types inherit `serialization::ConfigInterface<Self, Json::Value>`, declare fields via static `fields()` tuple, ship a fluent `Builder`, use a private default constructor for the framework. Same shape as the project's existing `FileSinkConfig`. Validation via `validate()`. Errors surface through Outcome at load time.

### 10.1 Config types

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

`firewall.hpp` types (`rate_limit`, `ip_rule`) move to `routing/firewall/` as data-only inputs to user-written rate-limit middleware. Not consumed by core.

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

Implementation:
1. Open and read file → on failure, `ConfigFileError`.
2. Parse JSON via the existing `Json::CharReader` pipeline → on failure, `ConfigParseError` with line info.
3. Walk `ServerConfig::fields()` via the framework's parse machinery — missing required field or type mismatch → `ConfigSchemaError` with the YAML-pointer-style `field_path` (e.g. `/listeners/1/tls/cert_file`).
4. Call `validate()` on populated config. `std::invalid_argument` → caught, rethrown as `ConfigSchemaError`.

### 10.3 Wiring config to runtime

`ServerConfig` is the input to `Server`'s constructor. Listeners aren't auto-spawned from config alone — driver instances with their per-protocol config come from code:

```cpp
auto cfg = load_server_config("server.json");
if (!cfg) { /* report and exit */ }

Server server{cfg.value()};

attach_default_listeners(server);   // helper for the common case

auto users = std::make_shared<UserController>(user_service);
add_basic_middleware(*users, log_mw, request_id_mw);
server.in_group("/api/v1").add_controller(users);

server.setup();
server.start();
```

`attach_default_listeners(Server&, DefaultDrivers={})` walks `cfg.listeners()`, dispatches by transport/protocol set, constructs the right driver instances using server-level timeouts/body_limit. For per-listener tuning, the user iterates manually.

### 10.4 YAML follow-up (out of v1 scope)

Same `ConfigInterface` pattern; new format type parameter (`Yaml::Value` or whatever the eventual lib uses). Config types themselves don't change. Strictly additive.

### 10.5 Hot-reload removed

`reload_if_changed` is removed (not stubbed). Listeners can't hot-rebind without socket churn; routes are baked at startup; timeouts are baked into driver configs; threads/arena can't change without restart. The honest answer: config changes require server restart in v1. systemd/k8s-orchestrated graceful shutdown + relaunch is the deployment pattern; the `drain_timeout`-driven graceful shutdown makes this transparent to clients.

## 11. Allocation Strategy (Performance Notes)

### 11.1 Per-request hot path

Target: framework-internal hot path stays at ~zero heap allocations per request.

For one `GET /users/42` with 6 headers and 0-byte body:

| Source | Heap allocs |
|---|---|
| Receive buffer (`beast::flat_buffer`) | 0 (per-connection reuse) |
| Beast parsing into `fields` | 0 (allocator points at request arena) |
| `Headers` (BeastBacking view) | 0 |
| `Body` (zero-copy span over Beast's body buffer) | 0 |
| Path/query params (arena `small_flat_map` + URL-decoded views) | 0 |
| `RequestContext::TypedBag` (untouched in this path) | 0 |
| Response body (`std::string` user-built) | 1 — fundamental, user data |
| Response headers (3 entries, arena) | 0 |

**Result: ~1 alloc per request, owned by user-built data.**

### 11.2 Per-connection

- `RequestArena` initial buffer: 8KB on the stack (configurable via `ServerConfig::request_arena_size`). Grows on demand if a request exceeds it.
- `beast::flat_buffer` reused across requests on a keep-alive connection.
- Per-connection cancel signal lives in `ConnectionTracker`'s list.

### 11.3 What remains user-controllable

Response body content: if a handler builds JSON via `Json::Value::toStyledString()`, that's the user's allocation chain. Framework moves it into the `Response`. For large payloads, the user can opt into `ResponseFactory::stream(...)` — the producer closure writes chunks directly to the Body::Writer the driver hands it; no buffering buried in the framework.

## 12. Migration Plan

### 12.1 File-by-file

| Path (current) | Disposition |
|---|---|
| `components/http/CMakeLists.txt` | Replaced — see §13 |
| `components/http/config/include/firewall_config.hpp` | Moved to `routing/firewall/`. `rate_limit`/`ip_rule` kept as data-only types. Not consumed by core. |
| `components/http/config/include/router_config.hpp` | Deleted. `server` struct, `route_table`, `route_flags`, `load_from_yaml`, `dump_to_yaml`, `reload_if_changed` all gone. Replaced by `ServerConfig` (§10). |
| `components/http/config/include/tls_config.hpp` | Replaced by `config/tls_config/`. Type renamed (was `tls_settings`), now `TlsConfig` with `ConfigInterface`. |
| `components/http/export/demiplane/http` | Kept as umbrella header; aggregate `#include`s updated. |
| `components/http/http_server/include/aliases.hpp` | Deleted. Beast types no longer leak. |
| `components/http/http_server/include/controller.hpp` | Replaced by `routing/controller/`. |
| `components/http/http_server/include/request_context.hpp` | Replaced by `types/request_context/`. |
| `components/http/http_server/include/response_factory.hpp` | Replaced by `types/response_factory/`. |
| `components/http/http_server/include/route_registry.hpp` | Replaced by `routing/route_registry/`. |
| `components/http/http_server/include/server.hpp` | Replaced by `server/server/`. |
| `components/http/http_server/source/*.cpp` | All deleted; replaced by per-layer source files. |
| `tests/manual_tests/http/handler/manual_example_http_handler.cpp` | Reworked into integration tests under `tests/integration_tests/http/`; example pattern stays available under `examples/http/`. |
| `benchmarks/http/*` | Updated to compile against new types. Benchmark logic intact. |

### 12.2 Phased PRs

| PR | Scope | Self-contained? |
|---|---|---|
| 1 | **Types layer** — `Request`, `Response`, `Headers`, `Body`, `RequestContext`, `errors.hpp`, `ResponseFactory`, `AsyncOutcome`, `http_enums.hpp`. + unit tests. | Yes — no driver, no server, just types. |
| 2 | **Routing layer** — `RouteRegistry`, `HttpController` evolution, `Middleware` shape, `GroupBinding`, `Router`, the bake step, conflict detection. + unit tests. Old `Server` still wires it. | Yes — registry/controller logic testable with synthetic `RequestContext`s. |
| 3 | **Connection + Driver interface + `Http11Driver`** — concept-based connections, templated `serve()`, bug-fix battery (URL decode, body limits, timeouts, exception → 500). h2/h3 scaffolds. vcpkg deps for nghttp2/ngtcp2/nghttp3 added. + driver-level tests. | Yes — drivers testable against fake connections. |
| 4 | **Listeners + TLS** — `ListenerBase`, `TcpListener`, `TlsListener`, `QuicListener` scaffold, `ConnectionTracker`, `build_ssl_context`. + integration tests for h1-over-TCP and h1-over-TLS. | Yes — first end-to-end testable layer. |
| 5 | **`Server` rewrite + observers + lifecycle** — three-state lifecycle, `setup`/`start`/`stop`, `graceful_shutdown`, observer interface. + lifecycle integration tests. | Yes — replaces old `Server`; architecture goes "live" here. |
| 6 | **Config (JSON + `ConfigInterface`)** — all config types, `load_server_config`, `attach_default_listeners` helper. + config tests. | Yes — JSON-loaded server is then a one-liner. |
| 7 | **Cleanup** — delete `aliases.hpp`, old `server.hpp`/`controller.hpp`/etc., port manual test, update benchmarks. | Yes — the deletions PR. |

Each PR builds and tests cleanly. The old code coexists with the new through PRs 1-6; PR 7 is the cleanup.

## 13. CMake Reorganization

Each layer becomes its own static library; the public umbrella aggregates them via the existing `add_combined_library` macro. Skeleton:

```cmake
set(DMP_HTTP ${DMP_COMPONENT}.Http)

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
    Demiplane::Common::Nexus
    Boost::beast Boost::asio Boost::container
    JsonCpp::JsonCpp)

# Routing
add_library(${DMP_HTTP}.Routing STATIC
    routing/route_registry/route_registry.cpp
    routing/controller/controller.cpp
    routing/group/group.cpp
    routing/middleware/middleware.cpp
    routing/router/router.cpp)
target_include_directories(${DMP_HTTP}.Routing PUBLIC routing)
target_link_libraries(${DMP_HTTP}.Routing PUBLIC ${DMP_HTTP}.Types)

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

- `route_registry_test.cpp` — add/freeze/lookup; conflict detection with batched aggregation; 404 vs 405 with Allow header; parametric segment matching; URL-decoded param capture; path normalization (trailing slash, multi-slash, case preservation).
- `headers_test.cpp` — multi-value get/get_all, case-insensitive lookup, `add` vs `set` semantics, BeastBacking ↔ OwnedBacking interchange.
- `body_parsing_test.cpp` — `read_to_string` / `read_json` / `read_form` / `read_multipart` against synthetic bodies, including limit-exceed paths, malformed payload paths, URL decoding edge cases (`+`, `%XX`, invalid escapes).
- `outcome_to_response_test.cpp` — every built-in error type round-trips through `to_http_response`; user-defined error type with custom conversion compiles and dispatches correctly; missing conversion produces a clear compile error.
- `request_context_test.cpp` — set/get/has type-keyed bag, type collisions, params accessed before set return nullopt.
- `config_load_test.cpp` — valid JSON loads; missing required field surfaces field path; type mismatch surfaces field path; round-trip via `dump_server_config` produces equivalent config; enum string mappings round-trip.

### 14.2 Integration tests (`tests/integration_tests/http/`)

Per project preference: real systems, no mocks. Each test binds `127.0.0.1:0`, captures `bound_port()`, issues real HTTP via Beast client, asserts on the wire response, then `stop()`s and joins.

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
- Lifecycle: `setup()` failure on port-in-use; `start()` blocks until `stop()`; SIGINT triggers graceful shutdown; `stop()` idempotent; registration after `setup()` throws.
- Graceful shutdown: in-flight completes; new connections refused; `on_shutdown_started` runs and is awaited; drain timeout force-cancels remaining.
- Concurrency: 1000 concurrent requests across N workers; correct responses; clean under TSan.
- TLS: handshake against self-signed cert (test fixture); ALPN negotiates h1; client offering only `h2` + listener without h2 driver → connection closes.
- Body streaming: `Response::stream(...)` correct chunked transfer; client receives chunks.
- Body large: 16 MB streamed download succeeds; 17 MB request → 413.
- Observer: `on_request` / `on_response` fire in order; `on_unhandled_exception` fires for non-Outcome handler throws.

### 14.3 Out of scope for v1 tests

- h2 / h3 wire-level tests (drivers are stubbed).
- TLS edge cases (cert chain validation, OCSP, session resumption).
- Performance regression tests (separate benchmarks).
- Hot-reload (out of scope).

## 15. Decisions Log

| Decision | Choice | Why |
|---|---|---|
| Build vs. buy h2/h3 | Hybrid: build h1 (Beast), buy h2 (nghttp2) and h3 (ngtcp2 + nghttp3); v1 ships h1 only with stubs | Matches user intent to write h2/h3 themselves later; nglibs reduce attack surface and bugs |
| Body model | Streaming truth, buffered convenience helpers | Maps to all three protocols; ergonomic for JSON-API common case; opt-in streaming for large payloads |
| Driver interface | Single coroutine `serve(Connection&, Router&)` | One method per driver; protocol complexity stays inside; coroutine-native consistent with rest of codebase |
| Listener model | ALPN-multiplexed TLS + separate UDP listener for h3 | Standard production pattern; h1+h2 share port 443 via ALPN; h3 is separate transport entirely |
| Routing API | Evolve B: keep controller-merge, add prefix + middleware + verbs | Controller-merge concept is the strongest part of current design; preserve and extend |
| Group/middleware scope | Controller-only middleware (option A); group via `server.in_group(prefix).add_controller(...)` | User wants explicit control; repetition resolved via free helper functions |
| Error model | `gears::Outcome<Response, Errors...>` + `catch(...) → 500` | Project's existing Outcome type; multiple typed errors per handler; catch-all for unexpected only |
| Error → Response | ADL `to_http_response(const E&)` | Compile-time checked (missing = build break); zero overhead; lives next to error type |
| Build-out scope | Option C: everything except protocol drivers | TLS interface + impl, ALPN real, body limits, timeouts, graceful shutdown, JSON config; only h2/h3 protocol bodies stay stubbed |
| Allocation strategy | Per-request arena + Beast allocator redirect + Headers tagged-union facade | Zero framework heap allocs on hot path; user-data allocs unchanged |
| Connection abstraction | Concept-based, no virtual hierarchy, no `dynamic_cast` | Concrete connection types satisfy `Connection` concept; drivers templated on connection type; polymorphism only at listener layer |
| Lifecycle | `setup()` (sync bind) / `start()` (block) / `stop()` (non-blocking, idempotent) | Honest failure surface; testable; signal-driven shutdown coexists with programmatic |
| Observer model | Single typed `ServerObserver` interface with `exception_ptr` for errors | Replaces 10-vector mess; UAF structurally impossible (heap-managed exception_ptr) |
| Routing perf | Segment-vector parametric routing, not `std::regex` | Original design's headline perf bug; segment vectors are 10x+ faster, simpler, swap-in trie later if needed |
| Config format | JSON via `ConfigInterface` for v1; YAML follow-up | Project's established pattern (FileSinkConfig); YAML adds the format param later |
| Hot-reload | Removed (not stubbed) | Genuinely hard; restart-on-change is honest; revisit when production deployment demands it |
| Directory structure | Co-located per-thing dirs (`types/request/{hpp,cpp}`), no include/source split | User preference; cleaner colocation |

## 16. Open Questions

None blocking implementation. Things to revisit when their layer is touched:

- **`std::pmr::polymorphic_allocator<>`'s exact integration with Beast's `Allocator` template parameter** — should be straightforward (`pmr::polymorphic_allocator<char>` is allocator-compliant) but warrants a small spike at PR 3 start.
- **Enum string-mapping in `ConfigInterface` JSON** — depends on what `serialization::` already supports. If int-only, that's fine for v1; user JSON files use ints; we upgrade to string-encoded later as a transparent format change.
- **`small_flat_map` + `pmr::polymorphic_allocator` interaction** — Boost.Container supports custom allocators; the exact instantiation pattern needs verification at PR 1 start.
- **OpenSSL ALPN callback context lifetime** — the `arg` parameter must outlive the SSL context. Storing it as a member of `TlsListener` should suffice; verify with a brief test at PR 4 start.

## 17. References

- Existing `components/http/` source tree (the thing being replaced)
- `common/scroll/sink/file_sink/include/file_sink_config.hpp` — reference pattern for `ConfigInterface` usage
- `common/gears/outcome/gears_outcome.hpp` — reference for `gears::Outcome` API
- Code review feedback from `component/http-1.1/v1.1` branch ultrareview
