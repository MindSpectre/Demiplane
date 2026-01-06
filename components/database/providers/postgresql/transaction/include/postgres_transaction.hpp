// postgres_transaction.hpp

// TODO: review design
#pragma once
#include <memory>
#include <string>
#include <vector>

#include <boost/asio/awaitable.hpp>

#include "postgres_connection.hpp"

namespace demiplane::db::postgres {

    // Forward declarations
    class Session;

    // Base transaction interface
    class Transaction {
    public:
        virtual ~Transaction() = default;

        virtual asio::awaitable<void> begin()    = 0;
        virtual asio::awaitable<void> commit()   = 0;
        virtual asio::awaitable<void> rollback() = 0;

        virtual asio::awaitable<ResultBlock> exec(std::string_view sql)                              = 0;
        virtual asio::awaitable<ResultBlock> exec_params(std::string_view sql, const Params& params) = 0;

        [[nodiscard]] bool is_active() const {
            return active_;
        }

    protected:
        bool active_{false};
        TransactionIsolation isolation_{TransactionIsolation::READ_COMMITTED};
    };

    // Single connection transaction
    class SimpleTransaction : public Transaction {
    public:
        explicit SimpleTransaction(Connection& conn,
                                   TransactionIsolation isolation = TransactionIsolation::READ_COMMITTED);

        asio::awaitable<void> begin() override;
        asio::awaitable<void> commit() override;
        asio::awaitable<void> rollback() override;

        asio::awaitable<ResultBlock> exec(std::string_view sql) override;
        asio::awaitable<ResultBlock> exec_params(std::string_view sql, const Params& params) override;

        // Savepoint support
        asio::awaitable<void> savepoint(std::string_view name);
        asio::awaitable<void> release_savepoint(std::string_view name);
        asio::awaitable<void> rollback_to_savepoint(std::string_view name);

    private:
        Connection* conn_;
    };

    // Multi-connection distributed transaction
    class DistributedTransaction : public Transaction {
    public:
        enum class CommitProtocol {
            BEST_EFFORT,  // Try to commit all, ignore failures
            TWO_PHASE,    // Full 2PC with prepare phase
            EVENTUAL      // Saga-style with compensations
        };

        explicit DistributedTransaction(std::vector<Connection*> participants,
                                        CommitProtocol protocol = CommitProtocol::TWO_PHASE);

        asio::awaitable<void> begin() override;
        asio::awaitable<void> commit() override;
        asio::awaitable<void> rollback() override;

        // Execute on all participants
        asio::awaitable<ResultBlock> exec(std::string_view sql) override;
        asio::awaitable<ResultBlock> exec_params(std::string_view sql, const Params& params) override;

        // Execute on specific participant
        asio::awaitable<ResultBlock> exec_on(size_t participant_idx, std::string_view sql);

        // Execute on participants matching role
        asio::awaitable<std::vector<ResultBlock>> exec_on_role(NodeRole role, std::string_view sql);

    private:
        asio::awaitable<void> two_phase_commit();
        asio::awaitable<void> best_effort_commit();

        std::vector<Connection*> participants_;
        CommitProtocol protocol_;
        std::string global_transaction_id_;
        std::vector<bool> prepared_;  // Track 2PC state
    };

    // Read-write splitting transaction
    class ReadWriteSplitTransaction : public Transaction {
    public:
        ReadWriteSplitTransaction(Connection& primary, std::vector<Connection*> replicas);

        asio::awaitable<void> begin() override;
        asio::awaitable<void> commit() override;
        asio::awaitable<void> rollback() override;

        asio::awaitable<ResultBlock> exec(std::string_view sql) override;
        asio::awaitable<ResultBlock> exec_params(std::string_view sql, const Params& params) override;

        // Explicit routing
        asio::awaitable<ResultBlock> exec_read(std::string_view sql);
        asio::awaitable<ResultBlock> exec_write(std::string_view sql);

    private:
        [[nodiscard]] bool is_read_query(std::string_view sql) const;
        Connection& select_replica();

        Connection* primary_;
        std::vector<Connection*> replicas_;
        size_t replica_index_{0};  // For round-robin
    };

    // RAII wrapper
    class AutoTransaction {
    public:
        explicit AutoTransaction(std::unique_ptr<Transaction> tx);
        ~AutoTransaction() noexcept;

        AutoTransaction(AutoTransaction&&) noexcept;
        AutoTransaction& operator=(AutoTransaction&&) noexcept;

        asio::awaitable<void> commit();
        Transaction* operator->() const {
            return tx_.get();
        }

    private:
        std::unique_ptr<Transaction> tx_;
        bool committed_{false};
    };

}  // namespace demiplane::db::postgres
