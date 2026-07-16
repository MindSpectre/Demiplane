#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <controller.hpp>
#include <route_registry.hpp>

namespace demiplane::http {

    /**
     * @brief Prefix-scoped controller mounting (spec §8.4).
     *
     * In PR 5 this is what Server::in_group(prefix) returns, constructed over
     * the Server's registry + controller list; until then callers construct it
     * directly. add_controller() runs the bake step: configure_routes() once,
     * prefix concat, middleware composition, Outcome→Response wiring, conflict
     * recording — and records the controller in the caller-owned sink (the
     * Server keeps them alive for the routes' lifetime; controllers are RAII).
     */
    class GroupBinding {
    public:
        template <gears::IsStringLike StringTp>
        GroupBinding(RouteRegistry& registry,
                     std::vector<std::shared_ptr<HttpController>>& controller_sink,
                     StringTp&& prefix) GEARS_UNRECOVERABLE_NOEXCEPT : registry_{&registry},
                                                                       controllers_{&controller_sink},
                                                                       prefix_{std::forward<StringTp>(prefix)} {
        }

        /// Bakes + merges the controller's routes under this group's prefix.
        /// Throws std::logic_error if the controller was already baked.
        template <std::derived_from<HttpController> C>
        GroupBinding& add_controller(std::shared_ptr<C> ctrl) {
            std::shared_ptr<HttpController> base = std::move(ctrl);
            detail::ControllerBaker::bake_into(*registry_, base, prefix_);
            controllers_->push_back(std::move(base));
            return *this;
        }

        /// Nested group via combined prefix.
        [[nodiscard]] GroupBinding in_group(const std::string_view sub_prefix) const {
            return GroupBinding{*registry_, *controllers_, join_path(prefix_, sub_prefix)};
        }

        [[nodiscard]] const std::string& prefix() const noexcept {
            return prefix_;
        }

    private:
        // non-null: bound from ctor lvalue refs, never reseated
        RouteRegistry* registry_;
        std::vector<std::shared_ptr<HttpController>>* controllers_;
        std::string prefix_;
    };

}  // namespace demiplane::http
