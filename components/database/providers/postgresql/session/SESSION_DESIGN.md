# PostgreSQL Session & Transaction Design

## Core Idea

`CapabilityProvider` is the shared interface. `Session` and `Transaction` are both capability providers that differ only in how they obtain a connection.

```
CapabilityProvider (interface)
  |-- with_async(io_executor) -> ScopedAsyncExecutor
  |-- with_sync()             -> ScopedSyncExecutor
  |   (later: with_copier(), with_pipeline())
  |
  |-- Session      : owns pool, borrows conn per capability call, returns on scope end
  |-- Transaction  : holds one conn for its lifetime, no-op release
```

## Session

- Owns (or wraps) a connection pool of `PGconn*`.
- Each `with_*()` call borrows a connection from the pool, wraps it in a scoped executor, and returns it when the scope ends.
- No single-connection bottleneck -- multiple concurrent operations use different connections.

```cpp
auto session = Session(pool);

// autocommit -- conn borrowed and returned per call
auto oc = co_await session.with_async(io_exec).execute("SELECT 1");
```

## Transaction

- Created via `session.in_transaction(opts)` -- borrows one connection from the session's pool.
- Provides the same `with_async()` / `with_sync()` capability interface, but all calls use the held connection.
- Explicit lifecycle: `begin()`, `commit()`, `rollback()`.
- Destructor returns connection to pool. Pool handles cleanup (ROLLBACK + DISCARD ALL) if transaction was left open.

```cpp
Transaction tx = session.in_transaction(opts);
tx.begin();

auto oc = co_await tx.with_async(io_exec).execute("INSERT ...");
if (!oc) { tx.rollback(); return; }

tx.commit();
// ~tx: connection returned to pool
```

## CapabilityProvider Interface

```cpp
class CapabilityProvider {
protected:
    virtual PGconn* acquire_connection() = 0;
    virtual void release_connection(PGconn*) = 0;

public:
    ScopedAsyncExecutor with_async(executor_type exec);
    ScopedSyncExecutor  with_sync();
};
```

- `Session::acquire` = borrow from pool, `release` = return to pool.
- `Transaction::acquire` = return held conn, `release` = no-op.
- Scoped executors call `release` on destruction.

## TransactionOptions

```cpp
struct TransactionOptions {
    IsolationLevel isolation = IsolationLevel::READ_COMMITTED;
    AccessMode access        = AccessMode::READ_WRITE;
    bool deferrable          = false;  // valid only with SERIALIZABLE + READ_ONLY

    void validate() const;
    std::string to_begin_sql() const;
};
```

## Savepoints

Scoped objects within a transaction. Hold `PGconn*`, send SQL directly.

```cpp
auto sp = tx.savepoint("sp1");   // SAVEPOINT sp1
// ... risky work ...
sp.rollback();                   // ROLLBACK TO SAVEPOINT sp1
// or
sp.release();                    // RELEASE SAVEPOINT sp1
```

## Transaction Control Implementation

`begin()` / `commit()` / `rollback()` use the held connection directly via `SyncExecutor` internally. Async variants (`begin_async`, etc.) available if needed.

## Out of Scope (separate abstractions)

- **Two-phase commit** -- different lifecycle (connection freed after PREPARE TRANSACTION).
- **Read/write split** -- routing concern across multiple connections, lives above Session.
- **Distributed transactions** -- coordination across multiple sessions.

## Connection Cleanup

Pool responsibility. When a connection is returned, pool sends `ROLLBACK; DISCARD ALL` before making it available. Transaction destructor does not implicitly rollback.
