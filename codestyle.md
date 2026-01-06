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

### `GEARS_UNREACHABLE()`

Use in `if constexpr` chains to catch unhandled types at compile time:

```cpp
template <typename T>
constexpr auto process(T value) {
    if constexpr (std::integral<T>) {
        return value * 2;
    } else if constexpr (std::floating_point<T>) {
        return value * 2.0;
    } else {
        GEARS_UNREACHABLE();  // Compile error if instantiated
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

### Requires Clause Placement

Place after template parameters, on own line with indent:

```cpp
template <typename F>
    requires std::invocable<F, T&>
constexpr auto and_then(F&& f) & -> std::invoke_result_t<F, T&>;
```

### Deducing This (C++23)

Prefer deducing this over CRTP:

```cpp
template <typename Self>
constexpr decltype(auto) value(this Self&& self) noexcept {
    return std::forward<Self>(self).value_;
}
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