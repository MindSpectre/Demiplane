# HTTP Redesign — PR 6: Config Layer (JSON + ConfigInterface) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the landed PR 1–5 server JSON-configurable: rewrite the staged plain-struct `TlsConfig`/`ServerConfig`
in place as `serialization::ConfigInterface` types, add `Timeouts` + `ListenerConfig`, implement
`load_server_config(path)` → `gears::Outcome<ServerConfig, ConfigFileError, ConfigParseError, ConfigSchemaError>` +
`dump_server_config`, and the `attach_default_listeners(Server&)` helper that turns a JSON file into live listeners —
plus resolution of every staged `PR 6` marker (TlsListener arena wiring, `build_ssl_context` cipher-list TODO). Spec:
§10 + §12.2 PR 6 of `docs/superpowers/specs/2026-05-07-http-redesign-design.md`; §14.1 `config_load_test` battery;
§16 open question "ConfigInterface field-type coverage" (resolved here — the machinery needed repair, see D1).

**Architecture:** Config types follow the `FileSinkConfig` reference pattern exactly: `final` classes deriving
`serialization::ConfigInterface<Self, Json::Value>`, a static `fields()` tuple, a private framework default ctor
(`friend class ConfigInterface`), a nested fluent `Builder` whose `finalize() &&` runs `validate()`, and (where an
escape hatch is warranted) a public full constructor that does NOT validate. Enum fields serialize as **strings** via
ADL-found `read_field`/`write_field` overloads declared next to each enum in `demiplane::http` (the machinery's
unqualified calls pick them up by ADL; unknown strings **throw** rather than silently defaulting). `load_server_config`
is the first real consumer of the (previously never-instantiated) machinery: file → jsoncpp `parseFromStream` →
`ServerConfig::deserialize<Json::Value>` inside a try/catch that maps `Json::Exception` (type mismatch) and
`std::invalid_argument` (enum codec / `validate()`) onto `ConfigSchemaError`. `attach_default_listeners` lives in the
**server layer** (it needs `Server`; config stays below it) and maps each `ListenerConfig` onto the landed
`add_tcp_listener`/`add_tls_listener`/`add_quic_listener` templates, deriving `Http11Config` from the server-level
`timeouts`/`body_limit`.

**Tech Stack:** C++23 (deducing-this Builders, concepts, `if constexpr`), `serialization::ConfigInterface` +
`Field<ptr, "name", policy>` + `read_field`/`write_field` JSON codecs, jsoncpp (`Json::CharReaderBuilder`,
`parseFromStream`, `Json::Exception`), `gears::Outcome`, the landed PR 1–5 dotted leaf targets, GoogleTest
(`add_unit_test` + `add_integration_test` on real `127.0.0.1:0` sockets via the PR 4/5 fixtures).

## Global Constraints

- Verify every task with the **`debug`** preset: `cmake --build build/debug --target <target> -- -j4` clean, then
  `ctest --test-dir build/debug --output-on-failure -R <pattern>`. The final sweep also builds the full tree.
- **No git commits by the executing agent** — the user manages git. Per-task `git` blocks below are the recommended
  grouping for when the user commits. **Never add a Claude/Co-Authored-By signature.**
- Dotted CMake target names only (`Demiplane.Component.HTTP.*`, `Demiplane.Tests.Unit.*`); **no new `::` aliases**;
  the public umbrella (`add_combined_library` in `components/http/CMakeLists.txt`) stays untouched until PR 7.
- Changes to `common/serialization` must keep the existing consumers **source-compatible**: `FileSinkConfig` (and the
  other scroll configs) are not edited, and the full `Demiplane.Tests.Unit.Scroll` suite must stay green (Task 1
  verifies).
- Config loading/dumping is the **cold path** — global-heap allocation is fine here. The zero-additional-allocation
  invariant (spec §11) is untouched: nothing in this PR runs per-request. Do not add hot-path branches.
- Keep `constexpr` on config methods even where they cannot constant-evaluate end-to-end (P2448 makes this valid;
  landed `FileSinkConfig` precedent and explicit user preference — partial constexpr folding of literal-input
  subexpressions).
- Builder setters are `noexcept` even where they move allocating types — match `FileSinkConfig::Builder` verbatim
  (recorded project preference: `bad_alloc` is treated as terminate-worthy, so `noexcept` on allocating
  constructors/setters is intentional).
- 4-space indent, `/** */` doc comments, `[[nodiscard]]` on accessors — match the landed HTTP sources. C++23
  **headers**, not modules. Cross-leaf includes use the rooted form (`<tls_config.hpp>`, `<server.hpp>`), never
  relative paths.
- Integration tests bind `127.0.0.1:0`, use real Beast clients, no mocks (project testing philosophy).
- Do not run TSan by default; note for the record: 3 pre-existing TSan races live in Nexus ctor/dtor and fire on any
  component's first `COMPONENT_LOG_*` — they are NOT config-layer regressions.

---

## Reconciliation against the landed code + spec (read before executing)

Spec §10 was written before PR 4/5 landed and before the serialization machinery was ever exercised. These are the
facts on the ground and the deviations this plan deliberately takes.

### Landed facts (authoritative over the spec)

1. **The `ConfigInterface` machinery has never been instantiated.** `grep` finds zero calls to
   `serialize<Json::Value>()`/`deserialize<Json::Value>()` outside `common/serialization` itself. Three latent
   compile bugs follow (they simply never surfaced):
   - **Two-phase lookup cannot reach the format overloads at all.** The machinery's `read_field`/`write_field` calls
     are *dependent* (their first argument is `Format&`), so ordinary lookup is frozen at the template **definition
     point** — and `config_interface.hpp` includes no format header (consumers include `<config_interface.hpp>`
     *before* `<json/json.hpp>`, so nothing is visible there either). At instantiation only **ADL** remains, and the
     associated namespaces are `Json` (from `Json::Value`) + the value type's own: for `int`/`bool`/`std::string`/
     `std::size_t`/`std::chrono::duration`/`std::vector<enum>` that set never contains `demiplane::serialization`,
     so the `json.hpp` overloads are **unfindable** → ill-formed. (Nested `ConfigInterface`-derived fields would be
     found — a base class's namespace is associated — which is exactly why this half-works on paper and went
     unnoticed.) The fix: the extension point's key parameter becomes a `serialization::FieldName` wrapper type, so
     `demiplane::serialization` is an associated namespace of **every** call.
   - `serialize_one_field`/`deserialize_one_field` pass `F::name.view()` (a `std::string_view` —
     `gears::FixedString::view()`, `gears_strings.hpp:28`) where the overloads take `const std::string&`.
     `std::string`'s ctor from `string_view` is **explicit** → no implicit conversion → ill-formed even where ADL
     would have found an overload. (Dissolved by the same `FieldName` change — it carries the `string_view`.)
   - `deserialize_one_field` (`config_interface.hpp:69`) does `ValueType val{};` — for a field whose type is another
     `ConfigInterface` config (private framework default ctor, `friend` = only ITS OWN `ConfigInterface`
     specialization), that default-construction is **inaccessible** → ill-formed. Same trap in `read_field` for
     `std::optional<T>` (`json.hpp:90`: `T inner{};`).
2. **`read_field`/`write_field` coverage** (`common/serialization/formats/json/json.{hpp,cpp}`): `std::string`,
   `string_view` (write-only), `int`, `std::size_t`, `bool`, `double`, `std::filesystem::path`,
   `std::map<string,string>`, `Json::Value` passthrough, `std::chrono::duration<Rep,Period>` (Int64 count), enums
   (generic template, **int-encoded**), `std::optional<T>`, nested `HasFields` types. **Missing for §10.1:**
   `std::vector<T>` (→ `listeners`, `protocols`) and `std::uint16_t` (→ `port`; the read side has no overload a
   `uint16_t&` can bind to).
3. **Reference pattern** = `common/scroll/sink/file_sink/include/file_sink_config.hpp`: `final` class,
   `ConfigInterface<Self, Json::Value>` base, `constexpr` full ctor (escape hatch, no validation), `constexpr
   validate() const override` throwing `std::invalid_argument`, `[[nodiscard]] constexpr` accessors, static
   `constexpr fields()` tuple of `serialization::Field<&Self::member_, "json_name"[, FieldPolicy]>`, `class Builder;`
   declared in-class + defined after (deducing-this setters, `finalize() &&` = validate + move out, members
   `friend class FileSinkConfig; friend class ConfigInterface; Self config_;`), private `constexpr Self() = default;`
   after `friend class ConfigInterface;`. Includes: `<config_interface.hpp>`, `<json/json.hpp>`.
4. **Staged plain structs at their final paths** (rewritten in place by this PR, includes stay stable):
   - `components/http/config/tls_config/tls_config.hpp` — `TlsConfig{cert_file, key_file, key_passphrase,
     dh_params_file, ca_file, min_version, session_cache, require_client_cert}` + nested `MinVersion{tls12,tls13}`.
     Field-access consumers: `listeners/tls_listener/build_ssl_context.cpp` (all 8 fields),
     `tests/unit_tests/http/listeners/test_build_ssl_context.cpp` (2 sites), `test_tls_listener.cpp:18`
     (`TlsConfig tls;` — deliberately empty), `test_quic_listener.cpp:21` (`TlsConfig{}`),
     `tests/integration_tests/http/test_http_tls.cpp:98-100`.
   - `components/http/config/server_config/server_config.hpp` — `ServerConfig{request_arena_size, drain_timeout,
     path_normalization}` + nested `PathNormalization{none, collapse_trailing_slash, collapse_multi_slash}`.
     Consumers: `server/server/server.hpp:106` (`cfg_.request_arena_size`), `server/server/server.cpp:40`
     (`map_normalization(cfg.path_normalization)` — reads the **moved-from parameter**, see D8), `server.cpp:220`
     (`cfg_.drain_timeout`), `server.cpp:343-350` (`map_normalization` switch),
     `tests/integration_tests/http/server_test_fixture.hpp:86` (`ServerConfig cfg = {}` default arg),
     `test_http_server_lifecycle.cpp:72,89,114,125,172-173`, `test_http_server_concurrency.cpp:58`,
     `test_http_run_standalone.cpp:57,131`. No benchmark/example consumers exist.
5. **Server build-phase API** (landed, PR 5 D7): `add_tcp_listener(std::string host, std::uint16_t port, Driver)`
   (forwards `cfg_.request_arena_size`), `add_tls_listener(std::string host, std::uint16_t port, TlsConfig,
   Drivers...)` (arena NOT wired — the staged PR 6 note at `server.hpp:112-114`), `add_quic_listener(host, port,
   TlsConfig, Driver)`, `config()` → `const ServerConfig&`, `in_group`/`add_controller`/`add_observer`. All throw
   `std::logic_error` after `setup()`.
6. **Listeners:** `TcpListener<Driver>{exec, host, port, driver, arena_size = 8192}` (arena already wired).
   `TlsListener<Drivers...>{exec, host, port, TlsConfig, drivers...}` — **no arena parameter**; its accept loop
   constructs `TlsConnection{sock, *ctx_}` and `TlsConnection` already accepts `arena_size = 8192` as third ctor arg
   (`tls_connection.hpp:35-39`), so wiring is ctor-plumbing only. `QuicListener<Http3Driver>{exec, host, port,
   TlsConfig, driver}` — scaffold, no connections, stays arena-less. Doc markers to resolve: `tls_listener.hpp:47-48`,
   `server.hpp:112-114`.
7. **Drivers:** `Http11Driver{const Http11Config&}` (explicit, noexcept). `Http11Config{max_header_bytes = 16 KB,
   max_body_bytes = 16 MB, header_timeout = 10 s, body_timeout = 30 s, idle_timeout = 60 s}` — plain struct, stays a
   plain struct (per-driver tuning, not JSON-loaded in v1; `attach_default_listeners` fills it from `ServerConfig`).
   `Http2Driver`/`Http3Driver` are default-constructible scaffolds (h2 `serve()` warns + closes). The h1 driver
   already replies **431** on `header_limit` and **413** on `body_limit` *before routing* — the integration test hook
   for proving `body_limit` reached the driver from JSON.
8. **`gears::Outcome<T, Es...>`** (`common/gears/outcome/gears_outcome.hpp`): implicit ctor from `T` and from
   `gears::err(E)`; `is_success()`/`is_error()`/`explicit operator bool`; `value()` (throws `BadOutcomeAccess` on
   error state); `holds_error<E>()`; `error<E>()` (throws `std::bad_variant_access` on wrong alternative).
9. **jsoncpp** (vcpkg): `Json::CharReaderBuilder` + `Json::parseFromStream(builder, istream, &root, &errs)`;
   `errs` formats as `"* Line N, Column M\n  <detail>"`. `asString()`/`asInt()`/`asUInt64()`/… throw
   `Json::LogicError` (⊂ `Json::Exception` ⊂ `std::exception`) on unconvertible types. `Json::Value` has
   `operator==` (round-trip equality checks) and move semantics.
10. **CMake conventions:** per-thing leaf targets owning their include dir (`add_library(<leaf> STATIC x.cpp)` or
    `INTERFACE` for header-only + `target_include_directories(... ${CMAKE_CURRENT_SOURCE_DIR})`), one INTERFACE
    aggregate per layer (`${DMP_HTTP}.Config` currently = TlsConfig + ServerConfig leaves; `${DMP_HTTP}.Server` =
    Observer + Core + RunStandalone). Serialization: `Demiplane::Common::Serialization` (combined lib: `.Config`
    header-only + `.Formats.Json` STATIC linking `JsonCpp::JsonCpp`).
11. **Test harness:** `add_unit_test(<target> <sources…>)` + separate `target_link_libraries(… ${TEST_LIBS})`
    (`TEST_LIBS` = gtest/gtest_main/gmock/gmock_main). HTTP test dirs are registered in `tests/CMakeLists.txt` under
    `if (BUILD_HTTP)`; common-layer unit targets are defined inline in `tests/unit_tests/CMakeLists.txt` (always
    built — the `tsan` preset builds `common/` only, so the Task 1 machinery test runs there too). Existing binaries:
    `Demiplane.Tests.Unit.Http.{Types,Routing,Connection,Drivers,Listeners}`,
    `Demiplane.Tests.Integration.Http.{Tcp,Tls,Server}`.
12. **Integration fixtures** (`tests/integration_tests/http/`): `ServerIntegrationFixture`
    (`server_test_fixture.hpp`, namespace `http_it`) — `start_server(configure, ServerConfig cfg = {},
    io_threads = 1)` runs build phase + workers + `setup()`; `port()` = first listener's `bound_port()`; `TearDown`
    runs the §9.7 stop→wait→teardown sequence. `TcpClient` (`http_test_fixture.hpp`, namespace `http_it`):
    `send(verb, target, body = {}, content_type = "text/plain", keep_alive = false)` → `ParsedResponse`
    (`bhttp::response<bhttp::string_body>`). `TlsClient` is **file-local** to `test_http_tls.cpp:43-90` (Task 9
    extracts it to a shared header). Self-signed cert PEMs + `write_temp(stem, contents)` live in
    `test_tls_cert.hpp` (namespace `http_tls_test`).
13. **Staged `PR 6` markers** (complete list, all resolved by this plan): `config/tls_config/tls_config.hpp:10-14`
    (header comment), `config/server_config/server_config.hpp:12-16,29-31` (header comments),
    `components/http/CMakeLists.txt` config-section comment, `config/CMakeLists.txt:1-5` (header comment),
    `listeners/tls_listener/tls_listener.hpp:47-48` (arena note), `server/server/server.hpp:112-114` (arena note),
    `listeners/tls_listener/build_ssl_context.cpp:48-49` (`TODO(PR6)` cipher-list check),
    `drivers/http11/http11_config.hpp:8-10` (comment), `routing/route_registry/route_registry.hpp:24` (comment),
    `connection/request_arena/request_arena.hpp:14` (comment references `ServerConfig::request_arena_size` — becomes
    an accessor).

### Deviations taken (each documented in the relevant task)

- **D1 — The serialization machinery is repaired and extended in `common/serialization`, gated by its own unit
  target.** §16 said "verify — and extend the serialization layer if needed — at PR 6 start"; the verification found
  it never compiled when instantiated (landed fact 1). Fixes: the extension-point key becomes
  **`serialization::FieldName`** (a `string_view` wrapper in `field.hpp`) so ADL reaches the format overloads from
  the machinery's dependent calls regardless of include order — every `read_field`/`write_field` signature changes
  from `const std::string& key` to `FieldName key` (legal to change: the extension point has zero external callers,
  landed fact 1); make `deserialize_one_field` read **directly into `builder.config_.*F::ptr`** (kills the
  `ValueType val{}` default-construction requirement — nested configs keep their private framework ctors — and
  preserves declared defaults on missing keys); `read_field(optional<T>)` gains a `HasFields<T>` branch that
  constructs via `T::deserialize`; NEW generic `std::vector<T>` codec (elements via `T::deserialize`/`serialize` for
  `HasFields`, via a wrap-object trick reusing every scalar/enum overload otherwise; non-array JSON throws
  `std::invalid_argument`); NEW `std::uint16_t` codec with a 0–65535 range check. Existing configs (`FileSinkConfig`
  et al.) are source-compatible: they only declare `fields()`/Builders and never call the codecs directly; same
  missing-key-keeps-default semantics.
- **D2 — HTTP config enums encode as strings** (`"tcp"`, `"tls13"`, `"http1"`, `"collapse_trailing_slash"`) via
  non-template ADL `read_field`/`write_field` overloads declared next to each enum. Satisfies §14.1 "enum string
  mappings round-trip"; supersedes §16's "int-encoded is acceptable". Unknown strings **throw
  `std::invalid_argument`** naming the field and the accepted set — a silent fallback would turn a typo
  (`"min_version": "tsl13"`) into weaker TLS. The `Protocol` codec lives in `listener_config.hpp` (config layer), NOT
  `http_enums.hpp` — the types layer must not grow a serialization/jsoncpp dependency.
- **D3 — `tls.key_passphrase` is `FieldPolicy::Secret`** (deserialize-only; the spec's §10.1 sketch listed it as a
  normal field). `dump_server_config` therefore never emits passphrases. §14.1's round-trip-equivalence test uses
  passphrase-free configs; a dedicated test locks the omission.
- **D4 — `ConfigSchemaError::field_path` is best-effort** (usually empty; `detail` names the offending field because
  every `validate()` message and enum-codec throw embeds it, e.g. `"listener.tls: required for tls/quic
  transports"`). The spec's `/listeners/1/tls/cert_file` YAML-pointer paths would require threading a path stack
  through every `read_field` overload signature — deferred as a strictly additive follow-up; the member stays so the
  API doesn't break when it lands.
- **D5 — `attach_default_listeners(Server&)` takes no `DefaultDrivers` parameter** (spec §10.3 sketched
  `(Server&, DefaultDrivers={})`): `Http11Config` is fully derivable from `ServerConfig`
  (`body_limit` + the three phase timeouts; `max_header_bytes` keeps its 16 KB struct default — no §10.1 field maps
  to it), and h2/h3 have no config yet. YAGNI; revisit when a second driver grows real config. v1 combinations:
  `tcp+[http1]`; `tls+` any non-empty duplicate-free subset of `{http1, http2}` where **JSON order = ALPN
  server-preference order = template-argument order**; `quic+[http3]`. Empty `protocols` defaults per transport
  (`http1`; `http3` on quic). Anything else throws `std::invalid_argument` (notably `tcp+[http2]` — h2c is
  unsupported). `ListenerConfig::validate()` enforces protocol **facts** only (h3 ⟺ quic, TLS-material presence, no
  duplicates) so the config layer stays driver-availability-agnostic; an empty `listeners` array is a no-op
  (programmatic `add_*_listener` calls compose with config-driven ones).
- **D6 — Constructability split.** `ServerConfig` is **Builder-only** (spec §10.1 sketch shows no public ctor; every
  default is valid, so `ServerConfig::Builder{}.finalize()` replaces the old `ServerConfig{}` at all 9 call sites).
  `TlsConfig` keeps a **public full ctor escape hatch** (FileSinkConfig precedent, does NOT validate) —
  `TlsConfig{"", ""}` — because the QUIC/TLS scaffold unit tests deliberately construct empty configs that
  `validate()` must reject on the loading path. `Timeouts` gets the spec's public 3-arg ctor; `ListenerConfig` is
  Builder-only.
- **D7 — `TlsListener` gains a second (delegating) constructor** `{exec, host, port, tls, arena_size, drivers...}`
  rather than a defaulted parameter — a default cannot follow a variadic pack. `Server::add_tls_listener` forwards
  `cfg_.request_arena_size()`. `QuicListener` stays arena-less (scaffold owns no connections).
- **D8 — Latent moved-from read in `Server`'s ctor fixed in passing:** `server.cpp:40` initializes
  `registry_{map_normalization(cfg.path_normalization)}` from the **parameter** after `cfg_{std::move(cfg)}` already
  ran (members initialize in declaration order; benign today only because moving a plain struct copies scalars). The
  accessor rewrite must touch this line anyway → it becomes `cfg_.path_normalization()`.
- **D9 — `load_server_config` hardening beyond the spec sketch:** rejects non-regular files (a directory otherwise
  surfaces as a confusing parse error), rejects a well-formed non-object top level (`"42"` is valid JSON that would
  otherwise silently produce an all-defaults config), and extracts the parse-error line number best-effort from
  jsoncpp's formatted message (`0` when unextractable). Unknown JSON keys are **ignored** (the `fields()` walk reads
  known names only) — documented behavior, locked by a test.

## File Structure

```
common/serialization/
├─ config/config_interface.hpp      MOD repair key materialization + read-into-member   (Task 1)
└─ formats/json/json.hpp            MOD optional<HasFields> fix; +vector<T>, +uint16_t  (Task 1)
   formats/json/json.cpp            MOD +uint16_t impls                                 (Task 1)

components/http/
├─ config/
│  ├─ CMakeLists.txt                MOD +3 leaves, aggregate, comment                   (Tasks 2,4,7)
│  ├─ timeouts/                     NEW {timeouts.hpp, CMakeLists.txt}                  (Task 2)
│  ├─ tls_config/tls_config.hpp     MOD rewrite in place → ConfigInterface              (Task 3)
│  ├─ tls_config/CMakeLists.txt     MOD link Serialization                              (Task 3)
│  ├─ listener_config/              NEW {listener_config.hpp, CMakeLists.txt}           (Task 4)
│  ├─ server_config/server_config.hpp  MOD rewrite in place → ConfigInterface           (Task 5)
│  ├─ server_config/CMakeLists.txt  MOD link Timeouts+ListenerConfig+Serialization      (Task 5)
│  └─ load_server_config/           NEW {load_server_config.hpp, .cpp, CMakeLists.txt}  (Task 7)
├─ listeners/
│  ├─ tls_listener/build_ssl_context.cpp  MOD accessors + cipher-list check (TODO PR6)  (Task 3)
│  └─ tls_listener/tls_listener.hpp MOD +arena ctor, resolve PR6 note                   (Task 6)
├─ server/
│  ├─ server/server.hpp             MOD accessor calls, arena forward, PR6 note         (Tasks 5,6)
│  ├─ server/server.cpp             MOD accessors + D8 moved-from fix                   (Task 5)
│  ├─ attach_default_listeners/     NEW {attach_default_listeners.hpp, .cpp, CMake}     (Task 8)
│  └─ CMakeLists.txt                MOD +leaf in aggregate                              (Task 8)
├─ drivers/http11/http11_config.hpp MOD comment sweep                                   (Task 10)
├─ routing/route_registry/route_registry.hpp  MOD comment sweep                         (Task 10)
├─ connection/request_arena/request_arena.hpp MOD comment sweep                         (Task 10)
└─ CMakeLists.txt                   MOD config-section comment                          (Task 10)

tests/unit_tests/
├─ CMakeLists.txt                   MOD +Serialization unit target                      (Task 1)
├─ serialization/test_config_interface.cpp   NEW machinery lock                         (Task 1)
└─ http/
   ├─ CMakeLists.txt                MOD +Http.Config, +Http.Server unit targets         (Tasks 2,8)
   ├─ config/test_timeouts.cpp                NEW                                       (Task 2)
   ├─ config/test_tls_config.cpp              NEW                                       (Task 3)
   ├─ config/test_listener_config.cpp         NEW                                       (Task 4)
   ├─ config/test_server_config.cpp           NEW                                       (Task 5)
   ├─ config/test_load_server_config.cpp      NEW                                       (Task 7)
   ├─ server/test_attach_default_listeners.cpp NEW                                      (Task 8)
   └─ listeners/{test_build_ssl_context,test_tls_listener,test_quic_listener}.cpp  MOD  (Tasks 3,6)

tests/integration_tests/http/
├─ server_test_fixture.hpp          MOD default-arg Builder                             (Task 5)
├─ test_http_server_lifecycle.cpp   MOD Builder at 5 sites                              (Task 5)
├─ test_http_server_concurrency.cpp MOD Builder at 1 site                               (Task 5)
├─ test_http_run_standalone.cpp     MOD Builder at 2 sites                              (Task 5)
├─ test_http_tls.cpp                MOD Builder + use extracted TlsClient               (Tasks 3,9)
├─ tls_client.hpp                   NEW extracted TlsClient                             (Task 9)
├─ test_http_config_wiring.cpp      NEW JSON→wire acceptance (§12.2 PR 6)               (Task 9)
└─ CMakeLists.txt                   MOD +source in Http.Server target                   (Task 9)

docs/superpowers/specs/2026-05-07-http-redesign-design.md  MOD spec sync                (Task 10)
```

Dependency order: Task 1 (machinery) unblocks everything; 2 (Timeouts) and 3 (TlsConfig) need 1; 4 (ListenerConfig)
needs 3; 5 (ServerConfig) needs 2+4; 6 (arena wiring) needs 5 (accessor form); 7 (load/dump) needs 5; 8 (attach)
needs 5 (+drivers); 9 (integration) needs 7+8; 10 is the docs pass.

---

### Task 1: Repair + extend the serialization machinery (common layer)

The `ConfigInterface` auto-serialize path has never compiled when instantiated (landed fact 1) and lacks
`std::vector<T>`/`std::uint16_t` codecs (landed fact 2). Fix both, prove it with a machinery-lock unit target that
exercises exactly the shapes the HTTP config layer needs — including nested configs with private framework
constructors held directly, in `std::optional`, and in `std::vector`.

**Files:**

- Modify: `common/serialization/config/field.hpp`
- Modify: `common/serialization/config/config_interface.hpp`
- Modify: `common/serialization/formats/json/json.hpp`
- Modify: `common/serialization/formats/json/json.cpp`
- Create: `tests/unit_tests/serialization/test_config_interface.cpp`
- Modify: `tests/unit_tests/CMakeLists.txt`

**Interfaces:**

- Consumes: `Field`/`FieldPolicy` (`field.hpp`), `HasFields` (`serial_concepts.hpp`).
- Produces (relied on by Tasks 2–7): `serialization::FieldName{std::string_view value}` with `str()` — the key
  parameter type of every `read_field`/`write_field` extension-point overload (ADL anchor; custom codecs — the HTTP
  enum codecs of Tasks 3–5 — must use it: `read_field(const Json::Value&, serialization::FieldName, E&)`).
  `Derived::deserialize<Json::Value>(const Json::Value&)` builds via `Derived::Builder` and **validates** (Builder's
  `finalize() &&` runs `validate()`); missing keys keep the member's declared default; `serialize<Json::Value>()`
  validates first and skips `Secret`/`Excluded` fields. New codecs: `read_field/write_field` for
  `std::vector<T>` and `std::uint16_t` (0–65535 range-checked). Field types are **no longer required to be
  default-constructible** at namespace scope.

- [ ] **Step 1: Write the failing machinery-lock test**

Create `tests/unit_tests/serialization/test_config_interface.cpp`:

```cpp
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <config_interface.hpp>
#include <json/json.hpp>

// Exercises the generic fields()/Builder machinery end-to-end against a
// FileSinkConfig-shaped pair of test configs — including the two shapes the
// HTTP config layer (PR 6) depends on: nested configs with PRIVATE framework
// default constructors (held directly, in std::optional, and in std::vector)
// and std::uint16_t fields. This is the machinery's own lock, independent of
// the HTTP component (it must keep passing with BUILD_COMPONENTS=OFF).

namespace {

    namespace ser = demiplane::serialization;

    class InnerConfig final : public ser::ConfigInterface<InnerConfig, Json::Value> {
    public:
        constexpr explicit InnerConfig(const int retries) noexcept
            : retries_{retries} {
        }

        constexpr void validate() const override {
            if (retries_ < 0) {
                throw std::invalid_argument("inner.retries must be non-negative");
            }
        }

        [[nodiscard]] constexpr int retries() const noexcept {
            return retries_;
        }

        static constexpr auto fields() {
            return std::tuple{
                ser::Field<&InnerConfig::retries_, "retries">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr InnerConfig() = default;

        int retries_ = 1;
    };

    class InnerConfig::Builder {
    public:
        Builder() = default;

        template <typename Self>
        constexpr auto&& retries(this Self&& self, const int value) noexcept {
            self.config_.retries_ = value;
            return std::forward<Self>(self);
        }

        [[nodiscard]] InnerConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class InnerConfig;
        friend class ConfigInterface;
        InnerConfig config_;
    };

    class OuterConfig final : public ser::ConfigInterface<OuterConfig, Json::Value> {
    public:
        enum class Mode : std::uint8_t { fast, safe };

        void validate() const override {
            if (name_.empty()) {
                throw std::invalid_argument("outer.name must not be empty");
            }
            inner_.validate();
            if (maybe_inner_) {
                maybe_inner_->validate();
            }
            for (const auto& item : items_) {
                item.validate();
            }
        }

        [[nodiscard]] const std::string& name() const noexcept {
            return name_;
        }
        [[nodiscard]] std::size_t count() const noexcept {
            return count_;
        }
        [[nodiscard]] std::uint16_t port() const noexcept {
            return port_;
        }
        [[nodiscard]] bool flag() const noexcept {
            return flag_;
        }
        [[nodiscard]] std::chrono::milliseconds delay() const noexcept {
            return delay_;
        }
        [[nodiscard]] Mode mode() const noexcept {
            return mode_;
        }
        [[nodiscard]] const std::string& secret() const noexcept {
            return secret_;
        }
        [[nodiscard]] const InnerConfig& inner() const noexcept {
            return inner_;
        }
        [[nodiscard]] const std::optional<InnerConfig>& maybe_inner() const noexcept {
            return maybe_inner_;
        }
        [[nodiscard]] const std::vector<InnerConfig>& items() const noexcept {
            return items_;
        }
        [[nodiscard]] const std::vector<int>& numbers() const noexcept {
            return numbers_;
        }

        static constexpr auto fields() {
            return std::tuple{
                ser::Field<&OuterConfig::name_, "name">{},
                ser::Field<&OuterConfig::count_, "count">{},
                ser::Field<&OuterConfig::port_, "port">{},
                ser::Field<&OuterConfig::flag_, "flag">{},
                ser::Field<&OuterConfig::delay_, "delay_ms">{},
                ser::Field<&OuterConfig::mode_, "mode">{},
                ser::Field<&OuterConfig::secret_, "secret", ser::FieldPolicy::Secret>{},
                ser::Field<&OuterConfig::inner_, "inner">{},
                ser::Field<&OuterConfig::maybe_inner_, "maybe_inner">{},
                ser::Field<&OuterConfig::items_, "items">{},
                ser::Field<&OuterConfig::numbers_, "numbers">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        OuterConfig() = default;

        std::string name_  = "default";
        std::size_t count_ = 0;
        std::uint16_t port_ = 80;
        bool flag_ = false;
        std::chrono::milliseconds delay_{250};
        Mode mode_ = Mode::fast;
        std::string secret_;
        // InnerConfig's framework default ctor is private — the member default
        // goes through the public full ctor (the exact shape ServerConfig uses
        // for its nested Timeouts).
        InnerConfig inner_ = InnerConfig{1};
        std::optional<InnerConfig> maybe_inner_{};
        std::vector<InnerConfig> items_{};
        std::vector<int> numbers_{};
    };

    class OuterConfig::Builder {
    public:
        Builder() = default;

        template <typename Self>
        auto&& name(this Self&& self, std::string value) noexcept {
            self.config_.name_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& count(this Self&& self, const std::size_t value) noexcept {
            self.config_.count_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& port(this Self&& self, const std::uint16_t value) noexcept {
            self.config_.port_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& flag(this Self&& self, const bool value) noexcept {
            self.config_.flag_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& delay(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.delay_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& mode(this Self&& self, const Mode value) noexcept {
            self.config_.mode_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& secret(this Self&& self, std::string value) noexcept {
            self.config_.secret_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& inner(this Self&& self, InnerConfig value) noexcept {
            self.config_.inner_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& maybe_inner(this Self&& self, InnerConfig value) noexcept {
            self.config_.maybe_inner_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& items(this Self&& self, std::vector<InnerConfig> value) noexcept {
            self.config_.items_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& numbers(this Self&& self, std::vector<int> value) noexcept {
            self.config_.numbers_ = std::move(value);
            return std::forward<Self>(self);
        }

        [[nodiscard]] OuterConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class OuterConfig;
        friend class ConfigInterface;
        OuterConfig config_;
    };

    OuterConfig make_full_config() {
        return OuterConfig::Builder{}
            .name("svc")
            .count(7)
            .port(8443)
            .flag(true)
            .delay(std::chrono::milliseconds{1500})
            .mode(OuterConfig::Mode::safe)
            .secret("hunter2")
            .inner(InnerConfig{2})
            .maybe_inner(InnerConfig{5})
            .items({InnerConfig{10}, InnerConfig{20}})
            .numbers({1, 2, 3})
            .finalize();
    }

}  // namespace

TEST(ConfigInterfaceTest, RoundTripPreservesEveryField) {
    const auto cfg   = make_full_config();
    Json::Value json = cfg.serialize<Json::Value>();
    // Secret fields do not survive the trip by design — re-inject for the
    // deserialize leg so this test covers the Secret READ path too.
    json["secret"] = "hunter2";
    const auto back = OuterConfig::deserialize<Json::Value>(json);

    EXPECT_EQ(back.name(), "svc");
    EXPECT_EQ(back.count(), 7u);
    EXPECT_EQ(back.port(), 8443);
    EXPECT_TRUE(back.flag());
    EXPECT_EQ(back.delay(), std::chrono::milliseconds{1500});
    EXPECT_EQ(back.mode(), OuterConfig::Mode::safe);
    EXPECT_EQ(back.secret(), "hunter2");
    EXPECT_EQ(back.inner().retries(), 2);
    ASSERT_TRUE(back.maybe_inner().has_value());
    EXPECT_EQ(back.maybe_inner()->retries(), 5);
    ASSERT_EQ(back.items().size(), 2u);
    EXPECT_EQ(back.items()[0].retries(), 10);
    EXPECT_EQ(back.items()[1].retries(), 20);
    EXPECT_EQ(back.numbers(), (std::vector{1, 2, 3}));
}

TEST(ConfigInterfaceTest, SecretFieldIsNotSerialized) {
    const auto json = make_full_config().serialize<Json::Value>();
    EXPECT_FALSE(json.isMember("secret"));
}

TEST(ConfigInterfaceTest, MissingKeysKeepDeclaredDefaults) {
    const auto cfg = OuterConfig::deserialize<Json::Value>(Json::Value{Json::objectValue});
    EXPECT_EQ(cfg.name(), "default");
    EXPECT_EQ(cfg.count(), 0u);
    EXPECT_EQ(cfg.port(), 80);
    EXPECT_FALSE(cfg.flag());
    EXPECT_EQ(cfg.delay(), std::chrono::milliseconds{250});
    EXPECT_EQ(cfg.mode(), OuterConfig::Mode::fast);
    EXPECT_EQ(cfg.inner().retries(), 1);
    EXPECT_FALSE(cfg.maybe_inner().has_value());
    EXPECT_TRUE(cfg.items().empty());
    EXPECT_TRUE(cfg.numbers().empty());
}

TEST(ConfigInterfaceTest, UnknownJsonKeysAreIgnored) {
    Json::Value json{Json::objectValue};
    json["name"]        = "svc";
    json["not_a_field"] = 42;
    const auto cfg      = OuterConfig::deserialize<Json::Value>(json);
    EXPECT_EQ(cfg.name(), "svc");
}

TEST(ConfigInterfaceTest, TypeMismatchThrowsJsonException) {
    Json::Value json{Json::objectValue};
    json["count"] = "not-a-number";
    EXPECT_THROW((void)OuterConfig::deserialize<Json::Value>(json), Json::Exception);
}

TEST(ConfigInterfaceTest, VectorFieldRejectsNonArray) {
    Json::Value json{Json::objectValue};
    json["items"] = 42;
    EXPECT_THROW((void)OuterConfig::deserialize<Json::Value>(json), std::invalid_argument);
}

TEST(ConfigInterfaceTest, Uint16RangeIsEnforced) {
    Json::Value json{Json::objectValue};
    json["port"] = 70000;
    EXPECT_THROW((void)OuterConfig::deserialize<Json::Value>(json), std::invalid_argument);
}

TEST(ConfigInterfaceTest, DeserializeRunsValidate) {
    Json::Value inner{Json::objectValue};
    inner["retries"] = -3;
    Json::Value json{Json::objectValue};
    json["inner"] = inner;
    EXPECT_THROW((void)OuterConfig::deserialize<Json::Value>(json), std::invalid_argument);
}

TEST(ConfigInterfaceTest, SerializeRunsValidate) {
    // The full ctor is the no-validation escape hatch; serialize() validates.
    EXPECT_THROW((void)InnerConfig{-1}.serialize<Json::Value>(), std::invalid_argument);
}
```

- [ ] **Step 2: Register the unit target**

In `tests/unit_tests/CMakeLists.txt`, append at the end of the file:

```cmake
##############################################################################
# Test Serialization (ConfigInterface machinery + JSON field codecs)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Serialization
        serialization/test_config_interface.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Serialization
        PRIVATE
        Demiplane::Common::Serialization
        ${TEST_LIBS}
)
##############################################################################
```

- [ ] **Step 3: Run the test to verify it fails (compile failure = red)**

Run: `cmake --preset debug 2>&1 | tail -3 && cmake --build build/debug --target Demiplane.Tests.Unit.Serialization -- -j4 2>&1 | tail -20`
Expected: **compile errors** inside the `ConfigInterface` instantiation — clang reports
`call to function 'write_field'/'read_field' that is neither visible in the template definition nor found by
argument-dependent lookup` for the scalar/chrono fields (landed fact 1, first bullet), plus missing vector/uint16
codecs and an inaccessible `InnerConfig()` for the nested fields (facts 1c, 2). This is the machinery bug surfacing
for the first time, not a test bug.

- [ ] **Step 4: Add `FieldName` + repair `config_interface.hpp`**

Replace the entire contents of `common/serialization/config/field.hpp` with:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <gears_strings.hpp>

namespace demiplane::serialization {

    enum class FieldPolicy : std::uint8_t {
        Normal,    // serialize + deserialize
        Secret,    // deserialize only (e.g., passwords)
        Excluded,  // skip both directions
        ReadOnly,  // serialize only
    };

    /// Key wrapper for the read_field/write_field extension points. A domain
    /// type (rather than a bare string) so the machinery's unqualified,
    /// DEPENDENT calls always reach the format overloads: FieldName makes
    /// demiplane::serialization an ASSOCIATED NAMESPACE of every call, which
    /// two-phase lookup requires — the format headers (json.hpp) are normally
    /// included AFTER config_interface.hpp, so ordinary lookup at the template
    /// definition point sees none of them, and a plain string key carries no
    /// namespace ADL could find them through.
    struct FieldName {
        std::string_view value;

        [[nodiscard]] std::string str() const {
            return std::string{value};
        }
    };

    namespace detail {
        template <typename T>
        struct member_pointer_traits;

        template <typename C, typename V>
        struct member_pointer_traits<V C::*> {
            using owner_type = C;
            using value_type = V;
        };
    }  // namespace detail

    template <auto Ptr, gears::FixedString Name, FieldPolicy Policy = FieldPolicy::Normal>
    struct Field {
        static constexpr auto ptr    = Ptr;
        static constexpr auto name   = Name;
        static constexpr auto policy = Policy;

        using owner_type = detail::member_pointer_traits<decltype(Ptr)>::owner_type;
        using value_type = detail::member_pointer_traits<decltype(Ptr)>::value_type;
    };

}  // namespace demiplane::serialization
```

Then replace the entire contents of `common/serialization/config/config_interface.hpp` with:

```cpp
#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "field.hpp"
#include "serial_concepts.hpp"

namespace demiplane::serialization {

    template <typename Derived, typename... Formats>
    class ConfigInterface {
    public:
        virtual ~ConfigInterface() = default;

        constexpr virtual void validate() const = 0;

        template <typename Format>
            requires(std::same_as<Format, Formats> || ...)
        [[nodiscard]] Format serialize() const {
            static_cast<const Derived&>(*this).validate();
            if constexpr (HasCustomSerialize<Derived, Format>) {
                return static_cast<const Derived&>(*this).custom_serialize(std::type_identity<Format>{});
            } else {
                return auto_serialize<Format>();
            }
        }

        template <typename Format>
            requires(std::same_as<Format, Formats> || ...)
        static Derived deserialize(const Format& input) {
            if constexpr (HasCustomDeserialize<Derived, Format>) {
                return Derived::custom_deserialize(input);
            } else {
                return auto_deserialize<Format>(input);
            }
        }

    private:
        template <typename Format>
        [[nodiscard]] Format auto_serialize() const {
            Format out{};
            constexpr auto fs = Derived::fields();
            std::apply(
                [&](const auto&... f) { (serialize_one_field(out, static_cast<const Derived&>(*this), f), ...); }, fs);
            return out;
        }

        template <typename Format, typename F>
        static void serialize_one_field(Format& out, const Derived& d, F) {
            if constexpr (F::policy != FieldPolicy::Secret && F::policy != FieldPolicy::Excluded) {
                // FieldName (not a bare string) keeps demiplane::serialization
                // an associated namespace of this dependent call — the only
                // route by which two-phase lookup reaches the format overloads
                // (see field.hpp).
                write_field(out, FieldName{F::name.view()}, d.*F::ptr);
            }
        }

        template <typename Format>
        static Derived auto_deserialize(const Format& input) {
            auto builder      = []() { return typename Derived::Builder{}; }();
            constexpr auto fs = Derived::fields();
            std::apply([&](const auto&... f) { (deserialize_one_field(input, builder, f), ...); }, fs);
            return std::move(builder).finalize();
        }

        template <typename Format, typename BuilderT, typename F>
        static void deserialize_one_field(const Format& input, BuilderT& builder, F) {
            if constexpr (F::policy != FieldPolicy::Excluded && F::policy != FieldPolicy::ReadOnly) {
                // Read straight into the builder's member: no temporary, so a
                // field's type need not be default-constructible at namespace
                // scope (nested ConfigInterface types keep their private
                // framework constructors), and a missing key leaves the
                // member's declared default untouched.
                read_field(input, FieldName{F::name.view()}, builder.config_.*F::ptr);
            }
        }
    };

}  // namespace demiplane::serialization
```

- [ ] **Step 5: Extend `json.hpp`**

Replace the entire contents of `common/serialization/formats/json/json.hpp` with (every key parameter is now the
`FieldName` ADL anchor):

```cpp
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <json/json.h>

#include <field.hpp>
#include <serial_concepts.hpp>

namespace demiplane::serialization {

    // ---- write_field overloads for Json::Value ----
    // The FieldName key parameter is the extension point's ADL anchor (see
    // field.hpp). Custom codecs (e.g. the HTTP enum-as-string overloads) must
    // use the same shape.

    void write_field(Json::Value& out, FieldName key, const std::string& v);
    void write_field(Json::Value& out, FieldName key, std::string_view v);
    void write_field(Json::Value& out, FieldName key, int v);
    void write_field(Json::Value& out, FieldName key, std::size_t v);
    void write_field(Json::Value& out, FieldName key, std::uint16_t v);
    void write_field(Json::Value& out, FieldName key, bool v);
    void write_field(Json::Value& out, FieldName key, double v);
    void write_field(Json::Value& out, FieldName key, const std::filesystem::path& v);
    void write_field(Json::Value& out, FieldName key, const std::map<std::string, std::string>& v);

    // Json::Value passthrough (for nested serialized objects)
    void write_field(Json::Value& out, FieldName key, const Json::Value& v);

    template <typename Rep, typename Period>
    void write_field(Json::Value& out, const FieldName key, std::chrono::duration<Rep, Period> d) {
        out[key.str()] = static_cast<Json::Int64>(d.count());
    }

    template <typename E>
        requires std::is_enum_v<E>
    void write_field(Json::Value& out, const FieldName key, E v) {
        out[key.str()] = static_cast<int>(v);
    }

    template <typename T>
    void write_field(Json::Value& out, const FieldName key, const std::optional<T>& v) {
        if (v) {
            write_field(out, key, *v);
        }
    }

    template <typename T>
        requires HasFields<T>
    void write_field(Json::Value& out, const FieldName key, const T& nested) {
        out[key.str()] = nested.template serialize<Json::Value>();
    }

    /// Vector fields encode as a JSON array. HasFields elements nest as
    /// objects; every other element type reuses its scalar/enum overload via a
    /// single-key wrap object (so ADL extension points work element-wise too).
    template <typename T>
    void write_field(Json::Value& out, const FieldName key, const std::vector<T>& v) {
        Json::Value arr{Json::arrayValue};
        for (const auto& element : v) {
            if constexpr (HasFields<T>) {
                arr.append(element.template serialize<Json::Value>());
            } else {
                Json::Value wrap{Json::objectValue};
                write_field(wrap, FieldName{"v"}, element);
                arr.append(wrap["v"]);
            }
        }
        out[key.str()] = std::move(arr);
    }

    // ---- read_field overloads for Json::Value ----
    // Return true if the field was present and read successfully. Convertible-
    // type mismatches throw Json::LogicError (jsoncpp's as*()); structural
    // mismatches (non-array for a vector) and range violations throw
    // std::invalid_argument.

    bool read_field(const Json::Value& in, FieldName key, std::string& v);
    bool read_field(const Json::Value& in, FieldName key, int& v);
    bool read_field(const Json::Value& in, FieldName key, std::size_t& v);
    bool read_field(const Json::Value& in, FieldName key, std::uint16_t& v);
    bool read_field(const Json::Value& in, FieldName key, bool& v);
    bool read_field(const Json::Value& in, FieldName key, double& v);
    bool read_field(const Json::Value& in, FieldName key, std::filesystem::path& v);
    bool read_field(const Json::Value& in, FieldName key, std::map<std::string, std::string>& v);
    bool read_field(const Json::Value& in, FieldName key, Json::Value& v);

    template <typename Rep, typename Period>
    bool read_field(const Json::Value& in, const FieldName key, std::chrono::duration<Rep, Period>& d) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        d = std::chrono::duration<Rep, Period>{static_cast<Rep>(in[k].asInt64())};
        return true;
    }

    template <typename E>
        requires std::is_enum_v<E>
    bool read_field(const Json::Value& in, const FieldName key, E& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = static_cast<E>(in[k].asInt());
        return true;
    }

    template <typename T>
    bool read_field(const Json::Value& in, const FieldName key, std::optional<T>& v) {
        if (!in.isMember(key.str())) {
            return false;
        }
        if constexpr (HasFields<T>) {
            // Construct through the framework path — T's default ctor is
            // typically private (Builder-only construction).
            v = T::template deserialize<Json::Value>(in[key.str()]);
            return true;
        } else {
            if (T inner{}; read_field(in, key, inner)) {
                v = std::move(inner);
                return true;
            }
            return false;
        }
    }

    template <typename T>
        requires HasFields<T>
    bool read_field(const Json::Value& in, const FieldName key, T& nested) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        nested = T::template deserialize<Json::Value>(in[k]);
        return true;
    }

    template <typename T>
    bool read_field(const Json::Value& in, const FieldName key, std::vector<T>& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const Json::Value& arr = in[k];
        if (!arr.isArray()) {
            throw std::invalid_argument{"config field '" + k + "': expected a JSON array"};
        }
        std::vector<T> result;
        result.reserve(arr.size());
        for (const auto& item : arr) {
            if constexpr (HasFields<T>) {
                result.push_back(T::template deserialize<Json::Value>(item));
            } else {
                Json::Value wrap{Json::objectValue};
                wrap["v"] = item;
                T element{};
                if (read_field(wrap, FieldName{"v"}, element)) {
                    result.push_back(std::move(element));
                }
            }
        }
        v = std::move(result);
        return true;
    }

}  // namespace demiplane::serialization
```

- [ ] **Step 6: Rewrite `json.cpp` (FieldName keys + the `std::uint16_t` pair)**

Replace the entire contents of `common/serialization/formats/json/json.cpp` with:

```cpp
#include "json.hpp"

namespace demiplane::serialization {

    // ---- write_field implementations ----

    void write_field(Json::Value& out, const FieldName key, const std::string& v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const std::string_view v) {
        out[key.str()] = std::string{v};
    }

    void write_field(Json::Value& out, const FieldName key, const int v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const std::size_t v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const std::uint16_t v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const bool v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const double v) {
        out[key.str()] = v;
    }

    void write_field(Json::Value& out, const FieldName key, const std::filesystem::path& v) {
        out[key.str()] = v.string();
    }

    void write_field(Json::Value& out, const FieldName key, const std::map<std::string, std::string>& v) {
        Json::Value obj{Json::objectValue};
        for (const auto& [mk, mv] : v) {
            obj[mk] = mv;
        }
        out[key.str()] = std::move(obj);
    }

    void write_field(Json::Value& out, const FieldName key, const Json::Value& v) {
        out[key.str()] = v;
    }

    // ---- read_field implementations ----

    bool read_field(const Json::Value& in, const FieldName key, std::string& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asString();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, int& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asInt();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, std::size_t& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asUInt64();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, std::uint16_t& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const auto raw = in[k].asUInt();  // Json::LogicError on type mismatch / negative
        if (raw > 0xFFFF) {
            throw std::invalid_argument{"config field '" + k + "': value " + std::to_string(raw) +
                                        " exceeds 65535"};
        }
        v = static_cast<std::uint16_t>(raw);
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, bool& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asBool();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, double& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asDouble();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, std::filesystem::path& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k].asString();
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, std::map<std::string, std::string>& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        for (const auto& obj = in[k]; const auto& mk : obj.getMemberNames()) {
            v[mk] = obj[mk].asString();
        }
        return true;
    }

    bool read_field(const Json::Value& in, const FieldName key, Json::Value& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        v = in[k];
        return true;
    }

}  // namespace demiplane::serialization
```

- [ ] **Step 7: Build + run the machinery lock**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Serialization -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Demiplane.Tests.Unit.Serialization"`
Expected: build clean; 9 tests PASS.

- [ ] **Step 8: Prove source compatibility with the existing consumers**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Scroll -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Demiplane.Tests.Unit.Scroll"`
Expected: scroll (FileSinkConfig et al.) builds untouched and its suite passes.

- [ ] **Step 9: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add common/serialization tests/unit_tests/serialization tests/unit_tests/CMakeLists.txt
# suggested message: "serialization: repair ConfigInterface instantiation (FieldName ADL anchor, read-into-member) + vector/uint16 codecs, machinery-lock tests"
```

---

### Task 2: `Timeouts` config leaf

**Files:**

- Create: `components/http/config/timeouts/timeouts.hpp`
- Create: `components/http/config/timeouts/CMakeLists.txt`
- Modify: `components/http/config/CMakeLists.txt`
- Create: `tests/unit_tests/http/config/test_timeouts.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1's machinery (`ConfigInterface`, chrono `read_field`/`write_field`).
- Produces: `demiplane::http::Timeouts` — public `constexpr Timeouts(std::chrono::milliseconds header, body, idle)
  noexcept`; accessors `header()`/`body()`/`idle()` → `std::chrono::milliseconds`; JSON fields
  `header_ms`/`body_ms`/`idle_ms` (integers, milliseconds); defaults 10 s / 30 s / 60 s; `validate()` rejects
  non-positive values; `Timeouts::Builder` with `header(ms)`/`body(ms)`/`idle(ms)`/`finalize()`. CMake leaf
  `${DMP_HTTP}.Config.Timeouts` (INTERFACE). Consumed by Task 5's `ServerConfig` (nested member — note its private
  framework ctor forces the member default through this public full ctor) and Task 8's `Http11Config` mapping. Also
  creates the `Demiplane.Tests.Unit.Http.Config` test target that Tasks 3/4/5/7 append sources to.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/config/test_timeouts.cpp`:

```cpp
#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

#include <json/json.hpp>
#include <timeouts.hpp>

using namespace demiplane::http;
using namespace std::chrono_literals;

TEST(TimeoutsConfigTest, DefaultsMatchSpec) {
    const auto t = Timeouts::deserialize<Json::Value>(Json::Value{Json::objectValue});
    EXPECT_EQ(t.header(), 10s);
    EXPECT_EQ(t.body(), 30s);
    EXPECT_EQ(t.idle(), 60s);
}

TEST(TimeoutsConfigTest, RoundTripsThroughJson) {
    const auto t    = Timeouts{5000ms, 10000ms, 30000ms};  // public full ctor (spec §10.1)
    const auto json = t.serialize<Json::Value>();
    EXPECT_EQ(json["header_ms"].asInt64(), 5000);
    EXPECT_EQ(json["body_ms"].asInt64(), 10000);
    EXPECT_EQ(json["idle_ms"].asInt64(), 30000);
    const auto back = Timeouts::deserialize<Json::Value>(json);
    EXPECT_EQ(back.header(), 5000ms);
    EXPECT_EQ(back.body(), 10000ms);
    EXPECT_EQ(back.idle(), 30000ms);
}

TEST(TimeoutsConfigTest, BuilderBuildsAndValidates) {
    const auto t = Timeouts::Builder{}.header(1s).body(2s).idle(3s).finalize();
    EXPECT_EQ(t.header(), 1s);
    EXPECT_EQ(t.body(), 2s);
    EXPECT_EQ(t.idle(), 3s);
}

TEST(TimeoutsConfigTest, ValidateRejectsNonPositive) {
    EXPECT_THROW((void)Timeouts::Builder{}.header(0ms).finalize(), std::invalid_argument);
    EXPECT_THROW((void)Timeouts::Builder{}.body(-1ms).finalize(), std::invalid_argument);
    EXPECT_THROW((void)Timeouts::Builder{}.idle(0ms).finalize(), std::invalid_argument);
    EXPECT_THROW((void)Timeouts{0ms, 1ms, 1ms}.serialize<Json::Value>(), std::invalid_argument);
}
```

- [ ] **Step 2: Create the `Http.Config` unit target**

Append at the end of `tests/unit_tests/http/CMakeLists.txt`:

```cmake
##############################################################################
# Test HTTP Config layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Config
        config/test_timeouts.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Config
        PRIVATE
        Demiplane.Component.HTTP.Config
        Demiplane.Component.HTTP.Types
        JsonCpp::JsonCpp
        ${TEST_LIBS}
)
##############################################################################
```

- [ ] **Step 3: Run to verify it fails**

Run: `cmake --preset debug 2>&1 | tail -3 && cmake --build build/debug --target Demiplane.Tests.Unit.Http.Config -- -j4 2>&1 | tail -10`
Expected: FAIL — `timeouts.hpp: No such file or directory`.

- [ ] **Step 4: Write the header**

Create `components/http/config/timeouts/timeouts.hpp`:

```cpp
#pragma once

#include <chrono>
#include <stdexcept>
#include <tuple>
#include <utility>

#include <config_interface.hpp>
#include <json/json.hpp>

namespace demiplane::http {

    /**
     * @brief Per-phase HTTP timeout set (spec §10.1), JSON-loadable.
     *
     * Field names carry the unit (header_ms/body_ms/idle_ms): JSON values are
     * plain integers interpreted as milliseconds. attach_default_listeners
     * maps these onto Http11Config's per-phase timeouts.
     */
    class Timeouts final : public serialization::ConfigInterface<Timeouts, Json::Value> {
    public:
        constexpr Timeouts(const std::chrono::milliseconds header,
                           const std::chrono::milliseconds body,
                           const std::chrono::milliseconds idle) noexcept
            : header_{header},
              body_{body},
              idle_{idle} {
        }

        constexpr void validate() const override {
            if (header_ <= std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("timeouts.header_ms must be positive");
            }
            if (body_ <= std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("timeouts.body_ms must be positive");
            }
            if (idle_ <= std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("timeouts.idle_ms must be positive");
            }
        }

        [[nodiscard]] constexpr std::chrono::milliseconds header() const noexcept {
            return header_;
        }
        [[nodiscard]] constexpr std::chrono::milliseconds body() const noexcept {
            return body_;
        }
        [[nodiscard]] constexpr std::chrono::milliseconds idle() const noexcept {
            return idle_;
        }

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&Timeouts::header_, "header_ms">{},
                serialization::Field<&Timeouts::body_, "body_ms">{},
                serialization::Field<&Timeouts::idle_, "idle_ms">{},
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

    class Timeouts::Builder {
    public:
        Builder() = default;

        template <typename Self>
        constexpr auto&& header(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.header_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& body(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.body_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& idle(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.idle_ = value;
            return std::forward<Self>(self);
        }

        [[nodiscard]] Timeouts finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class Timeouts;
        friend class ConfigInterface;
        Timeouts config_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 5: Write the CMake leaf + wire the aggregate**

Create `components/http/config/timeouts/CMakeLists.txt`:

```cmake
##############################################################################
# Http Config — Timeouts (per-phase timeout set; spec §10.1, PR 6)
##############################################################################
add_library(${DMP_HTTP}.Config.Timeouts INTERFACE timeouts.hpp)

target_include_directories(${DMP_HTTP}.Config.Timeouts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Config.Timeouts INTERFACE
        Demiplane::Common::Serialization
)
##############################################################################
```

In `components/http/config/CMakeLists.txt`, add the subdirectory and the aggregate entry:

```cmake
add_subdirectory(tls_config)
add_subdirectory(server_config)
add_subdirectory(timeouts)
```

and

```cmake
target_link_libraries(${DMP_HTTP}.Config INTERFACE
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Config.ServerConfig
        ${DMP_HTTP}.Config.Timeouts
)
```

- [ ] **Step 6: Build + run**

Run: `cmake --preset debug 2>&1 | tail -3 && cmake --build build/debug --target Demiplane.Tests.Unit.Http.Config -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Demiplane.Tests.Unit.Http.Config"`
Expected: build clean; 4 tests PASS.

- [ ] **Step 7: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add components/http/config/timeouts components/http/config/CMakeLists.txt tests/unit_tests/http/config/test_timeouts.cpp tests/unit_tests/http/CMakeLists.txt
# suggested message: "http/config: Timeouts config leaf (ConfigInterface, header_ms/body_ms/idle_ms)"
```

---

### Task 3: `TlsConfig` rewrite in place + consumers (+ cipher-list TODO)

Rewrite the PR 4 plain struct as a `ConfigInterface` type at the same path (includes stay stable), switch
`build_ssl_context` and the four test consumers from field access to accessors, and resolve the `TODO(PR6)`
cipher-list return check while editing that file.

**Files:**

- Modify (rewrite): `components/http/config/tls_config/tls_config.hpp`
- Modify: `components/http/config/tls_config/CMakeLists.txt`
- Modify: `components/http/listeners/tls_listener/build_ssl_context.cpp`
- Modify: `tests/unit_tests/http/listeners/test_build_ssl_context.cpp`
- Modify: `tests/unit_tests/http/listeners/test_tls_listener.cpp`
- Modify: `tests/unit_tests/http/listeners/test_quic_listener.cpp`
- Modify: `tests/integration_tests/http/test_http_tls.cpp`
- Create: `tests/unit_tests/http/config/test_tls_config.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1's machinery.
- Produces: `demiplane::http::TlsConfig` — accessors `cert_file()`, `key_file()`, `key_passphrase()`,
  `dh_params_file()`, `ca_file()` (all `const std::string&`), `min_version()` → `TlsConfig::MinVersion`,
  `session_cache()`/`require_client_cert()` → `bool`; **public full ctor** `TlsConfig(cert_file, key_file,
  key_passphrase = "", dh_params_file = "", ca_file = "", min_version = tls12, session_cache = true,
  require_client_cert = false)` (escape hatch, no validation — D6); `TlsConfig::Builder` (same names as accessors);
  JSON field `min_version` is the **string** `"tls12"|"tls13"` (D2); `key_passphrase` is Secret (D3). ADL codec
  functions `to_string(TlsConfig::MinVersion)`, `write_field`/`read_field` for `MinVersion`. Consumed by Task 4
  (`ListenerConfig::tls`), Task 6 (TlsListener), and existing listeners.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/config/test_tls_config.cpp`:

```cpp
#include <gtest/gtest.h>

#include <stdexcept>

#include <json/json.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;

TEST(TlsConfigTest, BuilderRoundTripsThroughJson) {
    const auto cfg = TlsConfig::Builder{}
                         .cert_file("/etc/ssl/cert.pem")
                         .key_file("/etc/ssl/key.pem")
                         .dh_params_file("/etc/ssl/dh.pem")
                         .ca_file("/etc/ssl/ca.pem")
                         .min_version(TlsConfig::MinVersion::tls13)
                         .session_cache(false)
                         .require_client_cert(true)
                         .finalize();
    const auto json = cfg.serialize<Json::Value>();
    EXPECT_EQ(json["min_version"].asString(), "tls13");
    const auto back = TlsConfig::deserialize<Json::Value>(json);
    EXPECT_EQ(back.cert_file(), "/etc/ssl/cert.pem");
    EXPECT_EQ(back.key_file(), "/etc/ssl/key.pem");
    EXPECT_EQ(back.dh_params_file(), "/etc/ssl/dh.pem");
    EXPECT_EQ(back.ca_file(), "/etc/ssl/ca.pem");
    EXPECT_EQ(back.min_version(), TlsConfig::MinVersion::tls13);
    EXPECT_FALSE(back.session_cache());
    EXPECT_TRUE(back.require_client_cert());
}

TEST(TlsConfigTest, PassphraseIsSecretReadButNeverDumped) {
    Json::Value json{Json::objectValue};
    json["cert_file"]      = "c.pem";
    json["key_file"]       = "k.pem";
    json["key_passphrase"] = "hunter2";
    const auto cfg = TlsConfig::deserialize<Json::Value>(json);
    EXPECT_EQ(cfg.key_passphrase(), "hunter2");
    EXPECT_FALSE(cfg.serialize<Json::Value>().isMember("key_passphrase"));
}

TEST(TlsConfigTest, UnknownMinVersionStringThrows) {
    Json::Value json{Json::objectValue};
    json["cert_file"]   = "c.pem";
    json["key_file"]    = "k.pem";
    json["min_version"] = "ssl3";
    EXPECT_THROW((void)TlsConfig::deserialize<Json::Value>(json), std::invalid_argument);
}

TEST(TlsConfigTest, ValidateRequiresCertKeyAndCaForClientCerts) {
    EXPECT_THROW((void)TlsConfig::Builder{}.key_file("k.pem").finalize(), std::invalid_argument);
    EXPECT_THROW((void)TlsConfig::Builder{}.cert_file("c.pem").finalize(), std::invalid_argument);
    EXPECT_THROW(
        (void)TlsConfig::Builder{}.cert_file("c.pem").key_file("k.pem").require_client_cert(true).finalize(),
        std::invalid_argument);
}

TEST(TlsConfigTest, FullCtorIsNoValidationEscapeHatch) {
    const TlsConfig empty{"", ""};  // must construct — scaffold tests rely on it (D6)
    EXPECT_TRUE(empty.cert_file().empty());
    EXPECT_THROW((void)empty.serialize<Json::Value>(), std::invalid_argument);  // serialize() validates
}
```

Add the source to the target in `tests/unit_tests/http/CMakeLists.txt`:

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Config
        config/test_timeouts.cpp
        config/test_tls_config.cpp
)
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --preset debug 2>&1 | tail -3 && cmake --build build/debug --target Demiplane.Tests.Unit.Http.Config -- -j4 2>&1 | tail -10`
Expected: FAIL — `TlsConfig` has no `Builder`/`deserialize` (still the plain struct).

- [ ] **Step 3: Rewrite the header in place**

Replace the entire contents of `components/http/config/tls_config/tls_config.hpp` with:

```cpp
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <config_interface.hpp>
#include <json/json.hpp>

namespace demiplane::http {

    /**
     * @brief TLS settings consumed by build_ssl_context (spec §7.4 / §10.1),
     *        JSON-loadable via serialization::ConfigInterface (PR 6;
     *        supersedes the PR 4 plain struct at this same path).
     *
     * min_version encodes as a string ("tls12" | "tls13"); key_passphrase is
     * FieldPolicy::Secret — read from JSON, never written by dump. The full
     * constructor is a no-validation escape hatch (scaffold tests build empty
     * configs on purpose); Builder::finalize() and deserialize() validate.
     */
    class TlsConfig final : public serialization::ConfigInterface<TlsConfig, Json::Value> {
    public:
        enum class MinVersion : std::uint8_t { tls12, tls13 };

        constexpr TlsConfig(std::string cert_file,
                            std::string key_file,
                            std::string key_passphrase     = "",
                            std::string dh_params_file     = "",
                            std::string ca_file            = "",
                            const MinVersion min_version   = MinVersion::tls12,
                            const bool session_cache       = true,
                            const bool require_client_cert = false) noexcept
            : cert_file_{std::move(cert_file)},
              key_file_{std::move(key_file)},
              key_passphrase_{std::move(key_passphrase)},
              dh_params_file_{std::move(dh_params_file)},
              ca_file_{std::move(ca_file)},
              min_version_{min_version},
              session_cache_{session_cache},
              require_client_cert_{require_client_cert} {
        }

        constexpr void validate() const override {
            if (cert_file_.empty()) {
                throw std::invalid_argument("tls.cert_file must be set");
            }
            if (key_file_.empty()) {
                throw std::invalid_argument("tls.key_file must be set");
            }
            if (require_client_cert_ && ca_file_.empty()) {
                throw std::invalid_argument("tls.ca_file must be set when tls.require_client_cert is true");
            }
        }

        [[nodiscard]] const std::string& cert_file() const noexcept {
            return cert_file_;
        }
        [[nodiscard]] const std::string& key_file() const noexcept {
            return key_file_;
        }
        [[nodiscard]] const std::string& key_passphrase() const noexcept {
            return key_passphrase_;
        }
        [[nodiscard]] const std::string& dh_params_file() const noexcept {
            return dh_params_file_;
        }
        [[nodiscard]] const std::string& ca_file() const noexcept {
            return ca_file_;
        }
        [[nodiscard]] constexpr MinVersion min_version() const noexcept {
            return min_version_;
        }
        [[nodiscard]] constexpr bool session_cache() const noexcept {
            return session_cache_;
        }
        [[nodiscard]] constexpr bool require_client_cert() const noexcept {
            return require_client_cert_;
        }

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&TlsConfig::cert_file_, "cert_file">{},
                serialization::Field<&TlsConfig::key_file_, "key_file">{},
                serialization::
                    Field<&TlsConfig::key_passphrase_, "key_passphrase", serialization::FieldPolicy::Secret>{},
                serialization::Field<&TlsConfig::dh_params_file_, "dh_params_file">{},
                serialization::Field<&TlsConfig::ca_file_, "ca_file">{},
                serialization::Field<&TlsConfig::min_version_, "min_version">{},
                serialization::Field<&TlsConfig::session_cache_, "session_cache">{},
                serialization::Field<&TlsConfig::require_client_cert_, "require_client_cert">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr TlsConfig() = default;

        std::string cert_file_;
        std::string key_file_;
        std::string key_passphrase_;
        std::string dh_params_file_;
        std::string ca_file_;
        MinVersion min_version_   = MinVersion::tls12;
        bool session_cache_       = true;
        bool require_client_cert_ = false;
    };

    // ── MinVersion <-> string codec ──────────────────────────────────────
    // ADL-found by the serialization machinery (a non-template overload beats
    // the generic int-encoding enum template). String-encoded per spec §14.1.

    [[nodiscard]] constexpr std::string_view to_string(const TlsConfig::MinVersion v) noexcept {
        switch (v) {
            case TlsConfig::MinVersion::tls13:
                return "tls13";
            case TlsConfig::MinVersion::tls12:
                return "tls12";
        }
        return "tls12";
    }

    inline void write_field(Json::Value& out, const serialization::FieldName key, const TlsConfig::MinVersion v) {
        out[key.str()] = std::string{to_string(v)};
    }

    /// @throws std::invalid_argument on an unknown string — a silent default
    /// would turn a typo into weaker TLS.
    inline bool read_field(const Json::Value& in, const serialization::FieldName key, TlsConfig::MinVersion& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const std::string raw = in[k].asString();  // Json::LogicError on non-string
        if (raw == "tls12") {
            v = TlsConfig::MinVersion::tls12;
            return true;
        }
        if (raw == "tls13") {
            v = TlsConfig::MinVersion::tls13;
            return true;
        }
        throw std::invalid_argument{
            "config field '" + k + "': unknown min_version '" + raw + R"(' (expected "tls12" or "tls13"))"};
    }

    class TlsConfig::Builder {
    public:
        Builder() = default;

        template <typename Self>
        auto&& cert_file(this Self&& self, std::string value) noexcept {
            self.config_.cert_file_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& key_file(this Self&& self, std::string value) noexcept {
            self.config_.key_file_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& key_passphrase(this Self&& self, std::string value) noexcept {
            self.config_.key_passphrase_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& dh_params_file(this Self&& self, std::string value) noexcept {
            self.config_.dh_params_file_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& ca_file(this Self&& self, std::string value) noexcept {
            self.config_.ca_file_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& min_version(this Self&& self, const MinVersion value) noexcept {
            self.config_.min_version_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& session_cache(this Self&& self, const bool value) noexcept {
            self.config_.session_cache_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& require_client_cert(this Self&& self, const bool value) noexcept {
            self.config_.require_client_cert_ = value;
            return std::forward<Self>(self);
        }

        [[nodiscard]] TlsConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class TlsConfig;
        friend class ConfigInterface;
        TlsConfig config_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 4: Link the leaf against Serialization**

Replace the contents of `components/http/config/tls_config/CMakeLists.txt` with:

```cmake
##############################################################################
# Http Config — TlsConfig (ConfigInterface; JSON-loadable; PR 6)
##############################################################################
add_library(${DMP_HTTP}.Config.TlsConfig INTERFACE tls_config.hpp)

target_include_directories(${DMP_HTTP}.Config.TlsConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Config.TlsConfig INTERFACE
        Demiplane::Common::Serialization
)
##############################################################################
```

- [ ] **Step 5: Switch `build_ssl_context.cpp` to accessors + resolve the cipher-list TODO**

Replace the entire contents of `components/http/listeners/tls_listener/build_ssl_context.cpp` with:

```cpp
#include "build_ssl_context.hpp"

#include <cstddef>

#include <boost/asio/ssl/error.hpp>
#include <boost/system/system_error.hpp>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>

namespace demiplane::http {

    namespace {
        // OpenSSL ALPN select callback (spike S2). `arg` is the advertised
        // length-prefixed protocol list (the listener's long-lived buffer, D4).
        int alpn_select_cb(SSL* /*ssl*/,
                           const unsigned char** out,
                           unsigned char* outlen,
                           const unsigned char* in,
                           unsigned int inlen,
                           void* arg) {
            const auto* advertised = static_cast<const std::string*>(arg);
            if (::SSL_select_next_proto(const_cast<unsigned char**>(out),
                                        outlen,
                                        reinterpret_cast<const unsigned char*>(advertised->data()),
                                        static_cast<unsigned int>(advertised->size()),
                                        in,
                                        inlen) == OPENSSL_NPN_NEGOTIATED) {
                return SSL_TLSEXT_ERR_OK;
            }
            // OpenSSL 3.6.3 has no SSL_TLSEXT_ERR_ALPN_FAILED — ALERT_FATAL aborts
            // the handshake with no_application_protocol (spike S2 / D4).
            return SSL_TLSEXT_ERR_ALERT_FATAL;
        }
    }  // namespace

    boost::asio::ssl::context build_ssl_context(const TlsConfig& cfg, const std::string& advertised_alpn_wire) {
        namespace ssl = boost::asio::ssl;

        ssl::context ctx{ssl::context::tls_server};

        auto options = ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::no_sslv3 |
                       ssl::context::no_tlsv1 | ssl::context::no_tlsv1_1 | ssl::context::no_compression |
                       ssl::context::single_dh_use;
        if (cfg.min_version() == TlsConfig::MinVersion::tls13) {
            options |= ssl::context::no_tlsv1_2;
        }
        ctx.set_options(options);

        // Modern TLS 1.2 cipher floor (TLS 1.3 suites use OpenSSL's safe defaults).
        // A silent 0 would fall back to OpenSSL defaults, weakening the floor
        // this function guarantees — so a failure throws.
        if (::SSL_CTX_set_cipher_list(ctx.native_handle(),
                                      "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
                                      "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
                                      "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305") != 1) {
            throw boost::system::system_error{
                boost::system::error_code{static_cast<int>(::ERR_get_error()),
                                          boost::asio::error::get_ssl_category()},
                "SSL_CTX_set_cipher_list"};
        }

        if (!cfg.key_passphrase().empty()) {
            ctx.set_password_callback(
                [pass = cfg.key_passphrase()](std::size_t, ssl::context::password_purpose) { return pass; });
        }

        ctx.use_certificate_chain_file(cfg.cert_file());              // throws on failure
        ctx.use_private_key_file(cfg.key_file(), ssl::context::pem);  // throws on failure

        if (!cfg.dh_params_file().empty()) {
            ctx.use_tmp_dh_file(cfg.dh_params_file());
        }

        if (cfg.require_client_cert()) {
            ctx.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
            if (!cfg.ca_file().empty()) {
                ctx.load_verify_file(cfg.ca_file());
            }
        }

        ::SSL_CTX_set_session_cache_mode(ctx.native_handle(),
                                         cfg.session_cache() ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);

        // ALPN: arg points at the CALLER'S buffer — must outlive ctx (D4).
        ::SSL_CTX_set_alpn_select_cb(
            ctx.native_handle(), &alpn_select_cb, const_cast<std::string*>(&advertised_alpn_wire));
        return ctx;
    }

}  // namespace demiplane::http
```

- [ ] **Step 6: Update the four consumer test files**

In `tests/unit_tests/http/listeners/test_build_ssl_context.cpp`, replace

```cpp
TEST(BuildSslContextTest, BuildsFromValidCert) {
    TlsConfig cfg;
    cfg.cert_file = http_tls_test::write_temp("cert.pem", http_tls_test::kTestCertPem);
    cfg.key_file  = http_tls_test::write_temp("key.pem", http_tls_test::kTestKeyPem);
```

with

```cpp
TEST(BuildSslContextTest, BuildsFromValidCert) {
    const auto cfg = TlsConfig::Builder{}
                         .cert_file(http_tls_test::write_temp("cert.pem", http_tls_test::kTestCertPem))
                         .key_file(http_tls_test::write_temp("key.pem", http_tls_test::kTestKeyPem))
                         .finalize();
```

and replace

```cpp
TEST(BuildSslContextTest, ThrowsOnMissingCert) {
    TlsConfig cfg;
    cfg.cert_file = "/nonexistent/path/cert.pem";
    cfg.key_file  = "/nonexistent/path/key.pem";
```

with

```cpp
TEST(BuildSslContextTest, ThrowsOnMissingCert) {
    const auto cfg = TlsConfig::Builder{}
                         .cert_file("/nonexistent/path/cert.pem")
                         .key_file("/nonexistent/path/key.pem")
                         .finalize();
```

In `tests/unit_tests/http/listeners/test_tls_listener.cpp:18`, replace

```cpp
    TlsConfig tls;  // empty cert paths — fine, bind() (which builds the ctx) is not called here
```

with

```cpp
    TlsConfig tls{"", ""};  // empty cert paths via the full-ctor escape hatch — fine,
                            // bind() (which builds the ctx) is not called here
```

In `tests/unit_tests/http/listeners/test_quic_listener.cpp:21`, replace the `TlsConfig{}` argument:

```cpp
    QuicListener<Http3Driver> listener{ioc.get_executor(), "127.0.0.1", 8443, TlsConfig{"", ""},
```

(keep the rest of the statement unchanged).

In `tests/integration_tests/http/test_http_tls.cpp`, replace

```cpp
            TlsConfig tls;
            tls.cert_file = cert;
            tls.key_file  = key;
```

with

```cpp
            const auto tls = TlsConfig::Builder{}.cert_file(cert).key_file(key).finalize();
```

- [ ] **Step 7: Build + run everything TlsConfig touches**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Config Demiplane.Tests.Unit.Http.Listeners Demiplane.Tests.Integration.Http.Tls -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Http.Config|Http.Listeners|Integration.Http.Tls"`
Expected: build clean; TlsConfig tests (5), listener unit tests, and the TLS integration battery all PASS.

- [ ] **Step 8: Verify no field access remains**

Run: `grep -rn "\.cert_file\b\|\.key_file\b\|\.min_version\b\|\.session_cache\b\|\.require_client_cert\b\|\.key_passphrase\b\|\.dh_params_file\b\|\.ca_file\b" components tests --include="*.cpp" --include="*.hpp" | grep -v "()" | grep -v "config_\." | grep -v "self\.config_"`
Expected: no output (every remaining use is an accessor call or the Builder's internal member writes).

- [ ] **Step 9: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add components/http/config/tls_config components/http/listeners/tls_listener/build_ssl_context.cpp tests/unit_tests/http/config/test_tls_config.cpp tests/unit_tests/http/listeners tests/integration_tests/http/test_http_tls.cpp tests/unit_tests/http/CMakeLists.txt
# suggested message: "http/config: TlsConfig -> ConfigInterface (string min_version, Secret passphrase) + cipher-list failure check"
```

---

### Task 4: `ListenerConfig` leaf (+ `Protocol`/`Transport` string codecs)

**Files:**

- Create: `components/http/config/listener_config/listener_config.hpp`
- Create: `components/http/config/listener_config/CMakeLists.txt`
- Modify: `components/http/config/CMakeLists.txt`
- Create: `tests/unit_tests/http/config/test_listener_config.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 3's `TlsConfig` (`std::optional<TlsConfig>` field), `Protocol` from `<http_enums.hpp>` (types
  layer), Task 1's vector/uint16 codecs.
- Produces: `demiplane::http::ListenerConfig` — nested `enum class Transport : std::uint8_t { tcp, tls, quic }`;
  accessors `bind_address()` → `const std::string&` (default `"0.0.0.0"`), `port()` → `std::uint16_t` (default
  `8080`; `0` stays legal — tests bind ephemeral ports), `transport()` (default `tcp`), `protocols()` →
  `const std::vector<Protocol>&` (default empty), `tls()` → `const std::optional<TlsConfig>&`;
  `effective_protocols()` → `std::vector<Protocol>` (empty ⇒ `{http1}`, or `{http3}` on quic) — the shape Task 8
  consumes; Builder-only construction (D6); JSON names `bind`/`port`/`transport`/`protocols`/`tls`. ADL codecs:
  `to_string(Protocol)` + `read_field`/`write_field` for `Protocol` (strings `"http1"|"http2"|"http3"`) and for
  `Transport` (`"tcp"|"tls"|"quic"`); unknown strings throw. `validate()` = protocol facts only (D5): no duplicates,
  h3 ⟺ quic, TLS material required for tls/quic and rejected for tcp, nested `tls_->validate()`, non-empty `bind`.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/config/test_listener_config.cpp`:

```cpp
#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include <http_enums.hpp>
#include <json/json.hpp>
#include <listener_config.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;

namespace {
    TlsConfig valid_tls() {
        return TlsConfig::Builder{}.cert_file("c.pem").key_file("k.pem").finalize();
    }
}  // namespace

TEST(ListenerConfigTest, DefaultsMatchSpec) {
    const auto l = ListenerConfig::deserialize<Json::Value>(Json::Value{Json::objectValue});
    EXPECT_EQ(l.bind_address(), "0.0.0.0");
    EXPECT_EQ(l.port(), 8080);
    EXPECT_EQ(l.transport(), ListenerConfig::Transport::tcp);
    EXPECT_TRUE(l.protocols().empty());
    EXPECT_FALSE(l.tls().has_value());
    EXPECT_EQ(l.effective_protocols(), std::vector{Protocol::http1});
}

TEST(ListenerConfigTest, RoundTripPreservesProtocolOrder) {
    const auto l = ListenerConfig::Builder{}
                       .bind_address("127.0.0.1")
                       .port(8443)
                       .transport(ListenerConfig::Transport::tls)
                       .protocols({Protocol::http2, Protocol::http1})
                       .tls(valid_tls())
                       .finalize();
    const auto json = l.serialize<Json::Value>();
    EXPECT_EQ(json["transport"].asString(), "tls");
    ASSERT_TRUE(json["protocols"].isArray());
    EXPECT_EQ(json["protocols"][0].asString(), "http2");
    EXPECT_EQ(json["protocols"][1].asString(), "http1");
    const auto back = ListenerConfig::deserialize<Json::Value>(json);
    EXPECT_EQ(back.bind_address(), "127.0.0.1");
    EXPECT_EQ(back.port(), 8443);
    EXPECT_EQ(back.transport(), ListenerConfig::Transport::tls);
    EXPECT_EQ(back.effective_protocols(), (std::vector{Protocol::http2, Protocol::http1}));
    ASSERT_TRUE(back.tls().has_value());
    EXPECT_EQ(back.tls()->cert_file(), "c.pem");
}

TEST(ListenerConfigTest, UnknownTransportOrProtocolStringThrows) {
    Json::Value bad_transport{Json::objectValue};
    bad_transport["transport"] = "udp";
    EXPECT_THROW((void)ListenerConfig::deserialize<Json::Value>(bad_transport), std::invalid_argument);

    Json::Value bad_protocol{Json::objectValue};
    bad_protocol["protocols"] = Json::Value{Json::arrayValue};
    bad_protocol["protocols"].append("gopher");
    EXPECT_THROW((void)ListenerConfig::deserialize<Json::Value>(bad_protocol), std::invalid_argument);
}

TEST(ListenerConfigTest, ValidateEnforcesProtocolFacts) {
    // http3 off quic
    EXPECT_THROW((void)ListenerConfig::Builder{}.protocols({Protocol::http3}).finalize(), std::invalid_argument);
    // quic with a non-h3 set
    EXPECT_THROW((void)ListenerConfig::Builder{}
                     .transport(ListenerConfig::Transport::quic)
                     .protocols({Protocol::http1})
                     .tls(valid_tls())
                     .finalize(),
                 std::invalid_argument);
    // duplicates
    EXPECT_THROW((void)ListenerConfig::Builder{}.protocols({Protocol::http1, Protocol::http1}).finalize(),
                 std::invalid_argument);
    // tls transport without material
    EXPECT_THROW((void)ListenerConfig::Builder{}.transport(ListenerConfig::Transport::tls).finalize(),
                 std::invalid_argument);
    // tcp WITH material
    EXPECT_THROW((void)ListenerConfig::Builder{}.tls(valid_tls()).finalize(), std::invalid_argument);
    // nested TlsConfig is validated
    EXPECT_THROW((void)ListenerConfig::Builder{}
                     .transport(ListenerConfig::Transport::tls)
                     .tls(TlsConfig{"", ""})
                     .finalize(),
                 std::invalid_argument);
}

TEST(ListenerConfigTest, QuicDefaultsToH3) {
    const auto l = ListenerConfig::Builder{}
                       .transport(ListenerConfig::Transport::quic)
                       .tls(valid_tls())
                       .finalize();
    EXPECT_EQ(l.effective_protocols(), std::vector{Protocol::http3});
}
```

Add the source to the target in `tests/unit_tests/http/CMakeLists.txt`:

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Config
        config/test_timeouts.cpp
        config/test_tls_config.cpp
        config/test_listener_config.cpp
)
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Config -- -j4 2>&1 | tail -5`
Expected: FAIL — `listener_config.hpp: No such file or directory`.

- [ ] **Step 3: Write the header**

Create `components/http/config/listener_config/listener_config.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <config_interface.hpp>
#include <http_enums.hpp>
#include <json/json.hpp>
#include <tls_config.hpp>

namespace demiplane::http {

    // ── Protocol <-> string codec ────────────────────────────────────────
    // Lives HERE, not in http_enums.hpp: the types layer must not grow a
    // serialization/jsoncpp dependency. ADL finds these from the machinery
    // (Protocol's innermost enclosing namespace is demiplane::http).

    [[nodiscard]] constexpr std::string_view to_string(const Protocol p) noexcept {
        switch (p) {
            case Protocol::http1:
                return "http1";
            case Protocol::http2:
                return "http2";
            case Protocol::http3:
                return "http3";
        }
        return "http1";
    }

    inline void write_field(Json::Value& out, const serialization::FieldName key, const Protocol v) {
        out[key.str()] = std::string{to_string(v)};
    }

    /// @throws std::invalid_argument on an unknown protocol string — a silent
    /// fallback would turn a typo into serving the wrong protocol.
    inline bool read_field(const Json::Value& in, const serialization::FieldName key, Protocol& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const std::string raw = in[k].asString();  // Json::LogicError on non-string
        if (raw == "http1") {
            v = Protocol::http1;
            return true;
        }
        if (raw == "http2") {
            v = Protocol::http2;
            return true;
        }
        if (raw == "http3") {
            v = Protocol::http3;
            return true;
        }
        throw std::invalid_argument{
            "config field '" + k + "': unknown protocol '" + raw + R"(' (expected "http1"|"http2"|"http3"))"};
    }

    /**
     * @brief One listening endpoint (spec §10.1): transport + protocol set +
     *        optional TLS material.
     *
     * `protocols` order is meaningful for multi-protocol TLS listeners: it is
     * the ALPN server-preference order (attach_default_listeners maps it onto
     * TlsListener's template-argument order). validate() enforces protocol
     * FACTS only (h3 ⟺ quic, TLS-material presence, no duplicates); which
     * combinations v1 can actually serve is attach_default_listeners' check
     * (D5) — the config layer stays driver-availability-agnostic.
     */
    class ListenerConfig final : public serialization::ConfigInterface<ListenerConfig, Json::Value> {
    public:
        enum class Transport : std::uint8_t { tcp, tls, quic };

        constexpr void validate() const override {
            if (bind_address_.empty()) {
                throw std::invalid_argument("listener.bind must not be empty");
            }
            for (std::size_t i = 0; i < protocols_.size(); ++i) {
                for (std::size_t j = i + 1; j < protocols_.size(); ++j) {
                    if (protocols_[i] == protocols_[j]) {
                        throw std::invalid_argument("listener.protocols must not contain duplicates");
                    }
                }
            }
            bool has_h3 = false;
            for (const auto p : protocols_) {
                if (p == Protocol::http3) {
                    has_h3 = true;
                }
            }
            if (transport_ == Transport::quic) {
                if (!protocols_.empty() && (protocols_.size() != 1 || !has_h3)) {
                    throw std::invalid_argument(R"(listener.protocols: quic transport carries exactly ["http3"])");
                }
            } else if (has_h3) {
                throw std::invalid_argument("listener.protocols: http3 requires the quic transport");
            }
            if (transport_ == Transport::tcp) {
                if (tls_.has_value()) {
                    throw std::invalid_argument("listener.tls: only valid for tls/quic transports");
                }
            } else {
                if (!tls_.has_value()) {
                    throw std::invalid_argument("listener.tls: required for tls/quic transports");
                }
                tls_->validate();
            }
        }

        [[nodiscard]] const std::string& bind_address() const noexcept {
            return bind_address_;
        }
        [[nodiscard]] constexpr std::uint16_t port() const noexcept {
            return port_;
        }
        [[nodiscard]] constexpr Transport transport() const noexcept {
            return transport_;
        }
        [[nodiscard]] const std::vector<Protocol>& protocols() const noexcept {
            return protocols_;
        }
        [[nodiscard]] const std::optional<TlsConfig>& tls() const noexcept {
            return tls_;
        }

        /// protocols() with the transport's default filled in (empty ⇒ http1;
        /// http3 on quic) — the shape attach_default_listeners consumes.
        [[nodiscard]] std::vector<Protocol> effective_protocols() const {
            if (!protocols_.empty()) {
                return protocols_;
            }
            return {transport_ == Transport::quic ? Protocol::http3 : Protocol::http1};
        }

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&ListenerConfig::bind_address_, "bind">{},
                serialization::Field<&ListenerConfig::port_, "port">{},
                serialization::Field<&ListenerConfig::transport_, "transport">{},
                serialization::Field<&ListenerConfig::protocols_, "protocols">{},
                serialization::Field<&ListenerConfig::tls_, "tls">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        ListenerConfig() = default;

        std::string bind_address_ = "0.0.0.0";
        std::uint16_t port_       = 8080;
        Transport transport_      = Transport::tcp;
        std::vector<Protocol> protocols_{};
        std::optional<TlsConfig> tls_{};
    };

    // ── Transport <-> string codec ───────────────────────────────────────

    [[nodiscard]] constexpr std::string_view to_string(const ListenerConfig::Transport t) noexcept {
        switch (t) {
            case ListenerConfig::Transport::tcp:
                return "tcp";
            case ListenerConfig::Transport::tls:
                return "tls";
            case ListenerConfig::Transport::quic:
                return "quic";
        }
        return "tcp";
    }

    inline void write_field(Json::Value& out, const serialization::FieldName key, const ListenerConfig::Transport v) {
        out[key.str()] = std::string{to_string(v)};
    }

    inline bool read_field(const Json::Value& in, const serialization::FieldName key, ListenerConfig::Transport& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const std::string raw = in[k].asString();  // Json::LogicError on non-string
        if (raw == "tcp") {
            v = ListenerConfig::Transport::tcp;
            return true;
        }
        if (raw == "tls") {
            v = ListenerConfig::Transport::tls;
            return true;
        }
        if (raw == "quic") {
            v = ListenerConfig::Transport::quic;
            return true;
        }
        throw std::invalid_argument{
            "config field '" + k + "': unknown transport '" + raw + R"(' (expected "tcp"|"tls"|"quic"))"};
    }

    class ListenerConfig::Builder {
    public:
        Builder() = default;

        template <typename Self>
        auto&& bind_address(this Self&& self, std::string value) noexcept {
            self.config_.bind_address_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& port(this Self&& self, const std::uint16_t value) noexcept {
            self.config_.port_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& transport(this Self&& self, const Transport value) noexcept {
            self.config_.transport_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& protocols(this Self&& self, std::vector<Protocol> value) noexcept {
            self.config_.protocols_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& tls(this Self&& self, TlsConfig value) noexcept {
            self.config_.tls_ = std::move(value);
            return std::forward<Self>(self);
        }

        [[nodiscard]] ListenerConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class ListenerConfig;
        friend class ConfigInterface;
        ListenerConfig config_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 4: Write the CMake leaf + wire the aggregate**

Create `components/http/config/listener_config/CMakeLists.txt`:

```cmake
##############################################################################
# Http Config — ListenerConfig (+ Protocol/Transport string codecs; PR 6)
##############################################################################
add_library(${DMP_HTTP}.Config.ListenerConfig INTERFACE listener_config.hpp)

target_include_directories(${DMP_HTTP}.Config.ListenerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Config.ListenerConfig INTERFACE
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Types.Enums
        Demiplane::Common::Serialization
)
##############################################################################
```

In `components/http/config/CMakeLists.txt`, extend the subdirectory list and aggregate:

```cmake
add_subdirectory(tls_config)
add_subdirectory(server_config)
add_subdirectory(timeouts)
add_subdirectory(listener_config)
```

and

```cmake
target_link_libraries(${DMP_HTTP}.Config INTERFACE
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Config.ServerConfig
        ${DMP_HTTP}.Config.Timeouts
        ${DMP_HTTP}.Config.ListenerConfig
)
```

- [ ] **Step 5: Build + run**

Run: `cmake --preset debug 2>&1 | tail -3 && cmake --build build/debug --target Demiplane.Tests.Unit.Http.Config -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Demiplane.Tests.Unit.Http.Config"`
Expected: build clean; 14 tests PASS (4 Timeouts + 5 TlsConfig + 5 ListenerConfig).

- [ ] **Step 6: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add components/http/config/listener_config components/http/config/CMakeLists.txt tests/unit_tests/http/config/test_listener_config.cpp tests/unit_tests/http/CMakeLists.txt
# suggested message: "http/config: ListenerConfig leaf + Protocol/Transport string codecs"
```

---

### Task 5: `ServerConfig` rewrite in place + every consumer

The widest task: the plain struct becomes Builder-only (D6), so all 9 `ServerConfig{}`/field-access sites move to
Builders/accessors, and the Server's latent moved-from-parameter read gets fixed (D8).

**Files:**

- Modify (rewrite): `components/http/config/server_config/server_config.hpp`
- Modify: `components/http/config/server_config/CMakeLists.txt`
- Modify: `components/http/server/server/server.hpp`
- Modify: `components/http/server/server/server.cpp`
- Modify: `tests/integration_tests/http/server_test_fixture.hpp`
- Modify: `tests/integration_tests/http/test_http_server_lifecycle.cpp`
- Modify: `tests/integration_tests/http/test_http_server_concurrency.cpp`
- Modify: `tests/integration_tests/http/test_http_run_standalone.cpp`
- Create: `tests/unit_tests/http/config/test_server_config.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 2's `Timeouts` (nested member — its private framework ctor forces the member default through the
  public full ctor), Task 4's `ListenerConfig` (vector member), Task 1's machinery.
- Produces: `demiplane::http::ServerConfig` — accessors `listeners()` → `const std::vector<ListenerConfig>&`,
  `threads()` → `std::size_t` (default 1), `timeouts()` → `const Timeouts&`, `body_limit()` → `std::size_t`
  (default 16 MB), `request_arena_size()` → `std::size_t` (default 8192), `drain_timeout()` →
  `std::chrono::milliseconds` (default 30 s), `path_normalization()` → `ServerConfig::PathNormalization` (default
  `collapse_trailing_slash`; enum unchanged — `Server::map_normalization` keeps compiling); **Builder-only** (D6)
  with setters `listeners(vector)`, `add_listener(ListenerConfig)`, `threads(n)`, `timeouts(Timeouts)`,
  `body_limit(n)`, `request_arena_size(n)`, `drain_timeout(ms)`, `path_normalization(p)`; JSON names
  `listeners`/`threads`/`timeouts`/`body_limit`/`request_arena_size`/`drain_timeout_ms`/`path_normalization`
  (string-encoded, D2). Consumed by Tasks 6–9 and every existing Server call site.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/config/test_server_config.cpp`:

```cpp
#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <vector>

#include <http_enums.hpp>
#include <json/json.hpp>
#include <listener_config.hpp>
#include <server_config.hpp>
#include <timeouts.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;
using namespace std::chrono_literals;

TEST(ServerConfigTest, DefaultsMatchSpec) {
    const auto cfg = ServerConfig::Builder{}.finalize();
    EXPECT_TRUE(cfg.listeners().empty());
    EXPECT_EQ(cfg.threads(), 1u);
    EXPECT_EQ(cfg.timeouts().header(), 10s);
    EXPECT_EQ(cfg.timeouts().body(), 30s);
    EXPECT_EQ(cfg.timeouts().idle(), 60s);
    EXPECT_EQ(cfg.body_limit(), 16u * 1024 * 1024);
    EXPECT_EQ(cfg.request_arena_size(), 8192u);
    EXPECT_EQ(cfg.drain_timeout(), 30s);
    EXPECT_EQ(cfg.path_normalization(), ServerConfig::PathNormalization::collapse_trailing_slash);
}

TEST(ServerConfigTest, RoundTripPreservesNestedListeners) {
    const auto cfg =
        ServerConfig::Builder{}
            .threads(4)
            .body_limit(65536)
            .request_arena_size(16384)
            .drain_timeout(5s)
            .path_normalization(ServerConfig::PathNormalization::collapse_multi_slash)
            .timeouts(Timeouts{5s, 10s, 30s})
            .add_listener(ListenerConfig::Builder{}.bind_address("127.0.0.1").port(8080).finalize())
            .add_listener(ListenerConfig::Builder{}
                              .bind_address("127.0.0.1")
                              .port(8443)
                              .transport(ListenerConfig::Transport::tls)
                              .protocols({Protocol::http1})
                              .tls(TlsConfig::Builder{}.cert_file("c.pem").key_file("k.pem").finalize())
                              .finalize())
            .finalize();

    const Json::Value json = cfg.serialize<Json::Value>();
    EXPECT_EQ(json["path_normalization"].asString(), "collapse_multi_slash");
    ASSERT_TRUE(json["listeners"].isArray());
    ASSERT_EQ(json["listeners"].size(), 2u);

    const auto back = ServerConfig::deserialize<Json::Value>(json);
    EXPECT_EQ(back.threads(), 4u);
    EXPECT_EQ(back.body_limit(), 65536u);
    EXPECT_EQ(back.request_arena_size(), 16384u);
    EXPECT_EQ(back.drain_timeout(), 5s);
    EXPECT_EQ(back.path_normalization(), ServerConfig::PathNormalization::collapse_multi_slash);
    EXPECT_EQ(back.timeouts().header(), 5s);
    ASSERT_EQ(back.listeners().size(), 2u);
    EXPECT_EQ(back.listeners()[0].transport(), ListenerConfig::Transport::tcp);
    EXPECT_EQ(back.listeners()[1].transport(), ListenerConfig::Transport::tls);
    ASSERT_TRUE(back.listeners()[1].tls().has_value());
    EXPECT_EQ(back.listeners()[1].tls()->cert_file(), "c.pem");

    // serialize → deserialize → serialize is a fixed point (Json::Value ==).
    EXPECT_EQ(back.serialize<Json::Value>(), json);
}

TEST(ServerConfigTest, UnknownPathNormalizationStringThrows) {
    Json::Value json{Json::objectValue};
    json["path_normalization"] = "collapse_everything";
    EXPECT_THROW((void)ServerConfig::deserialize<Json::Value>(json), std::invalid_argument);
}

TEST(ServerConfigTest, ValidateRejectsBadScalars) {
    EXPECT_THROW((void)ServerConfig::Builder{}.threads(0).finalize(), std::invalid_argument);
    EXPECT_THROW((void)ServerConfig::Builder{}.body_limit(0).finalize(), std::invalid_argument);
    EXPECT_THROW((void)ServerConfig::Builder{}.request_arena_size(0).finalize(), std::invalid_argument);
    EXPECT_THROW((void)ServerConfig::Builder{}.drain_timeout(-1ms).finalize(), std::invalid_argument);
}

TEST(ServerConfigTest, DeserializeRejectsInvalidNestedListener) {
    // tls transport without material: rejected on the loading path (the
    // element's own deserialize→finalize→validate chain).
    Json::Value listener{Json::objectValue};
    listener["transport"] = "tls";
    Json::Value json{Json::objectValue};
    json["listeners"] = Json::Value{Json::arrayValue};
    json["listeners"].append(listener);
    EXPECT_THROW((void)ServerConfig::deserialize<Json::Value>(json), std::invalid_argument);
}
```

Add the source to the target in `tests/unit_tests/http/CMakeLists.txt`:

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Config
        config/test_timeouts.cpp
        config/test_tls_config.cpp
        config/test_listener_config.cpp
        config/test_server_config.cpp
)
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Config -- -j4 2>&1 | tail -5`
Expected: FAIL — `ServerConfig` has no `Builder`/accessors (still the plain struct).

- [ ] **Step 3: Rewrite the header in place**

Replace the entire contents of `components/http/config/server_config/server_config.hpp` with:

```cpp
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <config_interface.hpp>
#include <json/json.hpp>
#include <listener_config.hpp>
#include <timeouts.hpp>

namespace demiplane::http {

    /**
     * @brief Server-level configuration (spec §10.1), JSON-loadable via
     *        serialization::ConfigInterface (PR 6; supersedes the PR 5 plain
     *        struct at this same path).
     *
     * `threads` is consumed only by run_standalone-style callers — the
     * injected-executor path takes its threads from whoever drives the
     * executor (spec §10.3). `path_normalization` mirrors routing's enum; the
     * Server maps it (map_normalization) so the config layer carries no
     * routing dependency. Builder-only construction: every default is valid,
     * so `ServerConfig::Builder{}.finalize()` is the canonical empty config.
     */
    class ServerConfig final : public serialization::ConfigInterface<ServerConfig, Json::Value> {
    public:
        enum class PathNormalization : std::uint8_t {
            none,                     ///< exact byte match
            collapse_trailing_slash,  ///< "/users/" == "/users"   (default)
            collapse_multi_slash,     ///< + "/users//42" == "/users/42"
        };

        void validate() const override {
            if (threads_ == 0) {
                throw std::invalid_argument("server.threads must be >= 1");
            }
            if (body_limit_ == 0) {
                throw std::invalid_argument("server.body_limit must be positive");
            }
            if (request_arena_size_ == 0) {
                throw std::invalid_argument("server.request_arena_size must be positive");
            }
            if (drain_timeout_ < std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("server.drain_timeout_ms must be non-negative");
            }
            timeouts_.validate();
            for (const auto& listener : listeners_) {
                listener.validate();
            }
        }

        [[nodiscard]] const std::vector<ListenerConfig>& listeners() const noexcept {
            return listeners_;
        }
        [[nodiscard]] constexpr std::size_t threads() const noexcept {
            return threads_;
        }
        [[nodiscard]] constexpr const Timeouts& timeouts() const noexcept {
            return timeouts_;
        }
        [[nodiscard]] constexpr std::size_t body_limit() const noexcept {
            return body_limit_;
        }
        [[nodiscard]] constexpr std::size_t request_arena_size() const noexcept {
            return request_arena_size_;
        }
        [[nodiscard]] constexpr std::chrono::milliseconds drain_timeout() const noexcept {
            return drain_timeout_;
        }
        [[nodiscard]] constexpr PathNormalization path_normalization() const noexcept {
            return path_normalization_;
        }

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&ServerConfig::listeners_, "listeners">{},
                serialization::Field<&ServerConfig::threads_, "threads">{},
                serialization::Field<&ServerConfig::timeouts_, "timeouts">{},
                serialization::Field<&ServerConfig::body_limit_, "body_limit">{},
                serialization::Field<&ServerConfig::request_arena_size_, "request_arena_size">{},
                serialization::Field<&ServerConfig::drain_timeout_, "drain_timeout_ms">{},
                serialization::Field<&ServerConfig::path_normalization_, "path_normalization">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        ServerConfig() = default;

        std::vector<ListenerConfig> listeners_{};
        std::size_t threads_ = 1;
        // Timeouts' framework default ctor is private — the member default
        // goes through its public full ctor.
        Timeouts timeouts_ =
            Timeouts{std::chrono::seconds{10}, std::chrono::seconds{30}, std::chrono::seconds{60}};
        std::size_t body_limit_         = 16 * 1024 * 1024;
        std::size_t request_arena_size_ = 8192;
        std::chrono::milliseconds drain_timeout_{std::chrono::seconds{30}};
        PathNormalization path_normalization_ = PathNormalization::collapse_trailing_slash;
    };

    // ── PathNormalization <-> string codec ───────────────────────────────

    [[nodiscard]] constexpr std::string_view to_string(const ServerConfig::PathNormalization p) noexcept {
        switch (p) {
            case ServerConfig::PathNormalization::none:
                return "none";
            case ServerConfig::PathNormalization::collapse_multi_slash:
                return "collapse_multi_slash";
            case ServerConfig::PathNormalization::collapse_trailing_slash:
                return "collapse_trailing_slash";
        }
        return "collapse_trailing_slash";
    }

    inline void write_field(Json::Value& out, const serialization::FieldName key,
                            const ServerConfig::PathNormalization v) {
        out[key.str()] = std::string{to_string(v)};
    }

    inline bool read_field(const Json::Value& in, const serialization::FieldName key,
                           ServerConfig::PathNormalization& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const std::string raw = in[k].asString();  // Json::LogicError on non-string
        if (raw == "none") {
            v = ServerConfig::PathNormalization::none;
            return true;
        }
        if (raw == "collapse_trailing_slash") {
            v = ServerConfig::PathNormalization::collapse_trailing_slash;
            return true;
        }
        if (raw == "collapse_multi_slash") {
            v = ServerConfig::PathNormalization::collapse_multi_slash;
            return true;
        }
        throw std::invalid_argument{"config field '" + k + "': unknown path_normalization '" + raw +
                                    R"(' (expected "none"|"collapse_trailing_slash"|"collapse_multi_slash"))"};
    }

    class ServerConfig::Builder {
    public:
        Builder() = default;

        template <typename Self>
        auto&& listeners(this Self&& self, std::vector<ListenerConfig> value) noexcept {
            self.config_.listeners_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& add_listener(this Self&& self, ListenerConfig value) noexcept {
            self.config_.listeners_.push_back(std::move(value));
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& threads(this Self&& self, const std::size_t value) noexcept {
            self.config_.threads_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& timeouts(this Self&& self, Timeouts value) noexcept {
            self.config_.timeouts_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& body_limit(this Self&& self, const std::size_t value) noexcept {
            self.config_.body_limit_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& request_arena_size(this Self&& self, const std::size_t value) noexcept {
            self.config_.request_arena_size_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& drain_timeout(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.drain_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& path_normalization(this Self&& self, const PathNormalization value) noexcept {
            self.config_.path_normalization_ = value;
            return std::forward<Self>(self);
        }

        [[nodiscard]] ServerConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class ServerConfig;
        friend class ConfigInterface;
        ServerConfig config_;
    };

}  // namespace demiplane::http
```

- [ ] **Step 4: Update the ServerConfig CMake leaf**

Replace the contents of `components/http/config/server_config/CMakeLists.txt` with:

```cmake
##############################################################################
# Http Config — ServerConfig (ConfigInterface; JSON-loadable; PR 6)
##############################################################################
add_library(${DMP_HTTP}.Config.ServerConfig INTERFACE server_config.hpp)

target_include_directories(${DMP_HTTP}.Config.ServerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Config.ServerConfig INTERFACE
        ${DMP_HTTP}.Config.ListenerConfig
        ${DMP_HTTP}.Config.Timeouts
        Demiplane::Common::Serialization
)
##############################################################################
```

- [ ] **Step 5: Switch the Server to accessors (+ D8 fix)**

In `components/http/server/server/server.hpp` (`add_tcp_listener`), replace

```cpp
            listeners_.push_back(std::make_unique<TcpListener<Driver>>(
                exec_, std::move(host), port, std::move(driver), cfg_.request_arena_size));
```

with

```cpp
            listeners_.push_back(std::make_unique<TcpListener<Driver>>(
                exec_, std::move(host), port, std::move(driver), cfg_.request_arena_size()));
```

In `components/http/server/server/server.cpp`, replace the constructor

```cpp
    Server::Server(ServerConfig cfg, boost::asio::any_io_executor exec)
        : cfg_{std::move(cfg)},
          exec_{std::move(exec)},
          registry_{map_normalization(cfg.path_normalization)} {
    }
```

with

```cpp
    Server::Server(ServerConfig cfg, boost::asio::any_io_executor exec)
        : cfg_{std::move(cfg)},
          exec_{std::move(exec)},
          // Read the MEMBER, not the moved-from parameter (D8): members
          // initialize in declaration order, so cfg_ is populated by now.
          registry_{map_normalization(cfg_.path_normalization())} {
    }
```

and in `graceful_shutdown()` (line ~220) replace

```cpp
            const auto deadline = std::chrono::steady_clock::now() + cfg_.drain_timeout;
```

with

```cpp
            const auto deadline = std::chrono::steady_clock::now() + cfg_.drain_timeout();
```

(`map_normalization`'s switch over `ServerConfig::PathNormalization` at server.cpp:343-350 keeps compiling — the
nested enum's shape is unchanged.)

- [ ] **Step 6: Update the integration fixture + the three test files**

In `tests/integration_tests/http/server_test_fixture.hpp`, replace

```cpp
        void start_server(const std::function<void(demiplane::http::Server&)>& configure,
                          demiplane::http::ServerConfig cfg = {},
                          const std::size_t io_threads      = 1) {
```

with

```cpp
        void start_server(const std::function<void(demiplane::http::Server&)>& configure,
                          demiplane::http::ServerConfig cfg = demiplane::http::ServerConfig::Builder{}.finalize(),
                          const std::size_t io_threads      = 1) {
```

In `tests/integration_tests/http/test_http_server_lifecycle.cpp`, replace **all four** occurrences of
`ServerConfig{}` (lines 72, 89, 114, 125 — `Server server{ServerConfig{}, ioc_.get_executor()};` and
`server_.emplace(ServerConfig{}, ioc_.get_executor());`) with `ServerConfig::Builder{}.finalize()`, e.g.:

```cpp
    Server server{ServerConfig::Builder{}.finalize(), ioc_.get_executor()};
```

and replace the drain-deadline construction (lines 172-173)

```cpp
    ServerConfig cfg;
    cfg.drain_timeout = std::chrono::milliseconds{100};  // << the 500ms /hang handler
```

with

```cpp
    const auto cfg = ServerConfig::Builder{}
                         .drain_timeout(std::chrono::milliseconds{100})  // << the 500ms /hang handler
                         .finalize();
```

In `tests/integration_tests/http/test_http_server_concurrency.cpp` (line 58) replace the `ServerConfig{},` argument
with `ServerConfig::Builder{}.finalize(),`.

In `tests/integration_tests/http/test_http_run_standalone.cpp` replace both occurrences (lines 57, 131):

```cpp
                run_standalone(ServerConfig::Builder{}.finalize(), threads, [this, with_stop_route](Server& s) {
```

and

```cpp
    EXPECT_THROW(run_standalone(ServerConfig::Builder{}.finalize(), 0, [](Server&) {}), std::invalid_argument);
```

- [ ] **Step 7: Build + run everything ServerConfig touches**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Config Demiplane.Tests.Integration.Http.Server -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Demiplane.Tests.Unit.Http.Config|Demiplane.Tests.Integration.Http.Server"`
Expected: build clean; 19 config unit tests + the full Server integration battery (lifecycle, observer,
run_standalone, concurrency) PASS.

- [ ] **Step 8: Verify no plain-struct usage remains**

Run: `grep -rn "ServerConfig{}\|cfg_\.request_arena_size\b\|cfg_\.drain_timeout\b\|\.path_normalization\b" components tests --include="*.cpp" --include="*.hpp" | grep -v "()"`
Expected: no output.

- [ ] **Step 9: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add components/http/config/server_config components/http/server/server tests/integration_tests/http/server_test_fixture.hpp tests/integration_tests/http/test_http_server_lifecycle.cpp tests/integration_tests/http/test_http_server_concurrency.cpp tests/integration_tests/http/test_http_run_standalone.cpp tests/unit_tests/http/config/test_server_config.cpp tests/unit_tests/http/CMakeLists.txt
# suggested message: "http/config: ServerConfig -> ConfigInterface (listeners/threads/timeouts/body_limit) + Server accessor switch + moved-from ctor fix"
```

---

### Task 6: Wire `request_arena_size` through `TlsListener`

Resolves the staged PR 6 notes at `tls_listener.hpp:47-48` and `server.hpp:112-114`. `TlsConnection` already accepts
the arena size (landed fact 6) — this is ctor plumbing.

**Files:**

- Modify: `components/http/listeners/tls_listener/tls_listener.hpp`
- Modify: `components/http/server/server/server.hpp`
- Modify: `tests/unit_tests/http/listeners/test_tls_listener.cpp`

**Interfaces:**

- Consumes: Task 5's `cfg_.request_arena_size()`.
- Produces: `TlsListener<Drivers...>{exec, host, port, TlsConfig, std::size_t request_arena_size, Drivers...}` —
  new delegated-to ctor (D7); the existing `{exec, host, port, TlsConfig, Drivers...}` form delegates with `8192`
  (all current direct constructions keep compiling). Overload resolution is unambiguous: `Drivers...` is fixed by
  the class instantiation, so the two ctors differ by exactly the `std::size_t` parameter.

- [ ] **Step 1: Write the failing test**

Append to `tests/unit_tests/http/listeners/test_tls_listener.cpp`:

```cpp
TEST(TlsListenerTest, ConstructsWithExplicitArenaSize) {
    boost::asio::io_context ioc;
    TlsConfig tls{"", ""};  // full-ctor escape hatch; bind() (ctx build) is not called
    TlsListener<Http11Driver> listener{
        ioc.get_executor(), "127.0.0.1", 0, tls, 4096, Http11Driver{Http11Config{}}};
    EXPECT_EQ(listener.bind_address(), "127.0.0.1");
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Listeners -- -j4 2>&1 | tail -5`
Expected: FAIL — no matching `TlsListener` constructor taking the `std::size_t` argument.

- [ ] **Step 3: Add the arena ctor + plumb it to the connection**

In `components/http/listeners/tls_listener/tls_listener.hpp`, replace the single constructor

```cpp
        TlsListener(boost::asio::any_io_executor exec,
                    std::string host,
                    const std::uint16_t port,
                    TlsConfig tls,
                    Drivers... drivers)
            : exec_{std::move(exec)},
              host_{std::move(host)},
              port_{port},
              tls_config_{std::move(tls)},
              drivers_{std::move(drivers)...},
              advertised_alpn_{build_alpn_wire()},
              acceptor_{exec_} {
        }
```

with the delegating pair

```cpp
        TlsListener(boost::asio::any_io_executor exec,
                    std::string host,
                    const std::uint16_t port,
                    TlsConfig tls,
                    Drivers... drivers)
            : TlsListener{std::move(exec), std::move(host), port, std::move(tls), 8192, std::move(drivers)...} {
        }

        TlsListener(boost::asio::any_io_executor exec,
                    std::string host,
                    const std::uint16_t port,
                    TlsConfig tls,
                    const std::size_t request_arena_size,
                    Drivers... drivers)
            : exec_{std::move(exec)},
              host_{std::move(host)},
              port_{port},
              tls_config_{std::move(tls)},
              arena_size_{request_arena_size},
              drivers_{std::move(drivers)...},
              advertised_alpn_{build_alpn_wire()},
              acceptor_{exec_} {
        }
```

Add the member between `tls_config_` and `drivers_` (declaration order = init order):

```cpp
        TlsConfig tls_config_;
        std::size_t arena_size_;
        std::tuple<Drivers...> drivers_;
```

In `run()`, pass it to the connection — replace

```cpp
                auto conn          = std::make_shared<TlsConnection>(std::move(sock), *ctx_);
```

with

```cpp
                auto conn          = std::make_shared<TlsConnection>(std::move(sock), *ctx_, arena_size_);
```

And in the class doc comment, replace

```cpp
     * a tracker Handle into this listener + serve via `this`). Arena size is fixed at the
     * 8 KB default in v1 (PR 6 wires request_arena_size).
```

with

```cpp
     * a tracker Handle into this listener + serve via `this`). Each connection's
     * request arena is sized by the ctor's request_arena_size (the Server passes
     * ServerConfig::request_arena_size(); the driver-list ctor defaults to 8 KB).
```

- [ ] **Step 4: Forward the configured size from the Server**

In `components/http/server/server/server.hpp`, replace

```cpp
        template <IsHttpDriver... Drivers>
        Server& add_tls_listener(std::string host, const std::uint16_t port, TlsConfig tls, Drivers... drivers) {
            require_build("add_tls_listener");
            // Arena size stays at the TlsListener 8 KB default until PR 6
            // wires request_arena_size through (PR 4 note in tls_listener.hpp).
            listeners_.push_back(std::make_unique<TlsListener<Drivers...>>(
                exec_, std::move(host), port, std::move(tls), std::move(drivers)...));
            return *this;
        }
```

with

```cpp
        template <IsHttpDriver... Drivers>
        Server& add_tls_listener(std::string host, const std::uint16_t port, TlsConfig tls, Drivers... drivers) {
            require_build("add_tls_listener");
            listeners_.push_back(std::make_unique<TlsListener<Drivers...>>(
                exec_, std::move(host), port, std::move(tls), cfg_.request_arena_size(), std::move(drivers)...));
            return *this;
        }
```

- [ ] **Step 5: Build + run**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Listeners Demiplane.Tests.Integration.Http.Tls Demiplane.Tests.Integration.Http.Server -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Http.Listeners|Integration.Http.Tls|Integration.Http.Server"`
Expected: build clean; all PASS (the delegating ctor keeps every existing direct construction compiling).

- [ ] **Step 6: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add components/http/listeners/tls_listener/tls_listener.hpp components/http/server/server/server.hpp tests/unit_tests/http/listeners/test_tls_listener.cpp
# suggested message: "http/listeners: wire request_arena_size through TlsListener (resolves PR6 staging note)"
```

---

### Task 7: `load_server_config` + `dump_server_config` + config error types

**Files:**

- Create: `components/http/config/load_server_config/load_server_config.hpp`
- Create: `components/http/config/load_server_config/load_server_config.cpp`
- Create: `components/http/config/load_server_config/CMakeLists.txt`
- Modify: `components/http/config/CMakeLists.txt`
- Create: `tests/unit_tests/http/config/test_load_server_config.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 5's `ServerConfig::deserialize<Json::Value>`/`serialize<Json::Value>`, `gears::Outcome`,
  `ResponseFactory::internal_error` (types layer).
- Produces: `demiplane::http::load_server_config(std::string_view path)` →
  `gears::Outcome<ServerConfig, ConfigFileError, ConfigParseError, ConfigSchemaError>`;
  `dump_server_config(const ServerConfig&)` → `Json::Value` (validates via `serialize()`); error structs
  `ConfigFileError{path, reason}`, `ConfigParseError{path, line, detail}`, `ConfigSchemaError{path, field_path,
  detail}` (field_path best-effort — D4); `to_http_response` for all three → 500 (cold path, global heap, matches
  `errors.cpp`). Consumed by Task 9's wiring test and by applications.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/config/test_load_server_config.cpp`:

```cpp
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

#include <body.hpp>
#include <http_enums.hpp>
#include <json/json.hpp>
#include <listener_config.hpp>
#include <load_server_config.hpp>
#include <response.hpp>
#include <server_config.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;
using namespace std::chrono_literals;

namespace {

    std::string write_temp(const std::string_view stem, const std::string_view contents) {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            ("dmp_http_cfg_" + std::to_string(::getpid()) + "_" + std::string{stem});
        std::ofstream out{path, std::ios::binary | std::ios::trunc};
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return path.string();
    }

    std::string body_of(const Response& r) {
        return std::string{r.body.buffered_view().value_or("")};
    }

}  // namespace

TEST(LoadServerConfigTest, LoadsValidConfig) {
    const auto path = write_temp("valid.json", R"({
        "threads": 4,
        "body_limit": 65536,
        "request_arena_size": 16384,
        "drain_timeout_ms": 5000,
        "path_normalization": "collapse_multi_slash",
        "timeouts": { "header_ms": 5000, "body_ms": 10000, "idle_ms": 30000 },
        "listeners": [
            { "bind": "127.0.0.1", "port": 8080, "transport": "tcp", "protocols": ["http1"] },
            { "bind": "127.0.0.1", "port": 8443, "transport": "tls", "protocols": ["http2", "http1"],
              "tls": { "cert_file": "c.pem", "key_file": "k.pem", "min_version": "tls13" } }
        ]
    })");
    auto outcome = load_server_config(path);
    ASSERT_TRUE(outcome.is_success());
    const auto& cfg = outcome.value();
    EXPECT_EQ(cfg.threads(), 4u);
    EXPECT_EQ(cfg.body_limit(), 65536u);
    EXPECT_EQ(cfg.request_arena_size(), 16384u);
    EXPECT_EQ(cfg.drain_timeout(), 5000ms);
    EXPECT_EQ(cfg.path_normalization(), ServerConfig::PathNormalization::collapse_multi_slash);
    EXPECT_EQ(cfg.timeouts().header(), 5000ms);
    EXPECT_EQ(cfg.timeouts().body(), 10000ms);
    EXPECT_EQ(cfg.timeouts().idle(), 30000ms);
    ASSERT_EQ(cfg.listeners().size(), 2u);
    EXPECT_EQ(cfg.listeners()[0].transport(), ListenerConfig::Transport::tcp);
    EXPECT_EQ(cfg.listeners()[0].port(), 8080);
    const auto& tls_listener = cfg.listeners()[1];
    EXPECT_EQ(tls_listener.effective_protocols(), (std::vector{Protocol::http2, Protocol::http1}));
    ASSERT_TRUE(tls_listener.tls().has_value());
    EXPECT_EQ(tls_listener.tls()->min_version(), TlsConfig::MinVersion::tls13);
}

TEST(LoadServerConfigTest, MissingFileIsFileError) {
    auto outcome = load_server_config("/nonexistent/dir/server.json");
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<ConfigFileError>());
    EXPECT_EQ(outcome.error<ConfigFileError>().path, "/nonexistent/dir/server.json");
}

TEST(LoadServerConfigTest, DirectoryIsFileError) {
    auto outcome = load_server_config(std::filesystem::temp_directory_path().string());
    ASSERT_TRUE(outcome.is_error());
    EXPECT_TRUE(outcome.holds_error<ConfigFileError>());
}

TEST(LoadServerConfigTest, MalformedJsonIsParseErrorWithLine) {
    const auto path = write_temp("malformed.json", R"({"threads": })");
    auto outcome    = load_server_config(path);
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<ConfigParseError>());
    const auto& err = outcome.error<ConfigParseError>();
    EXPECT_EQ(err.line, 1u);
    EXPECT_FALSE(err.detail.empty());
}

TEST(LoadServerConfigTest, NonObjectTopLevelIsSchemaError) {
    const auto path = write_temp("toplevel.json", "42");
    auto outcome    = load_server_config(path);
    ASSERT_TRUE(outcome.is_error());
    EXPECT_TRUE(outcome.holds_error<ConfigSchemaError>());
}

TEST(LoadServerConfigTest, TypeMismatchIsSchemaError) {
    const auto path = write_temp("mismatch.json", R"({"threads": "four"})");
    auto outcome    = load_server_config(path);
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<ConfigSchemaError>());
    EXPECT_NE(outcome.error<ConfigSchemaError>().detail.find("type mismatch"), std::string::npos);
}

TEST(LoadServerConfigTest, UnknownEnumStringIsSchemaErrorNamingField) {
    const auto path = write_temp("enum.json", R"({"path_normalization": "collapse_everything"})");
    auto outcome    = load_server_config(path);
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<ConfigSchemaError>());
    EXPECT_NE(outcome.error<ConfigSchemaError>().detail.find("path_normalization"), std::string::npos);
}

TEST(LoadServerConfigTest, ValidateFailureIsSchemaErrorNamingField) {
    const auto path = write_temp("novalidate.json", R"({"listeners": [{"transport": "tls"}]})");
    auto outcome    = load_server_config(path);
    ASSERT_TRUE(outcome.is_error());
    ASSERT_TRUE(outcome.holds_error<ConfigSchemaError>());
    EXPECT_NE(outcome.error<ConfigSchemaError>().detail.find("listener.tls"), std::string::npos);
}

TEST(LoadServerConfigTest, DumpLoadRoundTripIsAFixedPoint) {
    const auto cfg = ServerConfig::Builder{}
                         .threads(2)
                         .add_listener(ListenerConfig::Builder{}.bind_address("127.0.0.1").port(9090).finalize())
                         .finalize();
    const Json::Value dumped = dump_server_config(cfg);
    const auto path          = write_temp("roundtrip.json", dumped.toStyledString());
    auto outcome             = load_server_config(path);
    ASSERT_TRUE(outcome.is_success());
    EXPECT_EQ(dump_server_config(outcome.value()), dumped);
}

TEST(LoadServerConfigTest, ErrorsRenderAs500) {
    const auto file = to_http_response(ConfigFileError{"/etc/x.json", "cannot open"});
    EXPECT_EQ(file.status, HttpStatus::internal_server_error);
    EXPECT_NE(body_of(file).find("/etc/x.json"), std::string::npos);

    const auto parse = to_http_response(ConfigParseError{"/etc/x.json", 3, "syntax"});
    EXPECT_EQ(parse.status, HttpStatus::internal_server_error);
    EXPECT_NE(body_of(parse).find("line 3"), std::string::npos);

    const auto schema = to_http_response(ConfigSchemaError{"/etc/x.json", "", "threads must be >= 1"});
    EXPECT_EQ(schema.status, HttpStatus::internal_server_error);
    EXPECT_NE(body_of(schema).find("threads"), std::string::npos);
}
```

Add the source to the target in `tests/unit_tests/http/CMakeLists.txt`:

```cmake
add_unit_test(${UNIT_TESTING_TARGET}.Http.Config
        config/test_timeouts.cpp
        config/test_tls_config.cpp
        config/test_listener_config.cpp
        config/test_server_config.cpp
        config/test_load_server_config.cpp
)
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build/debug --target Demiplane.Tests.Unit.Http.Config -- -j4 2>&1 | tail -5`
Expected: FAIL — `load_server_config.hpp: No such file or directory`.

- [ ] **Step 3: Write the header**

Create `components/http/config/load_server_config/load_server_config.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include <demiplane/gears>
#include <json/json.h>

#include <server_config.hpp>

namespace demiplane::http {

    struct Response;  // conversions return it; defined in types/response

    /// Filesystem-level failure: missing / unreadable / not a regular file.
    struct ConfigFileError {
        std::string path;
        std::string reason;
    };

    /// Malformed JSON. `line` is best-effort (0 when it cannot be extracted
    /// from the reader's message); `detail` carries jsoncpp's full message.
    struct ConfigParseError {
        std::string path;
        std::size_t line = 0;
        std::string detail;
    };

    /// Well-formed JSON that does not describe a valid ServerConfig: a type
    /// mismatch, an unknown enum string, or a validate() failure. field_path
    /// is best-effort (often empty — `detail` names the offending field; full
    /// /listeners/1/tls/cert_file pointers would need path threading through
    /// every read_field overload, deferred — plan D4).
    struct ConfigSchemaError {
        std::string path;
        std::string field_path;
        std::string detail;
    };

    /**
     * @brief Load + validate a ServerConfig from a JSON file (spec §10.2).
     *
     * 1. Open the file (must be a regular file)   → ConfigFileError
     * 2. Parse JSON (Json::CharReader pipeline)   → ConfigParseError (+line)
     * 3. Deserialize via the fields() machinery   → ConfigSchemaError
     *    (type mismatch / unknown enum string)
     * 4. validate() (run by Builder::finalize)    → ConfigSchemaError
     *
     * Unknown JSON keys are ignored (the fields() walk reads known names
     * only). An empty file is a parse error, not an empty config.
     */
    gears::Outcome<ServerConfig, ConfigFileError, ConfigParseError, ConfigSchemaError> load_server_config(
        std::string_view path);

    /// Round-trip companion (spec §10.2): serialize — which validates first.
    /// Secret fields (tls.key_passphrase) are omitted by policy (plan D3).
    Json::Value dump_server_config(const ServerConfig& cfg);

    // ADL conversions (spec §10.2): config errors surfaced on admin endpoints.
    // Cold path — global heap via the static ResponseFactory (errors.cpp
    // convention); all three render as 500.
    Response to_http_response(const ConfigFileError& e);
    Response to_http_response(const ConfigParseError& e);
    Response to_http_response(const ConfigSchemaError& e);

}  // namespace demiplane::http
```

- [ ] **Step 4: Write the implementation**

Create `components/http/config/load_server_config/load_server_config.cpp`:

```cpp
#include "load_server_config.hpp"

#include <cerrno>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

#include <response.hpp>
#include <response_factory.hpp>

namespace demiplane::http {

    namespace {
        /// jsoncpp formats parse failures as "* Line 3, Column 5\n  ...".
        std::size_t parse_error_line(const std::string& errs) noexcept {
            const auto pos = errs.find("Line ");
            if (pos == std::string::npos) {
                return 0;
            }
            std::size_t line     = 0;
            const char* first    = errs.data() + pos + 5;
            const char* last     = errs.data() + errs.size();
            const auto [ptr, ec] = std::from_chars(first, last, line);
            return ec == std::errc{} ? line : 0;
        }
    }  // namespace

    gears::Outcome<ServerConfig, ConfigFileError, ConfigParseError, ConfigSchemaError> load_server_config(
        const std::string_view path) {
        std::string path_str{path};

        std::error_code fs_ec;
        if (!std::filesystem::is_regular_file(path_str, fs_ec)) {
            return gears::err(ConfigFileError{std::move(path_str), fs_ec ? fs_ec.message() : "not a regular file"});
        }

        std::ifstream file{path_str, std::ios::binary};
        if (!file.is_open()) {
            const std::error_code open_ec{errno, std::generic_category()};
            return gears::err(ConfigFileError{std::move(path_str), open_ec.message()});
        }

        Json::CharReaderBuilder reader;
        Json::Value root;
        std::string errs;
        if (!Json::parseFromStream(reader, file, &root, &errs)) {
            const std::size_t line = parse_error_line(errs);
            return gears::err(ConfigParseError{std::move(path_str), line, std::move(errs)});
        }
        if (!root.isObject()) {
            return gears::err(ConfigSchemaError{std::move(path_str), "", "top-level JSON value must be an object"});
        }

        try {
            return ServerConfig::deserialize<Json::Value>(root);
        } catch (const Json::Exception& e) {  // asString()/asInt64()/... type mismatch
            return gears::err(ConfigSchemaError{std::move(path_str), "", std::string{"type mismatch: "} + e.what()});
        } catch (const std::invalid_argument& e) {  // enum codec / validate()
            return gears::err(ConfigSchemaError{std::move(path_str), "", e.what()});
        }
    }

    Json::Value dump_server_config(const ServerConfig& cfg) {
        return cfg.serialize<Json::Value>();
    }

    Response to_http_response(const ConfigFileError& e) {
        return ResponseFactory::internal_error("Config file error: " + e.path + ": " + e.reason);
    }

    Response to_http_response(const ConfigParseError& e) {
        return ResponseFactory::internal_error("Config parse error: " + e.path + " line " + std::to_string(e.line) +
                                               ": " + e.detail);
    }

    Response to_http_response(const ConfigSchemaError& e) {
        std::string body = "Config schema error: " + e.path;
        if (!e.field_path.empty()) {
            body += " field ";
            body += e.field_path;
        }
        body += ": ";
        body += e.detail;
        return ResponseFactory::internal_error(std::move(body));
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Write the CMake leaf + wire the aggregate + retire the staging comment**

Create `components/http/config/load_server_config/CMakeLists.txt`:

```cmake
##############################################################################
# Http Config — load_server_config / dump_server_config + config error types
##############################################################################
add_library(${DMP_HTTP}.Config.LoadServerConfig STATIC
        load_server_config.cpp
        load_server_config.hpp
)

target_include_directories(${DMP_HTTP}.Config.LoadServerConfig PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Config.LoadServerConfig
        PUBLIC
        ${DMP_HTTP}.Config.ServerConfig
        ${DMP_HTTP}.Types
        Demiplane::Common::Gears
        Demiplane::Common::Serialization
        JsonCpp::JsonCpp
)
##############################################################################
```

Replace the entire contents of `components/http/config/CMakeLists.txt` with (final shape — subdirs, aggregate, and
the retired PR 4 staging comment):

```cmake
##############################################################################
# Http Config — configuration value types + JSON loading (spec §10, PR 6).
# Per-leaf convention; the dotted ${DMP_HTTP}.Config target is an INTERFACE
# aggregate.
##############################################################################
add_subdirectory(tls_config)
add_subdirectory(server_config)
add_subdirectory(timeouts)
add_subdirectory(listener_config)
add_subdirectory(load_server_config)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Config INTERFACE)

target_link_libraries(${DMP_HTTP}.Config INTERFACE
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Config.ServerConfig
        ${DMP_HTTP}.Config.Timeouts
        ${DMP_HTTP}.Config.ListenerConfig
        ${DMP_HTTP}.Config.LoadServerConfig
)
##############################################################################
```

- [ ] **Step 6: Build + run**

Run: `cmake --preset debug 2>&1 | tail -3 && cmake --build build/debug --target Demiplane.Tests.Unit.Http.Config -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Demiplane.Tests.Unit.Http.Config"`
Expected: build clean; 29 tests PASS (19 prior + 10 load/dump).

- [ ] **Step 7: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add components/http/config/load_server_config components/http/config/CMakeLists.txt tests/unit_tests/http/config/test_load_server_config.cpp tests/unit_tests/http/CMakeLists.txt
# suggested message: "http/config: load_server_config/dump_server_config + typed config errors (Outcome, to_http_response)"
```

---

### Task 8: `attach_default_listeners` (server layer)

Lives in the **server layer** (it needs `Server`; the config layer stays below it — one-way deps). Also creates the
`Demiplane.Tests.Unit.Http.Server` unit target (Server lifecycle stays integration-tested; this helper is pure
build-phase logic and unit-testable without sockets).

**Files:**

- Create: `components/http/server/attach_default_listeners/attach_default_listeners.hpp`
- Create: `components/http/server/attach_default_listeners/attach_default_listeners.cpp`
- Create: `components/http/server/attach_default_listeners/CMakeLists.txt`
- Modify: `components/http/server/CMakeLists.txt`
- Create: `tests/unit_tests/http/server/test_attach_default_listeners.cpp`
- Modify: `tests/unit_tests/http/CMakeLists.txt`

**Interfaces:**

- Consumes: `Server::config()`, `add_tcp_listener`/`add_tls_listener`/`add_quic_listener` (Tasks 5–6 shapes),
  `ListenerConfig::effective_protocols()`/`transport()`/`tls()`/`bind_address()`/`port()`,
  `Http11Driver{Http11Config}`, default-constructed `Http2Driver`/`Http3Driver` scaffolds.
- Produces: `void demiplane::http::attach_default_listeners(Server& server)` — walks
  `server.config().listeners()`, v1 combos per D5, throws `std::invalid_argument` on unsupported ones, no-op on an
  empty list. Call during the build phase (before `setup()`). Consumed by Task 9 and applications.

- [ ] **Step 1: Write the failing test**

Create `tests/unit_tests/http/server/test_attach_default_listeners.cpp`:

```cpp
#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/asio/io_context.hpp>

#include <attach_default_listeners.hpp>
#include <http_enums.hpp>
#include <listener_config.hpp>
#include <server.hpp>
#include <server_config.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;

namespace {

    TlsConfig dummy_tls() {
        // Paths need not exist: bind() (which builds the SSL ctx) never runs here.
        return TlsConfig::Builder{}.cert_file("c.pem").key_file("k.pem").finalize();
    }

    ServerConfig cfg_with(std::vector<ListenerConfig> listeners) {
        return ServerConfig::Builder{}.listeners(std::move(listeners)).finalize();
    }

    ListenerConfig tls_listener(std::vector<Protocol> protocols) {
        return ListenerConfig::Builder{}
            .bind_address("127.0.0.1")
            .port(0)
            .transport(ListenerConfig::Transport::tls)
            .protocols(std::move(protocols))
            .tls(dummy_tls())
            .finalize();
    }

}  // namespace

TEST(AttachDefaultListenersTest, EmptyConfigIsANoOp) {
    boost::asio::io_context ioc;
    Server server{ServerConfig::Builder{}.finalize(), ioc.get_executor()};
    attach_default_listeners(server);
    EXPECT_TRUE(server.listeners().empty());
}

TEST(AttachDefaultListenersTest, AttachesTcpH1WithEmptyProtocolsDefault) {
    boost::asio::io_context ioc;
    // No protocols listed — effective_protocols() defaults to [http1].
    Server server{
        cfg_with({ListenerConfig::Builder{}.bind_address("127.0.0.1").port(0).finalize()}), ioc.get_executor()};
    attach_default_listeners(server);
    ASSERT_EQ(server.listeners().size(), 1u);
    EXPECT_EQ(server.listeners().front()->bind_address(), "127.0.0.1");
}

TEST(AttachDefaultListenersTest, AttachesEveryTlsCombo) {
    boost::asio::io_context ioc;
    Server server{cfg_with({tls_listener({Protocol::http1}),
                            tls_listener({Protocol::http2}),
                            tls_listener({Protocol::http1, Protocol::http2}),
                            tls_listener({Protocol::http2, Protocol::http1})}),
                  ioc.get_executor()};
    attach_default_listeners(server);
    EXPECT_EQ(server.listeners().size(), 4u);
}

TEST(AttachDefaultListenersTest, AttachesQuicH3Scaffold) {
    boost::asio::io_context ioc;
    Server server{cfg_with({ListenerConfig::Builder{}
                                .bind_address("127.0.0.1")
                                .port(0)
                                .transport(ListenerConfig::Transport::quic)
                                .tls(dummy_tls())
                                .finalize()}),
                  ioc.get_executor()};
    attach_default_listeners(server);
    EXPECT_EQ(server.listeners().size(), 1u);
}

TEST(AttachDefaultListenersTest, RejectsH2cOverTcp) {
    // tcp+[http2] passes ListenerConfig::validate() (h2c is a protocol fact)
    // but v1 has no cleartext-h2 path — attach throws (D5).
    boost::asio::io_context ioc;
    Server server{
        cfg_with({ListenerConfig::Builder{}.bind_address("127.0.0.1").port(0).protocols({Protocol::http2}).finalize()}),
        ioc.get_executor()};
    EXPECT_THROW(attach_default_listeners(server), std::invalid_argument);
}
```

Append the unit target to `tests/unit_tests/http/CMakeLists.txt`:

```cmake
##############################################################################
# Test HTTP Server layer (unit — attach_default_listeners; the Server
# lifecycle itself is integration-tested)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Server
        server/test_attach_default_listeners.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Server
        PRIVATE
        Demiplane.Component.HTTP.Server
        Demiplane.Component.HTTP.Config
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Types
        Boost::asio
        ${TEST_LIBS}
)
##############################################################################
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --preset debug 2>&1 | tail -3 && cmake --build build/debug --target Demiplane.Tests.Unit.Http.Server -- -j4 2>&1 | tail -5`
Expected: FAIL — `attach_default_listeners.hpp: No such file or directory`.

- [ ] **Step 3: Write the header**

Create `components/http/server/attach_default_listeners/attach_default_listeners.hpp`:

```cpp
#pragma once

namespace demiplane::http {

    class Server;

    /**
     * @brief Walk server.config().listeners() and add the matching listener +
     *        driver instances (spec §10.3) — the config-driven wiring path.
     *
     * Driver config derives from the server-level config: Http11Config gets
     * body_limit + the three phase timeouts (max_header_bytes keeps its 16 KB
     * struct default — no ServerConfig field maps to it in v1).
     *
     * v1-supported (transport, protocols) combinations:
     *   tcp  + [http1]                     → TcpListener<Http11Driver>
     *   tls  + [http1]                     → TlsListener<Http11Driver>
     *   tls  + [http2]                     → TlsListener<Http2Driver>   (scaffold driver)
     *   tls  + [http1, http2] (either order) → TlsListener<…> in the LISTED
     *                                        order — JSON order is the ALPN
     *                                        server-preference order
     *   quic + [http3]                     → QuicListener<Http3Driver>  (scaffold)
     * An empty protocols list defaults per transport (http1; http3 on quic).
     *
     * Call during the build phase (before setup()). An empty listeners array
     * is a no-op — programmatic add_*_listener calls compose with this.
     *
     * @throws std::invalid_argument on a combination v1 cannot serve
     *         (e.g. tcp+[http2] — h2c is not supported).
     */
    void attach_default_listeners(Server& server);

}  // namespace demiplane::http
```

- [ ] **Step 4: Write the implementation**

Create `components/http/server/attach_default_listeners/attach_default_listeners.cpp`:

```cpp
#include "attach_default_listeners.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <http2_driver.hpp>
#include <http3_driver.hpp>
#include <http_enums.hpp>
#include <listener_config.hpp>
#include <server.hpp>
#include <server_config.hpp>

namespace demiplane::http {

    namespace {

        Http11Config make_http11_config(const ServerConfig& cfg) noexcept {
            Http11Config h11{};  // max_header_bytes keeps its 16 KB default
            h11.max_body_bytes = cfg.body_limit();
            h11.header_timeout = cfg.timeouts().header();
            h11.body_timeout   = cfg.timeouts().body();
            h11.idle_timeout   = cfg.timeouts().idle();
            return h11;
        }

        [[noreturn]] void throw_unsupported(const ListenerConfig& listener, const std::vector<Protocol>& protocols) {
            std::string msg = "attach_default_listeners: unsupported (transport, protocols) combination: ";
            msg += to_string(listener.transport());
            msg += " + [";
            for (std::size_t i = 0; i < protocols.size(); ++i) {
                if (i != 0) {
                    msg += ", ";
                }
                msg += to_string(protocols[i]);
            }
            msg += "]";
            throw std::invalid_argument{msg};
        }

        void attach_one(Server& server, const ListenerConfig& listener) {
            const ServerConfig& cfg = server.config();
            const auto protocols    = listener.effective_protocols();
            const std::vector h1    = {Protocol::http1};
            const std::vector h2    = {Protocol::http2};
            const std::vector h1_h2 = {Protocol::http1, Protocol::http2};
            const std::vector h2_h1 = {Protocol::http2, Protocol::http1};
            const std::vector h3    = {Protocol::http3};

            switch (listener.transport()) {
                case ListenerConfig::Transport::tcp:
                    if (protocols == h1) {
                        server.add_tcp_listener(
                            listener.bind_address(), listener.port(), Http11Driver{make_http11_config(cfg)});
                        return;
                    }
                    break;
                case ListenerConfig::Transport::tls:
                    // ListenerConfig::validate() guarantees tls() is engaged here.
                    if (protocols == h1) {
                        server.add_tls_listener(listener.bind_address(), listener.port(), *listener.tls(),
                                                Http11Driver{make_http11_config(cfg)});
                        return;
                    }
                    if (protocols == h2) {
                        server.add_tls_listener(
                            listener.bind_address(), listener.port(), *listener.tls(), Http2Driver{});
                        return;
                    }
                    if (protocols == h1_h2) {
                        server.add_tls_listener(listener.bind_address(), listener.port(), *listener.tls(),
                                                Http11Driver{make_http11_config(cfg)}, Http2Driver{});
                        return;
                    }
                    if (protocols == h2_h1) {
                        server.add_tls_listener(listener.bind_address(), listener.port(), *listener.tls(),
                                                Http2Driver{}, Http11Driver{make_http11_config(cfg)});
                        return;
                    }
                    break;
                case ListenerConfig::Transport::quic:
                    if (protocols == h3) {
                        server.add_quic_listener(
                            listener.bind_address(), listener.port(), *listener.tls(), Http3Driver{});
                        return;
                    }
                    break;
            }
            throw_unsupported(listener, protocols);
        }

    }  // namespace

    void attach_default_listeners(Server& server) {
        for (const ListenerConfig& listener : server.config().listeners()) {
            attach_one(server, listener);
        }
    }

}  // namespace demiplane::http
```

- [ ] **Step 5: Write the CMake leaf + wire the server aggregate**

Create `components/http/server/attach_default_listeners/CMakeLists.txt`:

```cmake
##############################################################################
# Http Server — attach_default_listeners (config-driven listener wiring)
##############################################################################
add_library(${DMP_HTTP}.Server.AttachDefaultListeners STATIC
        attach_default_listeners.cpp
        attach_default_listeners.hpp
)

target_include_directories(${DMP_HTTP}.Server.AttachDefaultListeners PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Server.AttachDefaultListeners
        PUBLIC
        ${DMP_HTTP}.Server.Core
        ${DMP_HTTP}.Config
        ${DMP_HTTP}.Drivers
)
##############################################################################
```

In `components/http/server/CMakeLists.txt`, add the subdirectory and the aggregate entry:

```cmake
add_subdirectory(server_observer)
add_subdirectory(server)
add_subdirectory(run_standalone)
add_subdirectory(attach_default_listeners)
```

and

```cmake
target_link_libraries(${DMP_HTTP}.Server INTERFACE
        ${DMP_HTTP}.Server.Observer
        ${DMP_HTTP}.Server.Core
        ${DMP_HTTP}.Server.RunStandalone
        ${DMP_HTTP}.Server.AttachDefaultListeners
)
```

- [ ] **Step 6: Build + run**

Run: `cmake --preset debug 2>&1 | tail -3 && cmake --build build/debug --target Demiplane.Tests.Unit.Http.Server -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Demiplane.Tests.Unit.Http.Server"`
Expected: build clean; 5 tests PASS.

- [ ] **Step 7: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add components/http/server/attach_default_listeners components/http/server/CMakeLists.txt tests/unit_tests/http/server tests/unit_tests/http/CMakeLists.txt
# suggested message: "http/server: attach_default_listeners — config-driven listener wiring (v1 combos)"
```

---

### Task 9: JSON→wire integration test (§12.2 PR 6 acceptance) + `TlsClient` extraction

Proves "a JSON-loaded server is a one-liner": file → `load_server_config` → `attach_default_listeners` → real
requests. Also proves `body_limit` reached the driver (413 on the wire) and the TLS path (cert files from JSON →
ALPN h1). The file-local `TlsClient` moves to a shared header first.

**Files:**

- Create: `tests/integration_tests/http/tls_client.hpp`
- Modify: `tests/integration_tests/http/test_http_tls.cpp`
- Create: `tests/integration_tests/http/test_http_config_wiring.cpp`
- Modify: `tests/integration_tests/http/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 7's `load_server_config`, Task 8's `attach_default_listeners`, `ServerIntegrationFixture` +
  `TcpClient` (landed fact 12), `http_tls_test::write_temp` + test cert PEMs (`test_tls_cert.hpp`).
- Produces: `http_it::TlsClient` in `tls_client.hpp` (moved verbatim; same API: `connect_handshake(port, alpns)`,
  `negotiated_alpn()`, `get(target)`).

- [ ] **Step 1: Extract `TlsClient` to a shared header**

Create `tests/integration_tests/http/tls_client.hpp` (class body moved **verbatim** from `test_http_tls.cpp`,
including its TODO comment):

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <openssl/ssl.h>

#include "http_test_fixture.hpp"

namespace http_it {

    // Synchronous Beast TLS client (moved verbatim from test_http_tls.cpp in
    // PR 6 so config-wiring tests reuse it). connect_handshake() sets the ALPN
    // offer and returns the handshake error_code (empty = success).
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
            // TODO: check SSL_set_alpn_protos return (0 == success); the stream_.handshake(...) tidy warning is benign (ec is used) — silence with std::ignore/NOLINT if desired.
            ::SSL_set_alpn_protos(stream_.native_handle(),
                                  reinterpret_cast<const unsigned char*>(wire.data()),
                                  static_cast<unsigned int>(wire.size()));
            stream_.next_layer().connect({asio::ip::make_address("127.0.0.1"), port});
            boost::beast::error_code ec;
            stream_.handshake(asio::ssl::stream_base::client, ec);
            return ec;
        }

        [[nodiscard]] std::string negotiated_alpn() {
            const unsigned char* proto = nullptr;
            unsigned int len           = 0;
            ::SSL_get0_alpn_selected(stream_.native_handle(), &proto, &len);
            return std::string{reinterpret_cast<const char*>(proto), len};
        }

        ParsedResponse get(const std::string& target) {
            bhttp::request<bhttp::string_body> req{bhttp::verb::get, target, 11};
            req.set(bhttp::field::host, "127.0.0.1");
            req.keep_alive(false);
            req.prepare_payload();
            bhttp::write(stream_, req);
            ParsedResponse res;
            boost::beast::flat_buffer buf;
            bhttp::read(stream_, buf, res);
            return res;
        }

    private:
        asio::io_context ioc_;
        asio::ssl::context ctx_{asio::ssl::context::tls_client};
        asio::ssl::stream<asio::ip::tcp::socket> stream_;
    };

}  // namespace http_it
```

(Adjust only two things while moving: the class now sits in namespace `http_it`, so the `http_it::ParsedResponse`
qualification inside `get()` drops to `ParsedResponse`, and the `asio`/`bhttp` aliases come from
`http_test_fixture.hpp`'s in-namespace aliases.)

In `tests/integration_tests/http/test_http_tls.cpp`: delete the entire file-local `TlsClient` class (the block from
the `// Synchronous Beast TLS client...` comment through its closing `};`), add the include

```cpp
#include "tls_client.hpp"
```

next to the existing `#include "test_tls_cert.hpp"`, and add inside the anonymous namespace (where the class used to
be):

```cpp
    using http_it::TlsClient;
```

- [ ] **Step 2: Verify the refactor is behavior-neutral**

Run: `cmake --build build/debug --target Demiplane.Tests.Integration.Http.Tls -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Demiplane.Tests.Integration.Http.Tls"`
Expected: build clean; the TLS battery passes unchanged.

- [ ] **Step 3: Write the wiring test**

Create `tests/integration_tests/http/test_http_config_wiring.cpp`:

```cpp
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <attach_default_listeners.hpp>
#include <controller.hpp>
#include <load_server_config.hpp>
#include <request_context.hpp>
#include <server.hpp>

#include "server_test_fixture.hpp"
#include "test_tls_cert.hpp"
#include "tls_client.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;

namespace {

    class HelloController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &HelloController::hello);
        }

    private:
        AsyncResponse hello(RequestContext ctx) {  // NOLINT(readability-convert-member-functions-to-static)
            co_return ctx.ok("hello config");
        }
    };

    class ConfigWiringTest : public http_it::ServerIntegrationFixture {};

}  // namespace

// §12.2 PR 6 acceptance: a JSON file becomes a serving listener in one
// load + one attach. body_limit=64 must reach the driver (413 pre-routing).
TEST_F(ConfigWiringTest, TcpListenerFromJsonServesAndEnforcesBodyLimit) {
    const auto path = http_tls_test::write_temp("wiring_tcp.json", R"({
        "threads": 1,
        "body_limit": 64,
        "request_arena_size": 8192,
        "drain_timeout_ms": 2000,
        "timeouts": { "header_ms": 5000, "body_ms": 5000, "idle_ms": 5000 },
        "listeners": [
            { "bind": "127.0.0.1", "port": 0, "transport": "tcp", "protocols": ["http1"] }
        ]
    })");
    auto loaded = load_server_config(path);
    ASSERT_TRUE(loaded.is_success());

    start_server(
        [](Server& s) {
            s.add_controller(std::make_shared<HelloController>());
            attach_default_listeners(s);
        },
        std::move(loaded).value());

    http_it::TcpClient ok_client{port()};
    const auto ok_res = ok_client.send(bhttp::verb::get, "/hello");
    EXPECT_EQ(ok_res.result_int(), 200u);
    EXPECT_EQ(ok_res.body(), "hello config");

    // Oversize Content-Length → the driver's eager 413, before routing runs.
    http_it::TcpClient big_client{port()};
    const auto big = big_client.send(bhttp::verb::post, "/hello", std::string(100, 'x'));
    EXPECT_EQ(big.result_int(), 413u);
}

// TLS material from JSON: cert/key paths land in build_ssl_context and ALPN
// negotiates http/1.1 on the wire.
TEST_F(ConfigWiringTest, TlsListenerFromJsonNegotiatesH1) {
    const std::string cert = http_tls_test::write_temp("wiring_cert.pem", http_tls_test::kTestCertPem);
    const std::string key  = http_tls_test::write_temp("wiring_key.pem", http_tls_test::kTestKeyPem);
    const std::string json = R"({
        "listeners": [
            { "bind": "127.0.0.1", "port": 0, "transport": "tls", "protocols": ["http1"],
              "tls": { "cert_file": ")" +
                             cert + R"(", "key_file": ")" + key + R"(" } }
        ]
    })";
    const auto path = http_tls_test::write_temp("wiring_tls.json", json);
    auto loaded     = load_server_config(path);
    ASSERT_TRUE(loaded.is_success());

    start_server(
        [](Server& s) {
            s.add_controller(std::make_shared<HelloController>());
            attach_default_listeners(s);
        },
        std::move(loaded).value());

    http_it::TlsClient client;
    const auto ec = client.connect_handshake(port(), {"http/1.1"});
    ASSERT_FALSE(ec) << ec.message();
    EXPECT_EQ(client.negotiated_alpn(), "http/1.1");
    const auto res = client.get("/hello");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "hello config");
}
```

- [ ] **Step 4: Register the source**

In `tests/integration_tests/http/CMakeLists.txt`, replace the existing `${INTEGRATION_TESTING_TARGET}.Http.Server`
block's opening (only the source list changes — `Demiplane.Component.HTTP.Server` already aggregates the new
`attach_default_listeners` leaf, and `.Config` + OpenSSL for the TLS client are already linked):

```cmake
add_integration_test(${INTEGRATION_TESTING_TARGET}.Http.Server
        test_http_server_lifecycle.cpp
        test_http_server_observer.cpp
        test_http_run_standalone.cpp
        test_http_server_concurrency.cpp
        test_http_config_wiring.cpp
        LINK_LIBS
        Demiplane.Component.HTTP.Server
        Demiplane.Component.HTTP.Listeners
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Types
        Demiplane.Component.HTTP.Config
        Demiplane::Common::Chrono
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
        LABELS "http"
)
```

- [ ] **Step 5: Build + run**

Run: `cmake --build build/debug --target Demiplane.Tests.Integration.Http.Server -- -j4 && ctest --test-dir build/debug --output-on-failure -R "Demiplane.Tests.Integration.Http.Server"`
Expected: build clean; the full Server integration battery + both wiring tests PASS.

- [ ] **Step 6: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add tests/integration_tests/http/tls_client.hpp tests/integration_tests/http/test_http_tls.cpp tests/integration_tests/http/test_http_config_wiring.cpp tests/integration_tests/http/CMakeLists.txt
# suggested message: "http/tests: JSON->wire config wiring battery (tcp 200/413, tls ALPN h1); extract shared TlsClient"
```

---

### Task 10: Spec sync + stale-comment sweep

**Files:**

- Modify: `docs/superpowers/specs/2026-05-07-http-redesign-design.md`
- Modify: `components/http/drivers/http11/http11_config.hpp`
- Modify: `components/http/routing/route_registry/route_registry.hpp`
- Modify: `components/http/connection/request_arena/request_arena.hpp`
- Modify: `components/http/CMakeLists.txt`

**Interfaces:** documentation only; no code behavior changes.

- [ ] **Step 1: Update the spec header + §10 + §16**

In `docs/superpowers/specs/2026-05-07-http-redesign-design.md`:

1. **Status line** (line 5-7): append the PR 6 plan reference:
   `PR 6 plan: docs/superpowers/plans/2026-07-07-http-config-layer.md (landed)`.
2. **§10.1**: add a short "Landed shape (PR 6)" note before the code sketches recording the deltas: `port` is
   `std::uint16_t`; `ServerConfig` is Builder-only while `TlsConfig` keeps a no-validation full-ctor escape hatch and
   `Timeouts` keeps its public 3-arg ctor (D6); `key_passphrase` is `FieldPolicy::Secret` (D3); every enum field is
   string-encoded via ADL codecs next to the enum, unknown strings throw (D2); `ListenerConfig` gained
   `effective_protocols()` (empty ⇒ transport default); `ServerConfig::drain_timeout` keeps the JSON name
   `drain_timeout_ms`.
3. **§10.2**: record D4 (field_path best-effort; `detail` names the field) and D9 (regular-file check, non-object
   top-level → schema error, parse line best-effort, unknown keys ignored). Note the machinery repair (D1) with a
   pointer at `tests/unit_tests/serialization/test_config_interface.cpp`.
4. **§10.3**: fix the helper signature to `attach_default_listeners(Server&)` (no `DefaultDrivers` — D5), list the
   v1 combos + the JSON-order-is-ALPN-preference rule, and note that `ServerConfig::threads` is forwarded by the
   caller (`run_standalone(cfg, cfg.threads(), …)` stays the documented usage).
5. **§9.1** comment `the config-driven path is attach_default_listeners (PR 6)` → `(PR 6, landed)`.
6. **§15 Decisions Log**: append rows for D1–D9 (Decision / Choice / Why — one row each, matching the table style;
   include the cipher-list-failure-throws decision from Task 3 in the D-row for build hardening or as its own row).
7. **§16 Open Questions**: mark the `ConfigInterface` field-type coverage question **resolved** — machinery repaired
   + extended (D1), locked by `Demiplane.Tests.Unit.Serialization`; enum string-encoding landed (D2).

- [ ] **Step 2: Sweep the stale staging comments**

In `components/http/drivers/http11/http11_config.hpp`, replace

```cpp
    /// Per-driver HTTP/1.1 limits + phase timeouts (spec §6.3). ServerConfig
    /// (PR 6) constructs these from loaded config; for now callers build them
    /// directly.
```

with

```cpp
    /// Per-driver HTTP/1.1 limits + phase timeouts (spec §6.3).
    /// attach_default_listeners derives one from ServerConfig (body_limit +
    /// phase timeouts); direct construction remains for per-driver tuning.
```

In `components/http/routing/route_registry/route_registry.hpp` (line 24), replace

```cpp
    /// in PR 2; ServerConfig (PR 6) maps its config enum onto it.
```

with

```cpp
    /// in PR 2; the Server maps ServerConfig::path_normalization() onto it.
```

In `components/http/connection/request_arena/request_arena.hpp` (line 14), replace

```cpp
     * Allocated once at `size` bytes (ServerConfig::request_arena_size, default
```

with

```cpp
     * Allocated once at `size` bytes (ServerConfig::request_arena_size(), default
```

In `components/http/CMakeLists.txt`, replace the config-section banner

```cmake
# Http Config layer (PR 4 of redesign — plain TlsConfig; PR 6 grows it)
```

with

```cmake
# Http Config layer (PR 6 of redesign)
```

- [ ] **Step 3: Verify no staging markers remain in code**

Run: `grep -rn "TODO(PR6)\|PR 6\b" components tests --include="*.hpp" --include="*.cpp" --include="CMakeLists.txt"`
Expected: no output (docs/ keeps its historical references; code carries none).

- [ ] **Step 4: Full-tree verification**

Run: `cmake --build build/debug -- -j4 && ctest --test-dir build/debug --output-on-failure`
Expected: whole tree builds; full suite green.

- [ ] **Step 5: Suggested commit grouping (user-managed git — do not commit yourself)**

```bash
git add docs/superpowers/specs/2026-05-07-http-redesign-design.md components/http/drivers/http11/http11_config.hpp components/http/routing/route_registry/route_registry.hpp components/http/connection/request_arena/request_arena.hpp components/http/CMakeLists.txt
# suggested message: "http: PR6 spec sync (config layer landed, D1-D9) + stale staging-comment sweep"
```

---

## Final acceptance sweep (after all tasks)

1. `cmake --build build/debug -- -j4` — zero warnings introduced in touched files.
2. `ctest --test-dir build/debug --output-on-failure` — full suite green (unit: Serialization + Http.Config +
   Http.Server + Http.Listeners + the pre-existing batteries; integration: Http.Tcp/Tls/Server incl. the new wiring
   tests).
3. ASan: `cmake --preset asan 2>&1 | tail -3 && cmake --build build/asan -- -j4 && ctest --test-dir build/asan --output-on-failure -R "Serialization|Http"`
   — clean (the asan preset builds components as-is).
4. TSan (optional, machinery only — the tsan preset is common-only): `ctest --test-dir build/tsan --output-on-failure -R "Serialization"`
   after `cmake --preset tsan && cmake --build build/tsan -- -j4`. Note: any TSan run touching HTTP components hits
   the 3 known pre-existing Nexus ctor/dtor races — not a PR 6 regression.
5. Greps come back empty: `TODO(PR6)`, `ServerConfig{}`, plain-field TlsConfig access (Task 3 Step 8 pattern).
6. The §12.2 PR 6 acceptance sentence holds: loading a JSON file and serving it is
   `load_server_config` + `attach_default_listeners` + `setup()` — demonstrated by
   `test_http_config_wiring.cpp`.

## Coverage traceability (spec §14.1 `config_load_test` + §12.2 PR 6 → tests)

| Spec requirement                                                | Covered by                                                                                                          |
|-----------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------|
| §14.1 valid JSON loads                                          | `LoadServerConfigTest.LoadsValidConfig`                                                                             |
| §14.1 missing required field surfaces field path                | `LoadServerConfigTest.ValidateFailureIsSchemaErrorNamingField` (detail names the field; field_path best-effort, D4) |
| §14.1 type mismatch surfaces field path                         | `LoadServerConfigTest.TypeMismatchIsSchemaError` + `ConfigInterfaceTest.TypeMismatchThrowsJsonException`            |
| §14.1 round-trip via dump_server_config produces equivalent cfg | `LoadServerConfigTest.DumpLoadRoundTripIsAFixedPoint` + `ServerConfigTest.RoundTripPreservesNestedListeners`        |
| §14.1 enum string mappings round-trip                           | `TlsConfigTest.BuilderRoundTripsThroughJson`, `ListenerConfigTest.RoundTripPreservesProtocolOrder`, `ServerConfigTest.*`, unknown-string throw tests (D2) |
| §10.1 config types (Timeouts/Tls/Listener/Server + Builders)    | Tasks 2–5 unit batteries (29 tests)                                                                                 |
| §10.2 typed load errors (file/parse/schema) + to_http_response  | `LoadServerConfigTest.{MissingFile,Directory,MalformedJson,NonObjectTopLevel,ErrorsRenderAs500}`                    |
| §10.3 attach_default_listeners walks cfg.listeners()            | `AttachDefaultListenersTest.*` (5 tests) + both `ConfigWiringTest` wire tests                                       |
| §12.2 PR 6 "JSON-loaded server is then a one-liner"             | `ConfigWiringTest.TcpListenerFromJsonServesAndEnforcesBodyLimit`, `ConfigWiringTest.TlsListenerFromJsonNegotiatesH1` |
| §16 ConfigInterface field-type coverage (open question)         | Task 1 (D1): machinery repair + `Demiplane.Tests.Unit.Serialization` (9 tests)                                      |
| Staged PR 6 markers (arena wiring, cipher TODO, comments)       | Task 6 (TlsListener arena), Task 3 Step 5 (cipher-list throw), Task 10 (comment sweep + spec sync)                  |
| ServerConfig consumers keep working (lifecycle/drain/standalone)| Task 5 Step 7 — full `Demiplane.Tests.Integration.Http.Server` battery re-run                                       |
