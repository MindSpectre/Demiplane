# Demiplane C++ Code Style Guide

This document covers styling decisions beyond `.clang-format` configuration.

## Attributes

### `[[nodiscard]]`

Apply to:

- All const getters and query methods (`is_*`, `has_*`, `supports_*`)
- Factory methods and builders
- Methods returning `Outcome<T, E>` or result types
- Any function where ignoring the return value is likely a bug

```cpp
[[nodiscard]] constexpr bool is_success() const noexcept;
[[nodiscard]] static std::shared_ptr<Table> make_ptr(std::string name);
[[nodiscard]] gears::Outcome<ResultBlock, ErrorCode> execute(std::string_view query);
```

### `[[maybe_unused]]`

Apply to:

- Caught exceptions when only type matters
- Lambda parameters documenting intent but unused
- Values read for side effects (benchmarks, volatile access)

```cpp
} catch ([[maybe_unused]] std::out_of_range& e) {
    log_error("Index out of range");
}

server.on_request([]([[maybe_unused]] const Request& req) {
    handle_default();
});
```

## Naming Conventions

| Element              | Convention                | Example                       |
|----------------------|---------------------------|-------------------------------|
| Classes/Structs      | PascalCase                | `QueryCompiler`, `ErrorCode`  |
| Functions/Methods    | snake_case                | `is_success()`, `to_string()` |
| Variables            | snake_case                | `table_name`, `result_count`  |
| Member variables     | snake_case + trailing `_` | `table_name_`, `distinct_`    |
| Template type params | PascalCase                | `T`, `ValueType`, `ColumnsTp` |
| Namespaces           | lowercase                 | `demiplane::db::postgres`     |
| Enum values          | PascalCase                | `LogLevel::Warning`           |
| Macros               | SCREAMING_SNAKE           | `DMP_ENABLE_LOGGING`          |
| Files                | snake_case                | `query_compiler.hpp`          |

### Getters

Prefer member-name style without `get_` prefix for simple accessors:

```cpp
constexpr const std::string& table_name() const noexcept;  // Good
constexpr const std::string& get_table_name() const noexcept;  // Avoid
```

Use `get_` for complex lookups or operations:

```cpp
[[nodiscard]] const FieldSchema* get_field_schema(std::string_view name) const;
```

Also use `get_` for atomic, volatile, or memory-barrier reads — the access isn't a plain
field load and the prefix signals that to the caller:

```cpp
[[nodiscard]] int64_t get_cursor() const noexcept;     // atomic load
[[nodiscard]] int64_t get_volatile() const noexcept;   // volatile read
```

### Template Parameters

```cpp
template <typename T>                    // Simple generic
template <typename ValueType>            // Descriptive when needed
template <typename ColumnsTp>            // Tp suffix for forwarding refs
template <typename Self>                 // Self for deducing this
template <typename... Args>              // Pack names
template <IsFieldDef FieldT>             // Constrained parameters
```

## Initialization

### Class Member Defaults

Use `=` syntax for default member initialization:

```cpp
class Query {
private:
    bool distinct_  = false;
    int limit_      = -1;
    std::string alias_ = "";
};
```

### Local Variables and Object Construction

Prefer `{}` over `()` for construction outside class definitions:

```cpp
void process() {
    User user{1, "name", true};           // Aggregate/constructor
    auto table = Table{"users"};          // Single argument
    std::vector<int> ids = {1, 2, 3};     // Container initialization

    // Not:
    // User user(1, "name", true);
    // auto table = Table("users");
}
```

### Constructor Initializer Lists

Use `{}` for member initialization:

```cpp
class Connection {
public:
    Connection(std::string host, int port)
        : host_{std::move(host)}
        , port_{port}
        , connected_{false} {
    }

private:
    std::string host_;
    int port_;
    bool connected_;
};
```

## Builder Pattern

When a type has more than a handful of configuration fields, expose construction through a
nested `Builder` class. The Builder keeps the configured value-type simple and lets callers
set only the fields they care about.

**Contract:**

1. **Setters use deducing `this`**, named after the field (no `with_` or `set_` prefix):

```cpp
template <typename Self>
constexpr auto&& ring_buffer_size(this Self&& self, std::size_t value) noexcept {
    self.config_.ring_buffer_size_ = value;
    return std::forward<Self>(self);
}
```

2. **Terminal method is `finalize() &&`.** It runs validation and moves the built object
   out. The rvalue-ref qualifier prevents reuse of a spent Builder:

```cpp
[[nodiscard]] Config finalize() && {
    config_.validate();
    return std::move(config_);
}
```

3. **Constructors accept existing configs** for edit-one-field workflows:

```cpp
Builder() = default;
explicit Builder(const Config& existing) : config_{existing} {}
explicit Builder(Config&& existing)      : config_{std::move(existing)} {}
```

## Constexpr and Consteval

### Constexpr

Use `constexpr` for anything that can possibly be evaluated at compile time, including features coming in C++26. Only
omit when runtime-only evaluation is explicitly intended:

```cpp
// Use constexpr liberally
constexpr bool is_valid() const noexcept;
constexpr auto transform(auto&& value);
static constexpr std::string_view name = "table";

// Omit only when runtime-only is intended
std::string generate_uuid();  // Inherently runtime
void write_to_file(std::string_view path);  // Side effects
```

### Consteval

Use `consteval` only for functions that must execute at compile time:

```cpp
consteval std::size_t hash_type_name(std::string_view name) {
    return compute_hash(name);
}

consteval auto make_field_index(auto... fields) {
    return std::array{fields...};
}
```

## Explicit Specifier

### Single-Argument Constructors

Always use `explicit` for single-argument constructors:

```cpp
class TableColumn {
public:
    explicit TableColumn(std::string name);
};
```

When implicit conversion is intentionally desired, use `explicit(false)` to indicate awareness:

```cpp
class StringView {
public:
    explicit(false) StringView(const char* str);  // Intentional implicit conversion
};
```

### Conditional Explicit

Use for template constructors:

```cpp
template <typename U>
constexpr explicit(!std::is_convertible_v<U, T>) Outcome(U&& value);
```

## Ref-Qualifiers

Use ref-qualifiers to prevent dangling references and optimize for value category:

```cpp
class Builder {
public:
    // Lvalue: return reference, caller keeps builder
    [[nodiscard]] const Config& config() const& noexcept {
        return config_;
    }

    // Rvalue: return by value, builder is being discarded
    [[nodiscard]] Config config() && noexcept {
        return std::move(config_);
    }

    // Method chaining with proper forwarding
    template <typename Self>
    constexpr auto&& with_name(this Self&& self, std::string name) {
        self.name_ = std::move(name);
        return std::forward<Self>(self);
    }

private:
    Config config_;
};
```

### Getters and Setters

Prefer `noexcept` for data access:

```cpp
[[nodiscard]] constexpr const std::string& name() const& noexcept;
[[nodiscard]] constexpr std::string name() && noexcept;
constexpr void set_name(std::string value) noexcept;
```

## Noexcept

Mark `noexcept` when function truly cannot throw:

```cpp
constexpr bool is_success() const noexcept;
constexpr uint16_t value() const noexcept;
```

Use conditional noexcept for templates:

```cpp
constexpr Outcome() noexcept(std::is_nothrow_constructible_v<T>)
    requires std::default_initializable<T>;
```

## Unreachable Code

### `std::unreachable()`

Use for code paths that should never execute:

```cpp
switch (category) {
    case Category::Success: return "success";
    case Category::Error: return "error";
}
std::unreachable();
```

### `GEARS_UNREACHABLE(Type, Message)`

Use in `if constexpr` chains to catch unhandled types at compile time. Expands to a
`static_assert` on `gears::dependent_false_v<Type>` followed by `std::unreachable()`, so the
assert only fires when the branch is actually instantiated:

```cpp
template <typename T>
constexpr auto process(T value) {
    if constexpr (std::integral<T>) {
        return value * 2;
    } else if constexpr (std::floating_point<T>) {
        return value * 2.0;
    } else {
        GEARS_UNREACHABLE(T, "process() expects integral or floating-point");
    }
}
```

## Static Member Enforcement

Use `gears::enforce_non_static` for members that could be static but intentionally aren't (e.g., for polymorphism or
future instance state):

```cpp
class Dialect {
public:
    std::string quote_identifier(std::string_view id) const {
        gears::enforce_non_static(this);
    }
};
```

## Error Handling

### Hot Path: `Outcome<T, ErrorCode>`

Use `Outcome` only in performance-critical, hot-path code:

```cpp
// Async executor - called thousands of times per second
[[nodiscard]] boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorCode>>
execute(std::string_view query);

// Tight loop processing
[[nodiscard]] gears::Outcome<Row, ParseError> parse_row(std::span<const std::byte> data);
```

### Setup/Rare/Cold Path: Exceptions

Use exceptions for setup functions, constructors, rare operations, and non-hot-path code:

```cpp
// Constructor - called once
Connection::Connection(const Config& config) {
    if (!config.is_valid()) {
        throw ConfigurationError{"Invalid connection config"};
    }
}

// Setup function - called at startup
void initialize_pool(const PoolConfig& config) {
    if (config.max_connections < 1) {
        throw std::invalid_argument{"Pool must have at least 1 connection"};
    }
}

// Rare operation
void migrate_schema(const Schema& target) {
    if (!can_migrate(target)) {
        throw MigrationError{"Incompatible schema versions"};
    }
}
```

## Platform-Specific Code

Gate platform-specific code with `#if defined(<PLATFORM>)`. Nest sub-feature checks (libc
version, kernel headers) inside the platform guard. Always provide a portable `#else`
fallback — callers should never need `#ifdef` at the use site.

```cpp
#if defined(__linux__)
    #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 30))
        #include <unistd.h>
    #else
        #include <sys/syscall.h>
    #endif
#endif

[[nodiscard]] inline uint64_t capture_kernel_tid() noexcept {
#if defined(__linux__)
    #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 30))
    return static_cast<uint64_t>(::gettid());
    #else
    return static_cast<uint64_t>(::syscall(SYS_gettid));
    #endif
#else
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif
}
```

## Templates and Concepts

### Prefer Concepts Over SFINAE

```cpp
// Good: concept constraint
template <typename T>
    requires std::invocable<T, int>
void apply(T&& fn);

// Avoid: SFINAE
template <typename T, std::enable_if_t<std::is_invocable_v<T, int>, int> = 0>
void apply(T&& fn);
```

### Concept Definitions

```cpp
template <typename T>
concept IsFieldDef = requires {
    typename T::value_type;
    { T::name() } -> std::convertible_to<std::string_view>;
};

template <typename T, typename... Args>
concept OneOf = (std::is_same_v<Args, T> || ...);
```

### Concept Naming

| Prefix    | Intent                                           | Example                                   |
|-----------|--------------------------------------------------|-------------------------------------------|
| `Is*`     | Type predicate — "T is a kind of X"              | `IsFieldDef`, `IsOperator`, `IsQuery`     |
| `Has*`    | Structural requirement — "T exposes members/ops" | `HasStaticNameMember`, `HasAcceptVisitor` |
| no prefix | Generic operator-like / variadic relation        | `OneOf`, `OneOfDecayed`                   |

### Requires Clause Placement

Place after template parameters, on own line with indent:

```cpp
template <typename F>
    requires std::invocable<F, T&>
constexpr auto and_then(F&& f) & -> std::invoke_result_t<F, T&>;
```

### Deducing This (C++23)

Prefer deducing this over CRTP.

For **member access** in getters and decompose methods, use `std::forward_like<Self>(self.member_)`:

```cpp
template <typename Self>
[[nodiscard]] constexpr auto&& value(this Self&& self) noexcept {
    return std::forward_like<Self>(self.value_);
}
```

For **method calls** (CRTP dispatch, chaining), use `std::forward<Self>(self).method()`:

```cpp
template <typename Self>
constexpr auto&& with_name(this Self&& self, std::string name) {
    self.name_ = std::move(name);
    return std::forward<Self>(self);
}

// CRTP dispatch to derived class
template <typename Self>
constexpr auto process(this Self&& self) {
    return std::forward<Self>(self).derived().do_work();
}
```

**Rationale:** `std::forward_like` directly applies the value category of `Self` to the member without going through the
object. This avoids clang-tidy `bugprone-use-after-move` false positives that occur with
`std::forward<Self>(self).member_`, and is semantically cleaner for member access.

### Out-of-line Template Implementations

For heavy template implementations, split the bodies into a `.inl` file placed in a
`detail/` (or `source/`) subdirectory next to the header. Include it at the bottom of the
matching `.hpp`, inside the same namespace context:

```cpp
// db_field.hpp
namespace demiplane::db {
    // ... class declarations ...
}  // namespace demiplane::db

#include "detail/db_field.inl"
```

## Lambda Captures

Prefer explicit captures over `[&]` or `[=]`:

```cpp
[this] { process(); }                            // Member access
[]() { return default_value(); }                 // Stateless
[&result](auto& val) { result = transform(val); } // Explicit ref
```

## Commenting

### File Headers

No copyright headers. Files start directly with `#pragma once`:

```cpp
#pragma once

#include <string>
```

### Documentation

Use Doxygen for public APIs:

```cpp
/**
 * @brief Async executor with coroutine support
 *
 * Thread safety: NOT thread-safe. Use strand for concurrent access.
 */
class AsyncExecutor {
```

### Inline Comments

Brief explanations for non-obvious logic:

```cpp
// Propagate error maintaining original category
std::visit([&result]<typename E>(E&& err) { ... }, value_);
```

### TODO Format

```cpp
// TODO: Issue#33 - add binary format flag
// TODO: add constexpr support
```

## Module Layout

Each top-level module under `common/` (and each component under `components/`) ships an
**umbrella header** at `<module>/export/demiplane/<module>` — a file with no extension
containing only `#pragma once` and `#include` directives for every public header of the
module:

```cpp
// common/gears/export/demiplane/gears
#pragma once

#include "gears_class_traits.hpp"
#include "gears_concepts.hpp"
#include "gears_exceptions.hpp"
#include "gears_hash.hpp"
// ...
```

Consumers include the whole module via `#include <demiplane/<module>>`:

```cpp
#include <demiplane/gears>
#include <demiplane/scroll>
```

## Namespaces

### Opening Style

Use the C++17 one-line nested form. Never nest with separate braces.

```cpp
namespace demiplane::scroll { ... }            // Good
namespace demiplane::db::postgres { ... }      // Good

// Avoid:
// namespace demiplane { namespace scroll { ... } }
```

### Internal Implementation Details

Place implementation-only types, trait structs, and helper concepts inside a nested
`namespace detail { ... }` — lowercase, singular (never `details` or `impl`). Anything
inside `detail` is not part of the public API.

```cpp
namespace demiplane::scroll {
    namespace detail {
        struct MetaSource { ... };
        struct ThreadLocalCache { ... };
    }  // namespace detail
}  // namespace demiplane::scroll
```

## Include Organization

Order (separated by blank lines):

1. Corresponding header (for .cpp files)
2. Standard library headers
3. Third-party libraries
4. Framework headers (`<demiplane/...>`)
5. Local project headers

```cpp
#include "query_compiler.hpp"

#include <algorithm>
#include <string>

#include <boost/asio.hpp>

#include <demiplane/gears>

#include <db_table.hpp>
#include "local_helper.hpp"
```

Use angle brackets for library/component headers, quotes for same-directory:

```cpp
#include <gears_outcome.hpp>   // Component header
#include "local_impl.hpp"      // Same directory
```

## Type Aliases

Use `_type` suffix for member type aliases:

```cpp
using value_type = T;
using error_type = E;
using executor_type = boost::asio::any_io_executor;
```

## Class Layout

Order within class:

1. Public types and aliases
2. Public static members
3. Constructors/destructors
4. Public methods
5. Protected members
6. Private types
7. Private methods
8. Private data members

```cpp
class Table {
public:
    using column_type = TableColumn;

    static std::shared_ptr<Table> make_ptr(std::string name);

    explicit Table(std::string name);
    ~Table() = default;

    [[nodiscard]] constexpr const std::string& name() const noexcept;

private:
    std::string name_;
    std::vector<column_type> columns_;
};
```

## Macros

### Prefix Convention

| Prefix       | Use                                                                   |
|--------------|-----------------------------------------------------------------------|
| `DMP_*`      | Global feature flags consumed across the whole framework              |
| `<MODULE>_*` | Module-scoped macros — `GEARS_*`, `SCROLL_*`, `NEXUS_*`               |
| short name   | Product-API macros where brevity matters — `LOG_*`, `COMPONENT_LOG_*` |

Examples: `DMP_ENABLE_LOGGING`, `DMP_COMPONENT_LOGGING`, `GEARS_UNREACHABLE`,
`SCROLL_COMPONENT_PREFIX`, `NEXUS_REGISTER`.

### Feature-Flag Disabled Fallbacks

When a macro is gated behind a feature flag, the `#else` branch must expand to a
compilable no-op so call sites never need `#ifdef` guards:

```cpp
#ifdef DMP_ENABLE_LOGGING
    #define LOG_INF(...) /* full expansion, stream-valued */
    #define LOG_DIRECT_FMT(logger_ptr, level, prefix, fmt, ...)                \
        (logger_ptr)->log(level, prefix, std::source_location::current(), fmt, __VA_ARGS__)
#else
    #define LOG_INF(...) ::demiplane::scroll::DummyStream()   // stream-valued → dummy
    #define LOG_DIRECT_FMT(...) ((void)0)                     // statement-form → no-op
#endif
```

### Logging

The `common/scroll` module defines three families of logging macros (`LOG_*`,
`COMPONENT_LOG_*`, `LOG_DIRECT_*`), each with its own precondition on the call site.
See `common/scroll/provider/include/log_macros.hpp` for the full set and the class-scope
setup required by `SCROLL_COMPONENT_PREFIX`.