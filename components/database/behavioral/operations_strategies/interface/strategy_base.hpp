#pragma once
#include <functional>
#include <iostream>

namespace demiplane::database::behavioral::strategies
{
    template <typename... Args>
    class StrategyBase
    {
    protected:
        bool enabled = false;
        bool supported = false;
        std::function<void(Args...)> action;

        virtual void internal_logic(Args... v)
        {
            std::cerr << "DOESNT DECLARED IN THIS SCOPE" << std::endl;
        };

    public:
        virtual ~StrategyBase() = default;

        explicit StrategyBase(std::function<void(Args...)> func, const bool support = false) : supported(support),
            action(func)
        {
        }

        StrategyBase() = default;

        void set_action(std::function<void(Args...)> action)
        {
            this->action = action;
        }

        void set_support(bool dbStrategySupport) { supported = dbStrategySupport; }
        [[nodiscard]] bool is_supported() const { return supported; }
        void enable() { enabled = true; }
        void disable() { enabled = false; }
        [[nodiscard]] bool is_enabled() const { return enabled; }

        void execute(Args... args)
        {
            if (enabled && !supported)
            {
                throw std::runtime_error("Unavailable strategy");
            }
            if (enabled && action)
            {
                InternalLogic(args...);
            }
        }
    };
}
