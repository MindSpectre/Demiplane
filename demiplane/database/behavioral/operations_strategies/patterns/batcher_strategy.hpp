#pragma once
#include "strategy_base.hpp"
#include <queue>

namespace demiplane::database::behavioral::strategies
{
    template <typename... Args>
    class BatcherStrategy final : public StrategyBase<Args...>
    {
    private:
        void internal_logic(Args... query) override
        {
            await.push(std::make_tuple(query...));

            if (await.size() >= threshold)
            {
                auto result = Merge(*this);
                std::apply(this->action, result);
            }
        }

        std::size_t threshold = 1;
        std::function<std::tuple<Args...>(BatcherStrategy<Args...>&)> Merge;

    public:
        std::queue<std::tuple<Args...>> await;
        void set_threshold(std::size_t threshold) { this->threshold = threshold; }
        [[nodiscard]] std::size_t get_threshold() const { return threshold; }
        BatcherStrategy() = default;

        ~BatcherStrategy() override
        {
            if (Merge && await.size() != 0)
            {
                auto result = Merge(*this);
                if (this->action)
                {
                    std::apply(this->action, result); // Call the inherited action method
                }
            }
        }

        void SetMerge(std::function<std::tuple<Args...>(BatcherStrategy<Args...>&)> externalProcess)
        {
            Merge = externalProcess;
        }
    };
}
