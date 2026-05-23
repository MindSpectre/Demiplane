#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "sequence.hpp"

namespace demiplane::multithread {

    /**
     * @brief Compile-time contract for consumer wait strategies.
     *
     * A wait strategy decides how a consumer waits when it has caught up to the
     * producer, and how a producer wakes a waiting consumer. It is supplied to the
     * disruptor as a template parameter and stored by value, so calls are inlined
     * and there is no virtual dispatch on the hot path.

     * A model must provide:
     *   - wait_for(sequence, cursor) -> highest available sequence (>= sequence)
     *   - signal()      noexcept  — wake one waiter after publishing
     *   - signal_all()  noexcept  — wake all waiters (e.g. for shutdown)
     */
    template <typename W>
    concept IsWaitStrategy = requires(W w, const std::int64_t sequence, const Sequence& cursor) {
        { w.wait_for(sequence, cursor) } -> std::convertible_to<std::int64_t>;
        { w.signal() } noexcept;
        { w.signal_all() } noexcept;
    };

    /**
     * @brief Type-erased wait strategy for callers that must choose at runtime.
     *
     * Most users name a concrete strategy as the disruptor's template argument and
     * pay no indirection. Some callers (e.g. the logger, driven by config) only know
     * which strategy to use at runtime. `AnyWaitStrategy` satisfies the WaitStrategy
     * concept by holding one heap-allocated strategy behind a single virtual call, so
     * the disruptor core stays virtual-free and this cost is opt-in only for
     * `Disruptor<T, AnyWaitStrategy>`.
     *
     * Construct in place with `AnyWaitStrategy::make<ConcreteStrategy>(args...)`
     * (works for immovable strategies such as Blocking), or implicitly from an
     * already-constructed movable strategy.
     */
    class AnyWaitStrategy {
    public:
        /// Construct the erased strategy in place (supports non-movable strategies).
        /// noexcept: an allocation failure here is treated as terminate-worthy.
        template <IsWaitStrategy WaitStrategy, typename... WaitStrategyArgsTp>
        [[nodiscard]] static AnyWaitStrategy make(WaitStrategyArgsTp&&... args) noexcept {
            return AnyWaitStrategy{std::make_unique<Model<WaitStrategy>>(std::forward<WaitStrategyArgsTp>(args)...)};
        }

        /// Convenience: wrap an already-constructed (movable) concrete strategy.
        template <IsWaitStrategy WaitStrategyTp>
            requires(!std::same_as<std::remove_cvref_t<WaitStrategyTp>, AnyWaitStrategy>)
        explicit(false) AnyWaitStrategy(WaitStrategyTp&& strategy) noexcept
            : impl_{std::make_unique<Model<std::remove_cvref_t<WaitStrategyTp>>>(
                  std::forward<WaitStrategyTp>(strategy))} {
        }

        [[nodiscard]] std::int64_t wait_for(const std::int64_t sequence, const Sequence& cursor) const {
            return impl_->wait_for(sequence, cursor);
        }

        void signal() const noexcept {
            impl_->signal();
        }

        void signal_all() const noexcept {
            impl_->signal_all();
        }

    private:
        struct Concept : gears::NonCopyable {
            Concept()                                                    = default;
            virtual ~Concept()                                           = default;
            virtual std::int64_t wait_for(std::int64_t, const Sequence&) = 0;
            virtual void signal() noexcept                               = 0;
            virtual void signal_all() noexcept                           = 0;
        };

        template <IsWaitStrategy WaitStrategyT>
        struct Model final : Concept {
            template <typename... WaitStrategyArgsTp>
            explicit Model(WaitStrategyArgsTp&&... args)
                : strategy_{std::forward<WaitStrategyArgsTp>(args)...} {
            }

            std::int64_t wait_for(const std::int64_t sequence, const Sequence& cursor) override {
                return strategy_.wait_for(sequence, cursor);
            }
            void signal() noexcept override {
                strategy_.signal();
            }
            void signal_all() noexcept override {
                strategy_.signal_all();
            }

            WaitStrategyT strategy_;
        };

        explicit AnyWaitStrategy(std::unique_ptr<Concept> impl) noexcept
            : impl_{std::move(impl)} {
        }
        std::unique_ptr<Concept> impl_;
    };

    static_assert(IsWaitStrategy<AnyWaitStrategy>, "AnyWaitStrategy must satisfy the WaitStrategy concept");

}  // namespace demiplane::multithread
