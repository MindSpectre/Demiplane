# PostgreSQL Transaction Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement Transaction, AutoTransaction, Savepoint, and CapabilityProvider concept as the final v0 PostgreSQL
client component.

**Architecture:** Transaction holds a ConnectionSlot for its lifetime, provides with_sync()/with_async() returning
non-owning executors (reusing existing PGconn* constructors). AutoTransaction wraps Transaction with auto-begin.
Savepoint is a scoped object borrowing PGconn* from Transaction. CapabilityProvider is a zero-overhead concept satisfied
by Session, Transaction, and AutoTransaction.

**Tech Stack:** C++23, libpq, Boost.Asio, Google Test, CMake

**Reference files:**

- Design: `components/database/providers/postgresql/transaction/TRANSACTION_DESIGN.md`
- Session pattern: `components/database/providers/postgresql/session/session.hpp` / `session.cpp`
- Executors: `components/database/providers/postgresql/capabilities/executors/sync_executor/postgres_sync_executor.hpp`
- ConnectionSlot: `components/database/providers/postgresql/connection_slot/connection_slot.hpp`
- Errors: `components/database/providers/postgresql/errors/postgres_errors.hpp`
- Error codes: `components/database/base/errors/db_error_codes.hpp`
- Session tests: `tests/integration_tests/database/pgsql/session/session_test.cpp`

**Conventions:**

- Namespace: `demiplane::db::postgres`
- Flat file layout (match Session pattern): `*.hpp` and `*.cpp` at directory root, not in `include/` / `source/`
- `gears::NonCopyable` base for move-only types
- `[[nodiscard]]` on all getters and `Outcome` returns
- `noexcept` on trivial accessors
- `{}` initialization, `snake_case_` members
- Logging via `#include "log_macros.hpp"` with `COMPONENT_LOG_*` macros

**Error code reuse:** The existing `db_error_codes.hpp` already has `ClientErrorCode::InvalidState`,
`ClientErrorCode::TransactionActive`, `ClientErrorCode::NoActiveTransaction`, `ClientErrorCode::PoolExhausted`. We reuse
these rather than introducing a separate `TransactionErrorCode` enum. Server-side errors (BEGIN/COMMIT/ROLLBACK
failures) naturally produce `ErrorContext` via existing `extract_error()`.

---

### Task 1: TransactionStatus enum and TransactionOptions struct

Create the foundation types. These are pure value types with no database dependency.

**Files:**

- Create: `components/database/providers/postgresql/transaction/transaction_status.hpp`
- Create: `components/database/providers/postgresql/transaction/transaction_options.hpp`
- Create: `components/database/providers/postgresql/transaction/transaction_options.cpp`

**Step 1: Create TransactionStatus enum**

Create `transaction_status.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace demiplane::db::postgres {

    enum class TransactionStatus : std::uint8_t {
        IDLE,         // Created but begin() not yet called
        ACTIVE,       // BEGIN sent, queries can execute
        COMMITTED,    // COMMIT sent successfully
        ROLLED_BACK,  // ROLLBACK sent successfully
        FAILED,       // A control command failed
    };

    [[nodiscard]] constexpr const char* to_string(const TransactionStatus status) noexcept {
        switch (status) {
            case TransactionStatus::IDLE:        return "IDLE";
            case TransactionStatus::ACTIVE:      return "ACTIVE";
            case TransactionStatus::COMMITTED:   return "COMMITTED";
            case TransactionStatus::ROLLED_BACK: return "ROLLED_BACK";
            case TransactionStatus::FAILED:      return "FAILED";
        }
        return "UNKNOWN";
    }

}  // namespace demiplane::db::postgres
```

**Step 2: Create TransactionOptions struct**

Create `transaction_options.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace demiplane::db::postgres {

    enum class IsolationLevel : std::uint8_t {
        READ_COMMITTED,
        REPEATABLE_READ,
        SERIALIZABLE,
    };

    enum class AccessMode : std::uint8_t {
        READ_WRITE,
        READ_ONLY,
    };

    [[nodiscard]] constexpr const char* to_string(const IsolationLevel level) noexcept {
        switch (level) {
            case IsolationLevel::READ_COMMITTED:  return "READ COMMITTED";
            case IsolationLevel::REPEATABLE_READ: return "REPEATABLE READ";
            case IsolationLevel::SERIALIZABLE:    return "SERIALIZABLE";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] constexpr const char* to_string(const AccessMode mode) noexcept {
        switch (mode) {
            case AccessMode::READ_WRITE: return "READ WRITE";
            case AccessMode::READ_ONLY:  return "READ ONLY";
        }
        return "UNKNOWN";
    }

    struct TransactionOptions {
        IsolationLevel isolation = IsolationLevel::READ_COMMITTED;
        AccessMode access        = AccessMode::READ_WRITE;
        bool deferrable          = false;

        [[nodiscard]] std::string to_begin_sql() const;
    };

}  // namespace demiplane::db::postgres
```

**Step 3: Implement to_begin_sql()**

Create `transaction_options.cpp`:

```cpp
#include "transaction_options.hpp"

#include <stdexcept>

namespace demiplane::db::postgres {

    std::string TransactionOptions::to_begin_sql() const {
        if (deferrable && (isolation != IsolationLevel::SERIALIZABLE || access != AccessMode::READ_ONLY)) {
            throw std::invalid_argument("DEFERRABLE is only valid with SERIALIZABLE READ ONLY");
        }

        std::string sql = "BEGIN";
        sql += " ISOLATION LEVEL ";
        sql += to_string(isolation);
        sql += ' ';
        sql += to_string(access);

        if (deferrable) {
            sql += " DEFERRABLE";
        }

        return sql;
    }

}  // namespace demiplane::db::postgres
```

**Step 4: Commit**

```
git add components/database/providers/postgresql/transaction/transaction_status.hpp \
      components/database/providers/postgresql/transaction/transaction_options.hpp \
      components/database/providers/postgresql/transaction/transaction_options.cpp
git commit -m "feat(transaction): add TransactionStatus enum and TransactionOptions struct"
```

---

### Task 2: Unit tests for TransactionOptions

**Files:**

- Create: `tests/unit_tests/database/postgresql/transaction_options_test.cpp`
- Create: `tests/unit_tests/database/postgresql/CMakeLists.txt`
- Modify: `tests/unit_tests/database/CMakeLists.txt`

**Step 1: Create unit test directory and CMakeLists**

Create `tests/unit_tests/database/postgresql/CMakeLists.txt`:

```cmake
# PostgreSQL Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Database.PostgreSQL.TransactionOptions
        transaction_options_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${DMP_DATABASE_POSTGRESQL}.Transaction
)
```

Add subdirectory to `tests/unit_tests/database/CMakeLists.txt`:

```cmake
add_subdirectory(base)
add_subdirectory(primitives)
add_subdirectory(postgresql)
```

**Step 2: Write TransactionOptions tests**

Create `tests/unit_tests/database/postgresql/transaction_options_test.cpp`:

```cpp
#include <transaction_options.hpp>

#include <gtest/gtest.h>

using namespace demiplane::db::postgres;

class TransactionOptionsTest : public ::testing::Test {};

TEST_F(TransactionOptionsTest, DefaultOptionsProduceReadCommittedReadWrite) {
    TransactionOptions opts;
    EXPECT_EQ(opts.to_begin_sql(), "BEGIN ISOLATION LEVEL READ COMMITTED READ WRITE");
}

TEST_F(TransactionOptionsTest, RepeatableReadIsolation) {
    TransactionOptions opts{.isolation = IsolationLevel::REPEATABLE_READ};
    EXPECT_EQ(opts.to_begin_sql(), "BEGIN ISOLATION LEVEL REPEATABLE READ READ WRITE");
}

TEST_F(TransactionOptionsTest, SerializableReadOnly) {
    TransactionOptions opts{
        .isolation = IsolationLevel::SERIALIZABLE,
        .access = AccessMode::READ_ONLY,
    };
    EXPECT_EQ(opts.to_begin_sql(), "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY");
}

TEST_F(TransactionOptionsTest, SerializableReadOnlyDeferrable) {
    TransactionOptions opts{
        .isolation = IsolationLevel::SERIALIZABLE,
        .access = AccessMode::READ_ONLY,
        .deferrable = true,
    };
    EXPECT_EQ(opts.to_begin_sql(), "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY DEFERRABLE");
}

TEST_F(TransactionOptionsTest, DeferrableWithNonSerializableThrows) {
    TransactionOptions opts{
        .isolation = IsolationLevel::READ_COMMITTED,
        .deferrable = true,
    };
    EXPECT_THROW(opts.to_begin_sql(), std::invalid_argument);
}

TEST_F(TransactionOptionsTest, DeferrableWithSerializableReadWriteThrows) {
    TransactionOptions opts{
        .isolation = IsolationLevel::SERIALIZABLE,
        .access = AccessMode::READ_WRITE,
        .deferrable = true,
    };
    EXPECT_THROW(opts.to_begin_sql(), std::invalid_argument);
}
```

**Step 3: Update transaction CMakeLists for the library target**

Rewrite `components/database/providers/postgresql/transaction/CMakeLists.txt` (replace existing skeleton to use flat
layout):

```cmake
##############################################################################
# PostgreSql Transaction
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Transaction STATIC
        status/transaction_status.hpp
        options/transaction_options.hpp
        options/transaction_options.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Transaction PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Transaction
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.ConnectionSlot
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async
        ${DMP_DATABASE_POSTGRESQL}.Errors
        Boost::asio
)
##############################################################################
```

Uncomment `add_subdirectory(transaction)` in `components/database/providers/postgresql/CMakeLists.txt` (add it before
the `add_combined_library` call).

**Step 4: Build and run unit tests**

Run:
`cmake --preset=debug && cmake --build build/debug --target ${UNIT_TESTING_TARGET}.Database.PostgreSQL.TransactionOptions`
Run: `ctest --test-dir build/debug -R "TransactionOptions" -V`
Expected: All 6 tests PASS.

**Step 5: Commit**

```
git add tests/unit_tests/database/postgresql/ \
      tests/unit_tests/database/CMakeLists.txt \
      components/database/providers/postgresql/transaction/CMakeLists.txt \
      components/database/providers/postgresql/CMakeLists.txt
git commit -m "feat(transaction): add TransactionOptions unit tests and CMake wiring"
```

---

### Task 3: Transaction class implementation

**Files:**

- Create: `components/database/providers/postgresql/transaction/postgres_transaction.hpp`
- Create: `components/database/providers/postgresql/transaction/postgres_transaction.cpp`
- Modify: `components/database/providers/postgresql/transaction/CMakeLists.txt`

**Step 1: Create Transaction header**

Create `postgres_transaction.hpp`:

```cpp
#pragma once

#include <gears_class_traits.hpp>
#include <postgres_async_executor.hpp>
#include <postgres_sync_executor.hpp>

#include "transaction_options.hpp"
#include "transaction_status.hpp"

namespace demiplane::db::postgres {

    struct ConnectionSlot;
    class Session;
    class Savepoint;

    class Transaction : gears::NonCopyable {
    public:
        using executor_type = boost::asio::any_io_executor;

        ~Transaction();

        Transaction(Transaction&& other) noexcept;
        Transaction& operator=(Transaction&& other) noexcept;

        // ============== Lifecycle Control (sync) ==============

        [[nodiscard]] gears::Outcome<void, ErrorContext> begin();
        [[nodiscard]] gears::Outcome<void, ErrorContext> commit();
        [[nodiscard]] gears::Outcome<void, ErrorContext> rollback();

        // ============== Capability Provision ==============

        [[nodiscard]] SyncExecutor with_sync();
        [[nodiscard]] AsyncExecutor with_async(executor_type exec);

        // ============== Savepoints ==============

        [[nodiscard]] gears::Outcome<Savepoint, ErrorContext> savepoint(std::string name);

        // ============== Introspection ==============

        [[nodiscard]] TransactionStatus status() const noexcept;
        [[nodiscard]] bool is_active() const noexcept;
        [[nodiscard]] bool is_finished() const noexcept;
        [[nodiscard]] PGconn* native_handle() const noexcept;

    private:
        friend class Session;
        Transaction(ConnectionSlot& slot, TransactionOptions opts);

        [[nodiscard]] gears::Outcome<void, ErrorContext> execute_control(std::string_view sql);

        ConnectionSlot* slot_;
        TransactionOptions options_;
        TransactionStatus status_ = TransactionStatus::IDLE;
    };

}  // namespace demiplane::db::postgres
```

**Step 2: Create Transaction source**

Create `postgres_transaction.cpp`:

```cpp
#include "postgres_transaction.hpp"

#include <connection_slot.hpp>
#include <postgres_savepoint.hpp>

#include "log_macros.hpp"

namespace demiplane::db::postgres {

    Transaction::Transaction(ConnectionSlot& slot, TransactionOptions opts)
        : slot_{&slot},
          options_{std::move(opts)} {
        COMPONENT_LOG_INF() << "Transaction created";
    }

    Transaction::~Transaction() {
        if (slot_) {
            slot_->reset();
            COMPONENT_LOG_INF() << "Transaction destroyed, slot released";
        }
    }

    Transaction::Transaction(Transaction&& other) noexcept
        : slot_{std::exchange(other.slot_, nullptr)},
          options_{std::move(other.options_)},
          status_{std::exchange(other.status_, TransactionStatus::FAILED)} {
    }

    Transaction& Transaction::operator=(Transaction&& other) noexcept {
        if (this != &other) {
            if (slot_) {
                slot_->reset();
            }
            slot_    = std::exchange(other.slot_, nullptr);
            options_ = std::move(other.options_);
            status_  = std::exchange(other.status_, TransactionStatus::FAILED);
        }
        return *this;
    }

    // ============== Lifecycle Control ==============

    gears::Outcome<void, ErrorContext> Transaction::begin() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (status_ != TransactionStatus::IDLE) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control(options_.to_begin_sql());
        if (result.is_success()) {
            status_ = TransactionStatus::ACTIVE;
        } else {
            status_ = TransactionStatus::FAILED;
        }
        return result;
    }

    gears::Outcome<void, ErrorContext> Transaction::commit() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (status_ != TransactionStatus::ACTIVE) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control("COMMIT");
        if (result.is_success()) {
            status_ = TransactionStatus::COMMITTED;
        } else {
            status_ = TransactionStatus::FAILED;
        }
        return result;
    }

    gears::Outcome<void, ErrorContext> Transaction::rollback() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (status_ != TransactionStatus::ACTIVE) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control("ROLLBACK");
        if (result.is_success()) {
            status_ = TransactionStatus::ROLLED_BACK;
        } else {
            status_ = TransactionStatus::FAILED;
        }
        return result;
    }

    // ============== Capability Provision ==============

    SyncExecutor Transaction::with_sync() {
        if (status_ != TransactionStatus::ACTIVE) {
            return SyncExecutor{nullptr};
        }
        return SyncExecutor{slot_->conn};
    }

    AsyncExecutor Transaction::with_async(executor_type exec) {
        if (status_ != TransactionStatus::ACTIVE) {
            return AsyncExecutor{nullptr, std::move(exec)};
        }
        return AsyncExecutor{slot_->conn, std::move(exec)};
    }

    // ============== Savepoints ==============

    gears::Outcome<Savepoint, ErrorContext> Transaction::savepoint(std::string name) {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (status_ != TransactionStatus::ACTIVE) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }

        const std::string sql = "SAVEPOINT " + name;
        auto result = execute_control(sql);
        if (!result.is_success()) {
            return gears::Err(result.error<ErrorContext>());
        }

        return Savepoint{slot_->conn, std::move(name)};
    }

    // ============== Introspection ==============

    TransactionStatus Transaction::status() const noexcept {
        return status_;
    }

    bool Transaction::is_active() const noexcept {
        return status_ == TransactionStatus::ACTIVE;
    }

    bool Transaction::is_finished() const noexcept {
        return status_ == TransactionStatus::COMMITTED || status_ == TransactionStatus::ROLLED_BACK;
    }

    PGconn* Transaction::native_handle() const noexcept {
        return slot_ ? slot_->conn : nullptr;
    }

    // ============== Internal ==============

    gears::Outcome<void, ErrorContext> Transaction::execute_control(const std::string_view sql) {
        SyncExecutor exec{slot_->conn};
        auto result = exec.execute(sql);
        if (!result.is_success()) {
            return gears::Err(result.error<ErrorContext>());
        }
        return gears::Ok();
    }

}  // namespace demiplane::db::postgres
```

**Step 3: Update transaction CMakeLists.txt**

Add the new files to the existing target in `components/database/providers/postgresql/transaction/CMakeLists.txt`:

```cmake
##############################################################################
# PostgreSql Transaction
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Transaction STATIC
        status/transaction_status.hpp
        options/transaction_options.hpp
        options/transaction_options.cpp
        manual_transaction/manual_transaction.hpp
        manual_transaction/manual_transaction.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Transaction PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Transaction
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.ConnectionSlot
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async
        ${DMP_DATABASE_POSTGRESQL}.Errors
        Boost::asio
)
##############################################################################
```

**Step 4: Build verification**

Run: `cmake --preset=debug && cmake --build build/debug --target ${DMP_DATABASE_POSTGRESQL}.Transaction`
Expected: Compiles successfully. (Note: savepoint header is forward-declared; the Savepoint include is in the .cpp. This
will fail until Task 5. If so, temporarily comment out the savepoint() method body and include.)

**Step 5: Commit**

```
git add components/database/providers/postgresql/transaction/postgres_transaction.hpp \
      components/database/providers/postgresql/transaction/postgres_transaction.cpp \
      components/database/providers/postgresql/transaction/CMakeLists.txt
git commit -m "feat(transaction): implement Transaction class with lifecycle control and capability provision"
```

---

### Task 4: Savepoint class implementation

**Files:**

- Create: `components/database/providers/postgresql/savepoint/postgres_savepoint.hpp`
- Create: `components/database/providers/postgresql/savepoint/postgres_savepoint.cpp`
- Create: `components/database/providers/postgresql/savepoint/CMakeLists.txt`
- Modify: `components/database/providers/postgresql/CMakeLists.txt`

**Step 1: Create Savepoint header**

Create `postgres_savepoint.hpp`:

```cpp
#pragma once

#include <string>
#include <string_view>

#include <gears_class_traits.hpp>
#include <gears_outcome.hpp>
#include <postgres_errors.hpp>

struct pg_conn;
using PGconn = pg_conn;

namespace demiplane::db::postgres {

    class Savepoint : gears::NonCopyable {
    public:
        ~Savepoint();

        Savepoint(Savepoint&& other) noexcept;
        Savepoint& operator=(Savepoint&& other) noexcept;

        [[nodiscard]] gears::Outcome<void, ErrorContext> rollback();
        [[nodiscard]] gears::Outcome<void, ErrorContext> release();

        [[nodiscard]] std::string_view name() const noexcept;
        [[nodiscard]] bool is_active() const noexcept;

    private:
        friend class Transaction;
        Savepoint(PGconn* conn, std::string name);

        [[nodiscard]] gears::Outcome<void, ErrorContext> execute_control(std::string_view sql);

        PGconn* conn_;
        std::string name_;
        bool active_ = true;
    };

}  // namespace demiplane::db::postgres
```

**Step 2: Create Savepoint source**

Create `postgres_savepoint.cpp`:

```cpp
#include "postgres_savepoint.hpp"

#include <postgres_sync_executor.hpp>

#include "log_macros.hpp"

namespace demiplane::db::postgres {

    Savepoint::Savepoint(PGconn* conn, std::string name)
        : conn_{conn},
          name_{std::move(name)} {
        COMPONENT_LOG_INF() << "Savepoint '" << name_ << "' created";
    }

    Savepoint::~Savepoint() {
        if (active_) {
            [[maybe_unused]] auto _ = release();
        }
    }

    Savepoint::Savepoint(Savepoint&& other) noexcept
        : conn_{std::exchange(other.conn_, nullptr)},
          name_{std::move(other.name_)},
          active_{std::exchange(other.active_, false)} {
    }

    Savepoint& Savepoint::operator=(Savepoint&& other) noexcept {
        if (this != &other) {
            if (active_) {
                [[maybe_unused]] auto _ = release();
            }
            conn_   = std::exchange(other.conn_, nullptr);
            name_   = std::move(other.name_);
            active_ = std::exchange(other.active_, false);
        }
        return *this;
    }

    gears::Outcome<void, ErrorContext> Savepoint::rollback() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (!active_) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control("ROLLBACK TO SAVEPOINT " + name_);
        if (result.is_success()) {
            active_ = false;
        }
        return result;
    }

    gears::Outcome<void, ErrorContext> Savepoint::release() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (!active_) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control("RELEASE SAVEPOINT " + name_);
        if (result.is_success()) {
            active_ = false;
        }
        return result;
    }

    std::string_view Savepoint::name() const noexcept {
        return name_;
    }

    bool Savepoint::is_active() const noexcept {
        return active_;
    }

    gears::Outcome<void, ErrorContext> Savepoint::execute_control(const std::string_view sql) {
        SyncExecutor exec{conn_};
        auto result = exec.execute(sql);
        if (!result.is_success()) {
            return gears::Err(result.error<ErrorContext>());
        }
        return gears::Ok();
    }

}  // namespace demiplane::db::postgres
```

**Step 3: Create Savepoint CMakeLists**

Create `components/database/providers/postgresql/savepoint/CMakeLists.txt`:

```cmake
##############################################################################
# PostgreSql Savepoint
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Savepoint STATIC
        postgres_savepoint.hpp
        postgres_savepoint.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Savepoint PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Savepoint
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync
        ${DMP_DATABASE_POSTGRESQL}.Errors
)
##############################################################################
```

**Step 4: Wire into provider CMakeLists**

In `components/database/providers/postgresql/CMakeLists.txt`, add `add_subdirectory(savepoint)` before the
`add_subdirectory(transaction)` line. Then add `${DMP_DATABASE_POSTGRESQL}.Savepoint` as a dependency of the Transaction
target in the transaction CMakeLists.

Update `components/database/providers/postgresql/transaction/CMakeLists.txt` to add:

```cmake
        ${DMP_DATABASE_POSTGRESQL}.Savepoint
```

to the `target_link_libraries` PUBLIC section.

**Step 5: Build verification**

Run: `cmake --preset=debug && cmake --build build/debug --target ${DMP_DATABASE_POSTGRESQL}.Savepoint`
Run: `cmake --build build/debug --target ${DMP_DATABASE_POSTGRESQL}.Transaction`
Expected: Both compile successfully.

**Step 6: Commit**

```
git add components/database/providers/postgresql/savepoint/ \
      components/database/providers/postgresql/CMakeLists.txt \
      components/database/providers/postgresql/transaction/CMakeLists.txt
git commit -m "feat(savepoint): implement Savepoint class with rollback and release control"
```

---

### Task 5: AutoTransaction class implementation

**Files:**

- Create: `components/database/providers/postgresql/transaction/postgres_auto_transaction.hpp`
- Create: `components/database/providers/postgresql/transaction/postgres_auto_transaction.cpp`
- Modify: `components/database/providers/postgresql/transaction/CMakeLists.txt`

**Step 1: Create AutoTransaction header**

Create `postgres_auto_transaction.hpp`:

```cpp
#pragma once

#include "postgres_transaction.hpp"

namespace demiplane::db::postgres {

    class AutoTransaction : gears::NonCopyable {
    public:
        using executor_type = boost::asio::any_io_executor;

        ~AutoTransaction() = default;

        AutoTransaction(AutoTransaction&&) noexcept = default;
        AutoTransaction& operator=(AutoTransaction&&) noexcept = default;

        [[nodiscard]] gears::Outcome<void, ErrorContext> commit();

        [[nodiscard]] SyncExecutor with_sync();
        [[nodiscard]] AsyncExecutor with_async(executor_type exec);

        [[nodiscard]] gears::Outcome<Savepoint, ErrorContext> savepoint(std::string name);

        [[nodiscard]] TransactionStatus status() const noexcept;
        [[nodiscard]] bool is_active() const noexcept;
        [[nodiscard]] bool is_finished() const noexcept;

    private:
        friend class Session;
        explicit AutoTransaction(Transaction tx);

        Transaction tx_;
    };

}  // namespace demiplane::db::postgres
```

**Step 2: Create AutoTransaction source**

Create `postgres_auto_transaction.cpp`:

```cpp
#include "postgres_auto_transaction.hpp"

namespace demiplane::db::postgres {

    AutoTransaction::AutoTransaction(Transaction tx)
        : tx_{std::move(tx)} {
    }

    gears::Outcome<void, ErrorContext> AutoTransaction::commit() {
        return tx_.commit();
    }

    SyncExecutor AutoTransaction::with_sync() {
        return tx_.with_sync();
    }

    AsyncExecutor AutoTransaction::with_async(executor_type exec) {
        return tx_.with_async(std::move(exec));
    }

    gears::Outcome<Savepoint, ErrorContext> AutoTransaction::savepoint(std::string name) {
        return tx_.savepoint(std::move(name));
    }

    TransactionStatus AutoTransaction::status() const noexcept {
        return tx_.status();
    }

    bool AutoTransaction::is_active() const noexcept {
        return tx_.is_active();
    }

    bool AutoTransaction::is_finished() const noexcept {
        return tx_.is_finished();
    }

}  // namespace demiplane::db::postgres
```

**Step 3: Update transaction CMakeLists.txt**

Add to the `add_library` source list:

```
        postgres_auto_transaction.hpp
        postgres_auto_transaction.cpp
```

**Step 4: Build verification**

Run: `cmake --build build/debug --target ${DMP_DATABASE_POSTGRESQL}.Transaction`
Expected: Compiles successfully.

**Step 5: Commit**

```
git add components/database/providers/postgresql/transaction/postgres_auto_transaction.hpp \
      components/database/providers/postgresql/transaction/postgres_auto_transaction.cpp \
      components/database/providers/postgresql/transaction/CMakeLists.txt
git commit -m "feat(transaction): implement AutoTransaction wrapper with auto-begin semantics"
```

---

### Task 6: CapabilityProvider concept

**Files:**

- Create: `components/database/providers/postgresql/capability_provider/capability_provider.hpp`
- Create: `components/database/providers/postgresql/capability_provider/CMakeLists.txt`
- Modify: `components/database/providers/postgresql/CMakeLists.txt`

**Step 1: Create CapabilityProvider concept header**

Create `capability_provider.hpp`:

```cpp
#pragma once

#include <concepts>

#include <boost/asio/any_io_executor.hpp>
#include <postgres_async_executor.hpp>
#include <postgres_sync_executor.hpp>

namespace demiplane::db::postgres {

    template <typename T>
    concept CapabilityProvider = requires(T provider, boost::asio::any_io_executor exec) {
        { provider.with_sync() } -> std::same_as<SyncExecutor>;
        { provider.with_async(exec) } -> std::same_as<AsyncExecutor>;
    };

}  // namespace demiplane::db::postgres
```

**Step 2: Create CMakeLists**

Create `components/database/providers/postgresql/capability_provider/CMakeLists.txt`:

```cmake
##############################################################################
# PostgreSql CapabilityProvider Concept (header-only)
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.CapabilityProvider INTERFACE)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.CapabilityProvider INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.CapabilityProvider INTERFACE
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async
        Boost::asio
)
##############################################################################
```

**Step 3: Wire into provider CMakeLists**

Add `add_subdirectory(capability_provider)` to `components/database/providers/postgresql/CMakeLists.txt`.

**Step 4: Build verification**

Run: `cmake --preset=debug && cmake --build build/debug`
Expected: Full build succeeds.

**Step 5: Commit**

```
git add components/database/providers/postgresql/capability_provider/ \
      components/database/providers/postgresql/CMakeLists.txt
git commit -m "feat(capability-provider): add CapabilityProvider concept for generic Session/Transaction usage"
```

---

### Task 7: Session factory methods

**Files:**

- Modify: `components/database/providers/postgresql/session/session.hpp`
- Modify: `components/database/providers/postgresql/session/session.cpp`
- Modify: `components/database/providers/postgresql/session/CMakeLists.txt`

**Step 1: Add factory method declarations to Session header**

Add includes at top of `session.hpp`:

```cpp
#include <gears_outcome.hpp>
#include <postgres_errors.hpp>
#include <postgres_transaction.hpp>
#include <postgres_auto_transaction.hpp>
```

Add factory methods to Session class (public section, after `shutdown()`):

```cpp
        // ============== Transaction Factories ==============

        [[nodiscard]] gears::Outcome<Transaction, ErrorContext> begin_transaction(
            TransactionOptions opts = {});

        [[nodiscard]] gears::Outcome<AutoTransaction, ErrorContext> begin_auto_transaction(
            TransactionOptions opts = {});
```

**Step 2: Implement factory methods in Session source**

Add to `session.cpp`:

```cpp
    gears::Outcome<Transaction, ErrorContext> Session::begin_transaction(TransactionOptions opts) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto* slot = cylinder_.acquire_slot();
        if (!slot) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return Transaction{*slot, std::move(opts)};
    }

    gears::Outcome<AutoTransaction, ErrorContext> Session::begin_auto_transaction(TransactionOptions opts) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto* slot = cylinder_.acquire_slot();
        if (!slot) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }

        Transaction tx{*slot, std::move(opts)};
        auto begin_result = tx.begin();
        if (!begin_result.is_success()) {
            return gears::Err(begin_result.error<ErrorContext>());
        }

        return AutoTransaction{std::move(tx)};
    }
```

**Step 3: Update Session CMakeLists dependencies**

Add to `components/database/providers/postgresql/session/CMakeLists.txt` link libraries:

```cmake
        ${DMP_DATABASE_POSTGRESQL}.Transaction
```

**Step 4: Update combined library in provider CMakeLists**

Add `${DMP_DATABASE_POSTGRESQL}.Transaction` and `${DMP_DATABASE_POSTGRESQL}.Savepoint` to the `LIBRARIES` list of the
`add_combined_library` call in `components/database/providers/postgresql/CMakeLists.txt`.

**Step 5: Build verification**

Run: `cmake --preset=debug && cmake --build build/debug`
Expected: Full project builds successfully.

**Step 6: Run existing tests to verify no regressions**

Run: `ctest --test-dir build/debug -L unit -V`
Expected: All existing unit tests pass.

**Step 7: Commit**

```
git add components/database/providers/postgresql/session/session.hpp \
      components/database/providers/postgresql/session/session.cpp \
      components/database/providers/postgresql/session/CMakeLists.txt \
      components/database/providers/postgresql/CMakeLists.txt
git commit -m "feat(session): add begin_transaction() and begin_auto_transaction() factory methods"
```

---

### Task 8: Transaction integration tests

**Files:**

- Create: `tests/integration_tests/database/pgsql/transaction/transaction_lifecycle_test.cpp`
- Create: `tests/integration_tests/database/pgsql/transaction/CMakeLists.txt`
- Modify: `tests/integration_tests/database/pgsql/CMakeLists.txt`

**Step 1: Create test directory CMakeLists**

Create `tests/integration_tests/database/pgsql/transaction/CMakeLists.txt`:

```cmake
##############################################################################
# PostgreSQL Transaction Integration Tests
##############################################################################

add_integration_test(
        ${INTEGRATION_POSTGRESQL_TESTS}.Transaction.Lifecycle
        transaction_lifecycle_test.cpp
        LINK_LIBS
        Demiplane::Component::Database::PostgreSQL
        ${TEST_LIBS}
        Boost::asio
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
```

Add `add_subdirectory(transaction)` to `tests/integration_tests/database/pgsql/CMakeLists.txt`.

**Step 2: Write transaction lifecycle tests**

Create `transaction_lifecycle_test.cpp`:

```cpp
#include <session.hpp>
#include <postgres_transaction.hpp>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

using namespace demiplane::db::postgres;
using namespace demiplane::db;
using namespace std::chrono_literals;

static ConnectionConfig make_test_config() {
    const auto credentials =
        ConnectionCredentials{}
            .host(demiplane::gears::value_or(std::getenv("POSTGRES_HOST"), "localhost"))
            .port(demiplane::gears::value_or(std::getenv("POSTGRES_PORT"), "5433"))
            .dbname(demiplane::gears::value_or(std::getenv("POSTGRES_DB"), "test_db"))
            .user(demiplane::gears::value_or(std::getenv("POSTGRES_USER"), "test_user"))
            .password(demiplane::gears::value_or(std::getenv("POSTGRES_PASSWORD"), "test_password"));
    ConnectionConfig config{std::move(credentials)};
    config.ssl_mode(SslMode::DISABLE).validate();
    return config;
}

class TransactionLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto conn_string = make_test_config().to_connection_string();
        PGconn* probe          = PQconnectdb(conn_string.c_str());
        if (!probe || PQstatus(probe) != CONNECTION_OK) {
            const std::string error = probe ? PQerrorMessage(probe) : "null connection";
            if (probe) PQfinish(probe);
            GTEST_SKIP() << "PostgreSQL unavailable: " << error;
        }
        PQfinish(probe);

        session_ = std::make_unique<Session>(make_test_config(), CylinderConfig{}.capacity(4).min_connections(1));

        auto exec = session_->with_sync();
        auto r1   = exec.execute(R"(
            CREATE TABLE IF NOT EXISTS tx_test (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL
            )
        )");
        ASSERT_TRUE(r1.is_success()) << r1.error<ErrorContext>().format();

        auto r2 = exec.execute("TRUNCATE TABLE tx_test RESTART IDENTITY CASCADE");
        ASSERT_TRUE(r2.is_success()) << r2.error<ErrorContext>().format();
    }

    void TearDown() override {
        if (session_) {
            auto exec               = session_->with_sync();
            [[maybe_unused]] auto _ = exec.execute("DROP TABLE IF EXISTS tx_test CASCADE");
            session_->shutdown();
        }
    }

    std::unique_ptr<Session> session_;
    boost::asio::io_context io_;
};

// ============== Manual Transaction Tests ==============

TEST_F(TransactionLifecycleTest, BeginCommitPersistsData) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());
    EXPECT_EQ(tx.status(), TransactionStatus::ACTIVE);

    auto insert = tx.with_sync().execute("INSERT INTO tx_test (name) VALUES ('Alice')");
    ASSERT_TRUE(insert.is_success()) << insert.error<ErrorContext>().format();

    ASSERT_TRUE(tx.commit().is_success());
    EXPECT_EQ(tx.status(), TransactionStatus::COMMITTED);

    // Verify data persisted via session
    auto exec   = session_->with_sync();
    auto select = exec.execute("SELECT name FROM tx_test WHERE name = 'Alice'");
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().rows(), 1);
}

TEST_F(TransactionLifecycleTest, BeginRollbackDiscardsData) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    auto insert = tx.with_sync().execute("INSERT INTO tx_test (name) VALUES ('Bob')");
    ASSERT_TRUE(insert.is_success());

    ASSERT_TRUE(tx.rollback().is_success());
    EXPECT_EQ(tx.status(), TransactionStatus::ROLLED_BACK);

    // Verify data NOT persisted
    auto exec   = session_->with_sync();
    auto select = exec.execute("SELECT name FROM tx_test WHERE name = 'Bob'");
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().rows(), 0);
}

TEST_F(TransactionLifecycleTest, DestructorReleasesSlotWithoutCommit) {
    const auto free_before = session_->cylinder_free_count();

    {
        auto tx_result = session_->begin_transaction();
        ASSERT_TRUE(tx_result.is_success());
        auto tx = std::move(tx_result.value());
        ASSERT_TRUE(tx.begin().is_success());

        auto insert = tx.with_sync().execute("INSERT INTO tx_test (name) VALUES ('Charlie')");
        ASSERT_TRUE(insert.is_success());
        // tx destroyed without commit/rollback — slot released, pool cleans up
    }

    EXPECT_GE(session_->cylinder_free_count(), free_before);

    // Verify data NOT persisted (pool sends DISCARD ALL which implies rollback)
    auto exec   = session_->with_sync();
    auto select = exec.execute("SELECT name FROM tx_test WHERE name = 'Charlie'");
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().rows(), 0);
}

TEST_F(TransactionLifecycleTest, StatusStartsAsIdle) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());

    EXPECT_EQ(tx.status(), TransactionStatus::IDLE);
    EXPECT_FALSE(tx.is_active());
    EXPECT_FALSE(tx.is_finished());
}

TEST_F(TransactionLifecycleTest, CommitWithoutBeginReturnsError) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());

    auto result = tx.commit();
    ASSERT_FALSE(result.is_success());
    EXPECT_EQ(result.error<ErrorContext>().code, ClientErrorCode::InvalidState);
}

TEST_F(TransactionLifecycleTest, RollbackWithoutBeginReturnsError) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());

    auto result = tx.rollback();
    ASSERT_FALSE(result.is_success());
}

TEST_F(TransactionLifecycleTest, DoubleCommitReturnsError) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());
    ASSERT_TRUE(tx.commit().is_success());

    auto result = tx.commit();
    ASSERT_FALSE(result.is_success());
}

TEST_F(TransactionLifecycleTest, WithSyncOnIdleTransactionReturnsInvalidExecutor) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());

    auto exec = tx.with_sync();
    EXPECT_FALSE(exec.valid());
}

TEST_F(TransactionLifecycleTest, WithSyncOnActiveTransactionReturnsValidExecutor) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());
    ASSERT_TRUE(tx.begin().is_success());

    auto exec = tx.with_sync();
    EXPECT_TRUE(exec.valid());
}

TEST_F(TransactionLifecycleTest, MultipleQueriesInSameTransaction) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());
    ASSERT_TRUE(tx.begin().is_success());

    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO tx_test (name) VALUES ('One')").is_success());
    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO tx_test (name) VALUES ('Two')").is_success());
    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO tx_test (name) VALUES ('Three')").is_success());

    // Verify visible within transaction
    auto count = tx.with_sync().execute("SELECT COUNT(*) FROM tx_test");
    ASSERT_TRUE(count.is_success());
    EXPECT_EQ(count.value().get<int>(0, 0), 3);

    ASSERT_TRUE(tx.commit().is_success());
}

// ============== AutoTransaction Tests ==============

TEST_F(TransactionLifecycleTest, AutoTransactionIsActiveImmediately) {
    auto atx_result = session_->begin_auto_transaction();
    ASSERT_TRUE(atx_result.is_success()) << atx_result.error<ErrorContext>().format();
    auto atx = std::move(atx_result.value());

    EXPECT_TRUE(atx.is_active());
    EXPECT_EQ(atx.status(), TransactionStatus::ACTIVE);
}

TEST_F(TransactionLifecycleTest, AutoTransactionCommitPersistsData) {
    auto atx_result = session_->begin_auto_transaction();
    ASSERT_TRUE(atx_result.is_success());
    auto atx = std::move(atx_result.value());

    auto insert = atx.with_sync().execute("INSERT INTO tx_test (name) VALUES ('Auto')");
    ASSERT_TRUE(insert.is_success());

    ASSERT_TRUE(atx.commit().is_success());

    auto exec   = session_->with_sync();
    auto select = exec.execute("SELECT name FROM tx_test WHERE name = 'Auto'");
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().rows(), 1);
}

TEST_F(TransactionLifecycleTest, AutoTransactionDestructorImplicitRollback) {
    {
        auto atx_result = session_->begin_auto_transaction();
        ASSERT_TRUE(atx_result.is_success());
        auto atx = std::move(atx_result.value());

        auto insert = atx.with_sync().execute("INSERT INTO tx_test (name) VALUES ('Ghost')");
        ASSERT_TRUE(insert.is_success());
        // ~atx without commit — pool cleanup handles rollback
    }

    auto exec   = session_->with_sync();
    auto select = exec.execute("SELECT name FROM tx_test WHERE name = 'Ghost'");
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().rows(), 0);
}
```

**Step 3: Build and run**

Run: `cmake --preset=debug && cmake --build build/debug`
Run: `ctest --test-dir build/debug -R "Transaction.Lifecycle" -V`
Expected: All tests PASS.

**Step 4: Commit**

```
git add tests/integration_tests/database/pgsql/transaction/ \
      tests/integration_tests/database/pgsql/CMakeLists.txt
git commit -m "test(transaction): add integration tests for Transaction and AutoTransaction lifecycle"
```

---

### Task 9: Savepoint integration tests

**Files:**

- Create: `tests/integration_tests/database/pgsql/transaction/savepoint_test.cpp`
- Modify: `tests/integration_tests/database/pgsql/transaction/CMakeLists.txt`

**Step 1: Add test target to CMakeLists**

Add to `tests/integration_tests/database/pgsql/transaction/CMakeLists.txt`:

```cmake

add_integration_test(
        ${INTEGRATION_POSTGRESQL_TESTS}.Transaction.Savepoint
        savepoint_test.cpp
        LINK_LIBS
        Demiplane::Component::Database::PostgreSQL
        ${TEST_LIBS}
        Boost::asio
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
```

**Step 2: Write savepoint tests**

Create `savepoint_test.cpp`:

```cpp
#include <session.hpp>
#include <postgres_savepoint.hpp>
#include <postgres_transaction.hpp>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

using namespace demiplane::db::postgres;
using namespace demiplane::db;

static ConnectionConfig make_test_config() {
    const auto credentials =
        ConnectionCredentials{}
            .host(demiplane::gears::value_or(std::getenv("POSTGRES_HOST"), "localhost"))
            .port(demiplane::gears::value_or(std::getenv("POSTGRES_PORT"), "5433"))
            .dbname(demiplane::gears::value_or(std::getenv("POSTGRES_DB"), "test_db"))
            .user(demiplane::gears::value_or(std::getenv("POSTGRES_USER"), "test_user"))
            .password(demiplane::gears::value_or(std::getenv("POSTGRES_PASSWORD"), "test_password"));
    ConnectionConfig config{std::move(credentials)};
    config.ssl_mode(SslMode::DISABLE).validate();
    return config;
}

class SavepointTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto conn_string = make_test_config().to_connection_string();
        PGconn* probe          = PQconnectdb(conn_string.c_str());
        if (!probe || PQstatus(probe) != CONNECTION_OK) {
            const std::string error = probe ? PQerrorMessage(probe) : "null connection";
            if (probe) PQfinish(probe);
            GTEST_SKIP() << "PostgreSQL unavailable: " << error;
        }
        PQfinish(probe);

        session_ = std::make_unique<Session>(make_test_config(), CylinderConfig{}.capacity(4).min_connections(1));

        auto exec = session_->with_sync();
        ASSERT_TRUE(exec.execute(R"(
            CREATE TABLE IF NOT EXISTS sp_test (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL
            )
        )").is_success());
        ASSERT_TRUE(exec.execute("TRUNCATE TABLE sp_test RESTART IDENTITY CASCADE").is_success());
    }

    void TearDown() override {
        if (session_) {
            auto exec               = session_->with_sync();
            [[maybe_unused]] auto _ = exec.execute("DROP TABLE IF EXISTS sp_test CASCADE");
            session_->shutdown();
        }
    }

    std::unique_ptr<Session> session_;
};

TEST_F(SavepointTest, RollbackToSavepointUndoesWork) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());
    ASSERT_TRUE(tx.begin().is_success());

    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO sp_test (name) VALUES ('Before')").is_success());

    auto sp_result = tx.savepoint("sp1");
    ASSERT_TRUE(sp_result.is_success()) << sp_result.error<ErrorContext>().format();
    auto sp = std::move(sp_result.value());

    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO sp_test (name) VALUES ('After')").is_success());

    // Verify both rows visible within transaction
    auto count = tx.with_sync().execute("SELECT COUNT(*) FROM sp_test");
    ASSERT_TRUE(count.is_success());
    EXPECT_EQ(count.value().get<int>(0, 0), 2);

    // Rollback to savepoint — undoes 'After'
    ASSERT_TRUE(sp.rollback().is_success());
    EXPECT_FALSE(sp.is_active());

    count = tx.with_sync().execute("SELECT COUNT(*) FROM sp_test");
    ASSERT_TRUE(count.is_success());
    EXPECT_EQ(count.value().get<int>(0, 0), 1);

    ASSERT_TRUE(tx.commit().is_success());

    // Verify final state
    auto exec   = session_->with_sync();
    auto select = exec.execute("SELECT name FROM sp_test");
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().rows(), 1);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "Before");
}

TEST_F(SavepointTest, ReleaseSavepointKeepsWork) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());
    ASSERT_TRUE(tx.begin().is_success());

    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO sp_test (name) VALUES ('Keep')").is_success());

    auto sp_result = tx.savepoint("sp_keep");
    ASSERT_TRUE(sp_result.is_success());
    auto sp = std::move(sp_result.value());

    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO sp_test (name) VALUES ('AlsoKeep')").is_success());

    ASSERT_TRUE(sp.release().is_success());
    EXPECT_FALSE(sp.is_active());

    ASSERT_TRUE(tx.commit().is_success());

    auto exec   = session_->with_sync();
    auto select = exec.execute("SELECT COUNT(*) FROM sp_test");
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().get<int>(0, 0), 2);
}

TEST_F(SavepointTest, NestedSavepoints) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());
    ASSERT_TRUE(tx.begin().is_success());

    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO sp_test (name) VALUES ('Base')").is_success());

    auto sp1_result = tx.savepoint("outer");
    ASSERT_TRUE(sp1_result.is_success());
    auto sp1 = std::move(sp1_result.value());

    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO sp_test (name) VALUES ('Middle')").is_success());

    auto sp2_result = tx.savepoint("inner");
    ASSERT_TRUE(sp2_result.is_success());
    auto sp2 = std::move(sp2_result.value());

    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO sp_test (name) VALUES ('Deep')").is_success());

    // Rollback inner savepoint — undoes 'Deep'
    ASSERT_TRUE(sp2.rollback().is_success());

    auto count = tx.with_sync().execute("SELECT COUNT(*) FROM sp_test");
    ASSERT_TRUE(count.is_success());
    EXPECT_EQ(count.value().get<int>(0, 0), 2);  // Base + Middle

    // Release outer savepoint — keeps 'Base' and 'Middle'
    ASSERT_TRUE(sp1.release().is_success());

    ASSERT_TRUE(tx.commit().is_success());

    auto exec   = session_->with_sync();
    auto select = exec.execute("SELECT COUNT(*) FROM sp_test");
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().get<int>(0, 0), 2);
}

TEST_F(SavepointTest, SavepointOnNonActiveTransactionReturnsError) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());
    // Not begun — still IDLE

    auto sp_result = tx.savepoint("bad");
    ASSERT_FALSE(sp_result.is_success());
    EXPECT_EQ(sp_result.error<ErrorContext>().code, ClientErrorCode::InvalidState);
}

TEST_F(SavepointTest, DoubleRollbackReturnsError) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());
    ASSERT_TRUE(tx.begin().is_success());

    auto sp_result = tx.savepoint("sp_double");
    ASSERT_TRUE(sp_result.is_success());
    auto sp = std::move(sp_result.value());

    ASSERT_TRUE(sp.rollback().is_success());

    auto second = sp.rollback();
    ASSERT_FALSE(second.is_success());
    EXPECT_EQ(second.error<ErrorContext>().code, ClientErrorCode::InvalidState);

    ASSERT_TRUE(tx.rollback().is_success());
}

TEST_F(SavepointTest, DestructorReleasesSavepointIfActive) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success());
    auto tx = std::move(tx_result.value());
    ASSERT_TRUE(tx.begin().is_success());

    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO sp_test (name) VALUES ('DtorTest')").is_success());

    {
        auto sp_result = tx.savepoint("sp_dtor");
        ASSERT_TRUE(sp_result.is_success());
        // ~sp — destructor calls release()
    }

    // Transaction should still be usable
    ASSERT_TRUE(tx.with_sync().execute("INSERT INTO sp_test (name) VALUES ('AfterDtor')").is_success());
    ASSERT_TRUE(tx.commit().is_success());

    auto exec   = session_->with_sync();
    auto select = exec.execute("SELECT COUNT(*) FROM sp_test");
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().get<int>(0, 0), 2);
}
```

**Step 3: Build and run**

Run: `cmake --preset=debug && cmake --build build/debug`
Run: `ctest --test-dir build/debug -R "Transaction.Savepoint" -V`
Expected: All tests PASS.

**Step 4: Commit**

```
git add tests/integration_tests/database/pgsql/transaction/savepoint_test.cpp \
      tests/integration_tests/database/pgsql/transaction/CMakeLists.txt
git commit -m "test(savepoint): add integration tests for savepoint create, rollback, release, and nesting"
```

---

### Task 10: Final verification and cleanup

**Step 1: Full build**

Run: `cmake --preset=debug && cmake --build build/debug`
Expected: Clean build, no warnings.

**Step 2: Run all unit tests**

Run: `ctest --test-dir build/debug -L unit -V`
Expected: All pass (including new TransactionOptions tests).

**Step 3: Run all integration tests**

Run: `ctest --test-dir build/debug -L integration -V`
Expected: All pass (including new Transaction, AutoTransaction, and Savepoint tests).

**Step 4: Run existing session tests to verify no regressions**

Run: `ctest --test-dir build/debug -R "Session" -V`
Expected: All existing session tests still pass.

**Step 5: Final commit (if any cleanup needed)**

Only if there were build issues or minor fixes during verification:

```
git add -A
git commit -m "chore(transaction): final cleanup and build verification"
```
