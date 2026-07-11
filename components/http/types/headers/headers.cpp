#include "headers.hpp"

#include <algorithm>
#include <cassert>

namespace demiplane::http {

    namespace {
        constexpr unsigned char lower(const unsigned char c) noexcept {
            return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c + 32) : c;
        }
        bool does_equal(const std::string_view a, const std::string_view b) noexcept {
            if (a.size() != b.size())
                return false;
            for (std::size_t i = 0; i < a.size(); ++i)
                if (lower(static_cast<unsigned char>(a[i])) != lower(static_cast<unsigned char>(b[i])))
                    return false;

            return true;
        }
    }  // namespace

    Headers Headers::owned(const std::pmr::polymorphic_allocator<> alloc) {
        return Headers{OwnedBacking{alloc}};
    }
    Headers Headers::view_of_beast(const BeastFields& fields) {
        return Headers{BeastBacking{&fields}};
    }

    Headers& Headers::operator=(Headers&& other) noexcept {
        if (this != &other) {
            // Adopt the source backing wholesale: emplace destroys ours and
            // move-CONSTRUCTS theirs (vector steals buffer + allocator). See
            // the header note on the pmr POCMA=false move-assign trap.
            std::visit([&]<typename B>(B& b) { backing_.emplace<B>(std::move(b)); }, other.backing_);
        }
        return *this;
    }

    void Headers::promote_to_owned(const std::pmr::polymorphic_allocator<> alloc) {
        if (std::holds_alternative<OwnedBacking>(backing_))
            return;
        const auto& [fields] = std::get<BeastBacking>(backing_);
        OwnedBacking owned{alloc};
        for (const auto& f : *fields) {
            owned.entries.emplace_back(std::pmr::string{std::string_view(f.name_string()), alloc},
                                       std::pmr::string{std::string_view(f.value()), alloc});
        }
        backing_ = std::move(owned);
    }

    Headers::OwnedBacking& Headers::as_owned() {
        assert(std::holds_alternative<OwnedBacking>(backing_) &&
               "mutating a viewing Headers — call promote_to_owned(alloc) first");
        return std::get<OwnedBacking>(backing_);
    }

    std::optional<std::string_view> Headers::get(std::string_view name) const {
        return std::visit(
            [&]<typename B>(const B& b) -> std::optional<std::string_view> {
                if constexpr (std::same_as<B, BeastBacking>) {
                    auto it = b.fields->find(name);
                    if (it == b.fields->end())
                        return std::nullopt;
                    return std::string_view(it->value());
                } else {
                    for (const auto& [n, v] : b.entries)
                        if (does_equal(std::string_view(n), name))
                            return std::string_view(v);
                    return std::nullopt;
                }
            },
            backing_);
    }

    std::string Headers::get_or(const std::string_view name, const std::string_view fallback) const {
        if (const auto v = get(name))
            return std::string{*v};
        return std::string{fallback};
    }

    std::pmr::vector<std::string_view> Headers::get_all(std::string_view name,
                                                        const std::pmr::polymorphic_allocator<> alloc) const {
        std::pmr::vector<std::string_view> out{alloc};
        std::visit(
            [&]<typename B>(const B& b) {
                if constexpr (std::same_as<B, BeastBacking>) {
                    auto range = b.fields->equal_range(name);
                    for (auto it = range.first; it != range.second; ++it)
                        out.emplace_back(it->value());
                } else {
                    for (const auto& [n, v] : b.entries)
                        if (does_equal(std::string_view(n), name))
                            out.emplace_back(v);
                }
            },
            backing_);
        return out;
    }

    bool Headers::contains(const std::string_view name) const {
        return get(name).has_value();
    }

    void Headers::add(const std::string_view name, const std::string_view value) {
        auto& o      = as_owned();
        const auto a = o.entries.get_allocator();
        o.entries.emplace_back(std::pmr::string{name, a}, std::pmr::string{value, a});
    }
    void Headers::set(const std::string_view name, const std::string_view value) {
        auto& o = as_owned();
        std::erase_if(o.entries, [&](const auto& kv) { return does_equal(std::string_view(kv.first), name); });
        const auto a = o.entries.get_allocator();
        o.entries.emplace_back(std::pmr::string{name, a}, std::pmr::string{value, a});
    }
    void Headers::remove(const std::string_view name) {
        auto& o = as_owned();
        std::erase_if(o.entries, [&](const auto& kv) { return does_equal(std::string_view(kv.first), name); });
    }

    std::size_t Headers::size() const {
        return std::visit(
            []<typename B>(const B& b) -> std::size_t {
                if constexpr (std::same_as<B, BeastBacking>)
                    return static_cast<std::size_t>(std::distance(b.fields->begin(), b.fields->end()));
                else
                    return b.entries.size();
            },
            backing_);
    }

    Headers::const_iterator Headers::begin() const {
        const_iterator it;
        it.h_   = this;
        it.idx_ = 0;
        if (auto* bb = std::get_if<BeastBacking>(&backing_))
            it.beast_it_ = bb->fields->begin();
        return it;
    }
    Headers::const_iterator Headers::end() const {
        const_iterator it;
        it.h_   = this;
        it.idx_ = size();
        if (auto* bb = std::get_if<BeastBacking>(&backing_))
            it.beast_it_ = bb->fields->end();
        return it;
    }

    Headers::value_type Headers::const_iterator::operator*() const {
        return std::visit(
            [&]<typename B>(const B& b) -> Headers::value_type {
                if constexpr (std::same_as<B, BeastBacking>) {
                    return {std::string_view(beast_it_->name_string()), std::string_view(beast_it_->value())};
                } else {
                    const auto& [n, v] = b.entries[idx_];
                    return {std::string_view(n), std::string_view(v)};
                }
            },
            h_->backing_);
    }

    Headers::const_iterator& Headers::const_iterator::operator++() {
        ++idx_;
        if (std::holds_alternative<BeastBacking>(h_->backing_))
            ++beast_it_;  // O(1)
        return *this;
    }
    Headers::const_iterator Headers::const_iterator::operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

}  // namespace demiplane::http
