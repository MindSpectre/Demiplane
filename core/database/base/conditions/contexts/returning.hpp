#pragma once

namespace demiplane::database::query {

    template <typename Derived>
    class Returning {
    public:
        Derived& return_with(Columns returning_fields) {
            returning_fields_ = std::move(returning_fields);
            return static_cast<Derived&>(*this);
        }
        [[nodiscard]] const Columns& returning_fields() const {
            return returning_fields_;
        }
        [[nodiscard]] bool has_returning_fields() const {
            return !returning_fields_.empty();
        }
    protected:
        Columns returning_fields_;
    };
} // namespace demiplane::database::query
