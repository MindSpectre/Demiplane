#pragma once

namespace common::database {
    class PqxxViewRecord final : public ViewRecord {
    public:
        [[nodiscard]] std::string extract(const std::size_t idx) const& override {
            return std::string{row_[static_cast<int32_t>(idx)].view()};
        }

        [[nodiscard]] std::string_view view(const std::size_t idx) const& override {
            return row_[static_cast<int32_t>(idx)].view();
        }

        void set_row(pqxx::row&& row) {
            row_ = std::move(row);
        }

        [[nodiscard]] std::size_t size() const& override {
            return row_.size();
        }

        [[nodiscard]] std::string name(const std::size_t idx) const& override {
            return row_[static_cast<int32_t>(idx)].name();
        }

        PqxxViewRecord() = default;

    private:
        pqxx::row row_{};
    };
} // namespace common::database
