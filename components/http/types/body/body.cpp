#include "body.hpp"

namespace demiplane::http {

    namespace {
        struct EmptyPayload {
            boost::asio::awaitable<std::optional<std::span<const std::byte>>> read_chunk() {
                co_return std::nullopt;
            }
            std::optional<std::size_t> size_hint() const { return 0; }
            std::optional<std::string_view> buffered_view() const { return std::string_view{}; }
        };

        struct OwnedBufferPayload {
            std::string bytes;
            bool consumed = false;
            boost::asio::awaitable<std::optional<std::span<const std::byte>>> read_chunk() {
                if (consumed || bytes.empty()) { consumed = true; co_return std::nullopt; }
                consumed = true;
                co_return std::span<const std::byte>{
                    reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()};
            }
            std::optional<std::size_t> size_hint() const { return bytes.size(); }
            std::optional<std::string_view> buffered_view() const { return bytes; }
        };
    }

    Body::Body() noexcept : vt_{vtable_for<EmptyPayload>()} {
        ::new (storage_) EmptyPayload{};
    }

    Body Body::owned(std::string bytes) {
        return Body{emplace_t{}, std::in_place_type<OwnedBufferPayload>,
                    OwnedBufferPayload{std::move(bytes), false}};
    }

    Body::Body(Body&& o) noexcept : vt_{o.vt_} {
        vt_->move(storage_, o.storage_);          // move-construct ours, destroy o's payload
        o.vt_ = vtable_for<EmptyPayload>();        // o becomes a valid EmptyBody
        ::new (o.storage_) EmptyPayload{};
    }

    Body& Body::operator=(Body&& o) noexcept {
        if (this == &o) return *this;
        vt_->destroy(storage_);
        vt_ = o.vt_;
        vt_->move(storage_, o.storage_);
        o.vt_ = vtable_for<EmptyPayload>();
        ::new (o.storage_) EmptyPayload{};
        return *this;
    }

    Body::~Body() { vt_->destroy(storage_); }

    boost::asio::awaitable<std::optional<std::span<const std::byte>>> Body::read_chunk() {
        return vt_->read_chunk(obj());
    }
    std::optional<std::size_t> Body::size_hint() const { return vt_->size_hint(obj()); }
    std::optional<std::string_view> Body::buffered_view() const { return vt_->buffered_view(obj()); }

    // Buffered helper definitions land in Task 12.

}  // namespace demiplane::http
