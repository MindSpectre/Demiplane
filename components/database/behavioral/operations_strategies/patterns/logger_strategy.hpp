#pragma once

#include "strategy_base.hpp"

namespace demiplane::database::behavioral::strategies
{
    template <typename... Args>
    class LoggerStrategy final : public StrategyBase<Args...>
    {
    private:
        void internal_logic(Args... query) override
        {
            std::cerr << "LOGGING:......\n";
            std::cerr << "Current status query: NOT EXECUTED.\n";
            Print(query...);
            try
            {
                this->action(query...);
            }
            catch (std::exception& exc)
            {
                std::cerr << "CATCHED ERROR\n";
                throw;
            }
            std::cerr << "EXECUTION COMPLETED\n";
        }

        std::function<void(Args...)> Print;

    public:
        LoggerStrategy() = default;

        LoggerStrategy(std::function<void(Args...)> func, bool supported,
                       std::function<void(Args...)> externalProcess) : StrategyBase<Args...>(func, supported),
                                                                       Print(externalProcess)
        {
        }

        ~LoggerStrategy() override = default;

        void SetPrint(std::function<void(Args...)> externalProcess)
        {
            Print = externalProcess;
        }
    };
}
