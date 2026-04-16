#include "queued_holder.hpp"

#include "blocking_pool.hpp"

namespace demiplane::db::postgres {

    QueuedHolder::QueuedHolder(PGconn* conn, BlockingPool* owner) noexcept
        : ConnectionHolder{conn},
          owner_{owner} {
    }

    QueuedHolder::~QueuedHolder() {
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    void QueuedHolder::reset() noexcept {
        const bool cleanup_ok = run_cleanup_sql();
        if (!cleanup_ok || !conn_ || PQstatus(conn_) != CONNECTION_OK) {
            owner_->drop_dead(this);
            return;
        }
        owner_->return_holder(this);
    }

}  // namespace demiplane::db::postgres
