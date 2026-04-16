#pragma once

#include <condition_variable>

namespace demiplane::db::postgres {

    class QueuedHolder;

    /**
     * FIFO waiter node for BlockingPool::acquire.
     *
     * Stack-allocated inside acquire() and pushed as a raw pointer onto
     * BlockingPool::waiters_ under the pool mutex. A releaser pops the
     * head waiter, sets assigned + ready, then notifies its cv. The
     * waiter re-checks the predicate under the pool mutex to handle the
     * race between cv.wait_for returning and releaser handoff.
     */
    struct Waiter {
        QueuedHolder* assigned = nullptr;
        std::condition_variable cv;
        bool ready = false;
    };

}  // namespace demiplane::db::postgres
