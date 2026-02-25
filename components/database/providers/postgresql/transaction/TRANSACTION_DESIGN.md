# PostgreSQL Transaction Design

## Overview

Transaction is the final component of the v0 PostgreSQL client. It provides manual and automatic transaction control
while satisfying the same `CapabilityProvider` concept as `Session`, allowing generic code to work with either.

## Core Principle

Transaction borrows a `ConnectionSlot` from Session's cylinder and holds it for its entire lifetime. Executors obtained
from Transaction are non-owning — they borrow the `PGconn*` without slot ownership. On destruction, Transaction releases
the slot back to the pool.

## Type Hierarchy

```
CapabilityProvider (concept)
  requires: with_sync() -> SyncExecutor, with_async(exec) -> AsyncExecutor

Session          — satisfies CapabilityProvider (existing, unchanged)
Transaction      — satisfies CapabilityProvider, manual lifecycle
AutoTransaction  — wraps Transaction, auto-begins on construction
Savepoint        — scoped object created from Transaction
```

## CapabilityProvider Concept

```cpp
template <typename T>
concept CapabilityProvider = requires(T provider, boost::asio::any_io_executor exec) {
    { provider.with_sync() } -> std::same_as<SyncExecutor>;
    { provider.with_async(exec) } -> std::same_as<AsyncExecutor>;
};
```

Zero overhead. Satisfied by Session, Transaction, and AutoTransaction. No changes to Session required.

## TransactionStatus

State machine with enforced transitions:

```
IDLE → ACTIVE → COMMITTED
               → ROLLED_BACK
               → FAILED
```

- `IDLE`: created but `begin()` not yet called
- `ACTIVE`: BEGIN sent, queries can execute
- `COMMITTED`: COMMIT sent successfully
- `ROLLED_BACK`: ROLLBACK sent successfully
- `FAILED`: a control command failed

Valid transitions:

- `begin()`: IDLE → ACTIVE
- `commit()`: ACTIVE → COMMITTED
- `rollback()`: ACTIVE → ROLLED_BACK
- Any control failure: current state → FAILED
- Destructor: releases slot regardless of state

## TransactionOptions

```cpp
enum class IsolationLevel : std::uint8_t { READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE };
enum class AccessMode : std::uint8_t { READ_WRITE, READ_ONLY };

struct TransactionOptions {
    IsolationLevel isolation = IsolationLevel::READ_COMMITTED;
    AccessMode access = AccessMode::READ_WRITE;
    bool deferrable = false;  // only valid with SERIALIZABLE + READ_ONLY

    std::string to_begin_sql() const;
};
```

## Transaction

```cpp
class Transaction : gears::NonCopyable {
public:
    using executor_type = boost::asio::any_io_executor;

    ~Transaction();  // releases slot via slot_->reset()

    Transaction(Transaction&&) noexcept;
    Transaction& operator=(Transaction&&) noexcept;

    // Lifecycle control (sync only)
    [[nodiscard]] Outcome<void, ErrorContext> begin();
    [[nodiscard]] Outcome<void, ErrorContext> commit();
    [[nodiscard]] Outcome<void, ErrorContext> rollback();

    // Capability provision
    [[nodiscard]] SyncExecutor with_sync();
    [[nodiscard]] AsyncExecutor with_async(executor_type exec);

    // Savepoints
    [[nodiscard]] Outcome<Savepoint, ErrorContext> savepoint(std::string name);

    // Introspection
    [[nodiscard]] TransactionStatus status() const noexcept;
    [[nodiscard]] bool is_active() const noexcept;
    [[nodiscard]] bool is_finished() const noexcept;
    [[nodiscard]] PGconn* native_handle() const noexcept;

private:
    friend class Session;
    Transaction(ConnectionSlot& slot, TransactionOptions opts);

    ConnectionSlot* slot_;
    TransactionOptions options_;
    TransactionStatus status_ = TransactionStatus::IDLE;
};
```

- `with_sync()` / `with_async()` return executors via `SyncExecutor(conn_)` / `AsyncExecutor(conn_, exec)` — existing
  non-owning constructors, no new types needed
- Control methods return `Outcome<void, ErrorContext>` — invalid state transitions return error without touching the
  connection
- `begin()` internally creates a temporary `SyncExecutor(conn_)` and executes `options_.to_begin_sql()`
- `commit()` / `rollback()` do the same with `"COMMIT"` / `"ROLLBACK"`
- Destructor releases slot regardless of state — pool cleanup (DISCARD ALL) handles dirty connections

## AutoTransaction

```cpp
class AutoTransaction : gears::NonCopyable {
public:
    using executor_type = boost::asio::any_io_executor;

    ~AutoTransaction() = default;

    AutoTransaction(AutoTransaction&&) noexcept = default;
    AutoTransaction& operator=(AutoTransaction&&) noexcept = default;

    // Lifecycle (commit only — begin is automatic, rollback is implicit via dtor)
    [[nodiscard]] Outcome<void, ErrorContext> commit();

    // Delegated capability provision
    [[nodiscard]] SyncExecutor with_sync();
    [[nodiscard]] AsyncExecutor with_async(executor_type exec);

    // Delegated savepoints
    [[nodiscard]] Outcome<Savepoint, ErrorContext> savepoint(std::string name);

    // Delegated introspection
    [[nodiscard]] TransactionStatus status() const noexcept;
    [[nodiscard]] bool is_active() const noexcept;
    [[nodiscard]] bool is_finished() const noexcept;

private:
    friend class Session;
    explicit AutoTransaction(Transaction tx);  // tx.begin() already called

    Transaction tx_;
};
```

- Created via `Session::begin_auto_transaction(opts)` — acquires slot, constructs Transaction, calls `begin()`, wraps in
  AutoTransaction
- Factory returns `Outcome<AutoTransaction, ErrorContext>` since both slot acquisition and BEGIN can fail
- No explicit `rollback()` — uncommitted destruction defers to pool cleanup
- All methods delegate to inner `Transaction`

## Session Factory Methods

```cpp
class Session {
    // ... existing ...
    [[nodiscard]] Outcome<Transaction, ErrorContext> begin_transaction(TransactionOptions opts = {});
    [[nodiscard]] Outcome<AutoTransaction, ErrorContext> begin_auto_transaction(TransactionOptions opts = {});
};
```

## Savepoint

```cpp
class Savepoint : gears::NonCopyable {
public:
    ~Savepoint();  // releases savepoint if still active

    Savepoint(Savepoint&&) noexcept;
    Savepoint& operator=(Savepoint&&) noexcept;

    // Control (sync only)
    [[nodiscard]] Outcome<void, ErrorContext> rollback();  // ROLLBACK TO SAVEPOINT <name>
    [[nodiscard]] Outcome<void, ErrorContext> release();   // RELEASE SAVEPOINT <name>

    // Introspection
    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] bool is_active() const noexcept;

private:
    friend class Transaction;
    Savepoint(PGconn* conn, std::string name);

    PGconn* conn_;
    std::string name_;
    bool active_ = true;
};
```

- Created via `Transaction::savepoint("sp1")` — sends `SAVEPOINT sp1` SQL during factory call
- Returns `Outcome<Savepoint, ErrorContext>`
- Holds raw `PGconn*` borrowed from Transaction (no slot ownership)
- Destructor calls `release()` if still active
- Nested savepoints work naturally — PostgreSQL handles nesting, each Savepoint is independent

## Error Handling

<!-- TODO: TransactionErrorCode was not implemented — reuses existing ClientErrorCode values
     (InvalidState, PoolExhausted, etc.) from postgres_errors.hpp instead. Update this section
     to reflect actual error handling. -->

```cpp
enum class TransactionErrorCode : std::uint8_t {
    INVALID_TRANSITION,
    SLOT_UNAVAILABLE,
    BEGIN_FAILED,
    COMMIT_FAILED,
    ROLLBACK_FAILED,
    SAVEPOINT_FAILED,
};
```

| Operation                                       | Error source                      | Return type                              |
|-------------------------------------------------|-----------------------------------|------------------------------------------|
| `begin_transaction()`                           | Cylinder exhausted                | `Outcome<Transaction, ErrorContext>`     |
| `begin_auto_transaction()`                      | Cylinder exhausted or BEGIN fails | `Outcome<AutoTransaction, ErrorContext>` |
| `begin()` / `commit()` / `rollback()`           | Invalid state or SQL failure      | `Outcome<void, ErrorContext>`            |
| `savepoint(name)`                               | SQL failure or tx not active      | `Outcome<Savepoint, ErrorContext>`       |
| `with_sync()` / `with_async()` on non-active tx | Returns invalid executor          | `valid() == false`                       |

Control methods check `status_` before touching the connection. On SQL failure, `status_` moves to `FAILED`.

## File Layout

<!-- TODO: Layout below shows include/source subdirectories but actual implementation uses flat
     layout (matching Session pattern). Update to reflect actual structure. -->

```
components/database/providers/postgresql/
├── transaction/
│   ├── include/
│   │   ├── postgres_transaction.hpp
│   │   ├── postgres_auto_transaction.hpp
│   │   ├── transaction_options.hpp
│   │   └── transaction_status.hpp
│   └── source/
│       ├── postgres_transaction.cpp
│       ├── postgres_auto_transaction.cpp
│       └── transaction_options.cpp
├── savepoint/
│   ├── include/
│   │   └── postgres_savepoint.hpp
│   └── source/
│       └── postgres_savepoint.cpp
├── capability_provider/
│   └── include/
│       └── capability_provider.hpp         # concept, header-only
```

## CMake Targets

```cmake
${DMP_DATABASE_POSTGRESQL}.Transaction
LINKS: ${DMP_DATABASE_POSTGRESQL}.ConnectionSlot
${DMP_DATABASE_POSTGRESQL}.SyncExecutor
${DMP_DATABASE_POSTGRESQL}.Errors

${DMP_DATABASE_POSTGRESQL}.Savepoint
LINKS: ${DMP_DATABASE_POSTGRESQL}.Errors

${DMP_DATABASE_POSTGRESQL}.CapabilityProvider
INTERFACE only
```

Session gains dependency on Transaction target.

## Testing

Unit tests (no database):

- `transaction_options_test.cpp` — `to_begin_sql()` for all option combinations
- `transaction_status_test.cpp` — state transition validation

Integration tests (live PostgreSQL):

- `transaction_lifecycle_test.cpp` — begin/commit/rollback cycles, data persistence verification
- `transaction_isolation_test.cpp` — isolation level behavior
- `transaction_error_handling_test.cpp` — invalid transitions, connection failures
- `auto_transaction_test.cpp` — auto-begin, implicit cleanup on destruction
- `savepoint_test.cpp` — create/rollback/release, nesting

## Out of Scope

- Two-phase commit — different lifecycle (connection freed after PREPARE TRANSACTION)
- Read/write split — routing concern across multiple connections, lives above Session
- Distributed transactions — coordination across multiple sessions
