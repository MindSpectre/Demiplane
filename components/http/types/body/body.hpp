#pragma once

#include <cstddef>
#include <demiplane/gears>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <async_outcome.hpp>
#include <boost/asio/awaitable.hpp>
#include <errors.hpp>
#include <json/json.h>

namespace demiplane::http {

    struct MultipartField {
        std::string name;
        std::string value;  // payload (small/non-file fields)
        std::string content_type;
        std::string filename;  // empty for non-file fields
    };

    /**
     * @brief Streaming-truth body, held as an SBO value type.
     *
     * Type-erased over a payload (EmptyBody / OwnedBufferBody in PR1;
     * BeastRequestBody in PR3; StreamingProducerBody later). Common bodies live
     * entirely inline — zero heap nodes, no unique_ptr, no dynamic_cast. The
     * driver writes a body by driving read_chunk(), or — for non-streaming
     * bodies — by writing buffered_view() in one shot.
     */
    class Body : gears::NonCopyable {
    public:
        Body() noexcept;  // EmptyBody
        Body(Body&&) noexcept;
        Body& operator=(Body&&) noexcept;
        ~Body();

        static Body empty() noexcept {
            return Body{};
        }
        static Body owned(std::string bytes);  // OwnedBufferBody

        // Streaming primitive. nullopt when exhausted. (Plain function returning
        // the payload's awaitable — no extra coroutine frame here.)
        [[nodiscard]] boost::asio::awaitable<std::optional<std::span<const std::byte>>> read_chunk();

        [[nodiscard]] std::optional<std::size_t> size_hint() const;

        /// Whole-body view for non-streaming bodies (driver fast-path + tests).
        /// nullopt for streaming bodies; "" for EmptyBody.
        [[nodiscard]] std::optional<std::string_view> buffered_view() const;

        // ── Buffered helpers (defs in Task 12) ───────────────────────────
        AsyncOutcome<std::string, BodyLimitExceeded> read_to_string(std::size_t limit);

        AsyncOutcome<Json::Value, JsonParseError, BodyLimitExceeded> read_json(std::size_t limit);

        AsyncOutcome<std::unordered_map<std::string, std::string>, FormParseError, BodyLimitExceeded>
        read_form(std::size_t limit);

        AsyncOutcome<std::vector<MultipartField>, MultipartParseError, BodyLimitExceeded>
        read_multipart(std::size_t limit, std::string_view boundary);

    private:
        static constexpr std::size_t INLINE_SIZE = 48;

        struct VTable {
            boost::asio::awaitable<std::optional<std::span<const std::byte>>> (*read_chunk)(void*);
            std::optional<std::size_t> (*size_hint)(const void*);
            std::optional<std::string_view> (*buffered_view)(const void*);
            void (*move)(void* dst, void* src) noexcept;  // move-construct dst, destroy src
            void (*destroy)(void*) noexcept;
        };

        template <typename T>
        static const VTable* vtable_for() noexcept {
            static const VTable vt{
                +[](void* p) { return static_cast<T*>(p)->read_chunk(); },
                +[](const void* p) { return static_cast<const T*>(p)->size_hint(); },
                +[](const void* p) { return static_cast<const T*>(p)->buffered_view(); },
                +[](void* d, void* s) noexcept {
                    ::new (d) T(std::move(*static_cast<T*>(s)));
                    static_cast<T*>(s)->~T();
                },
                +[](void* p) noexcept { static_cast<T*>(p)->~T(); },
            };
            return &vt;
        }

        struct emplace_t {};
        template <typename T, typename... A>
        explicit Body(emplace_t, std::in_place_type_t<T>, A&&... a)
            : vt_{vtable_for<T>()} {
            static_assert(sizeof(T) <= INLINE_SIZE, "Body payload exceeds SBO budget");
            static_assert(alignof(T) <= alignof(std::max_align_t));
            ::new (storage_) T(std::forward<A>(a)...);
        }

        void* obj() noexcept {
            return storage_;
        }
        [[nodiscard]] const void* obj() const noexcept {
            return storage_;
        }

        alignas(std::max_align_t) std::byte storage_[INLINE_SIZE]{};  // TODO: C++26 attr [indeterminate] apply
        const VTable* vt_;
    };

}  // namespace demiplane::http
