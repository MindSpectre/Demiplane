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
#include <sstream>

#include <gears_outcome.hpp>
#include "../url_decode/url_decode.hpp"

namespace demiplane::http {

    namespace {
        boost::asio::awaitable<gears::Outcome<std::string, BodyLimitExceeded>>
        drain(Body& body, std::size_t limit) {
            std::string out;
            while (true) {
                auto chunk = co_await body.read_chunk();
                if (!chunk) break;
                if (out.size() + chunk->size() > limit) co_return gears::err(BodyLimitExceeded{limit});
                out.append(reinterpret_cast<const char*>(chunk->data()), chunk->size());
            }
            co_return out;
        }
    }

    AsyncOutcome<std::string, BodyLimitExceeded> Body::read_to_string(std::size_t limit) {
        co_return co_await drain(*this, limit);
    }

    AsyncOutcome<Json::Value, JsonParseError, BodyLimitExceeded> Body::read_json(std::size_t limit) {
        auto d = co_await drain(*this, limit);
        if (!d.is_success()) co_return gears::err(d.error<BodyLimitExceeded>());
        Json::Value root; std::string err;
        Json::CharReaderBuilder builder;
        std::istringstream stream{std::move(d).value()};
        if (!Json::parseFromStream(builder, stream, &root, &err))
            co_return gears::err(JsonParseError{std::move(err)});
        co_return root;
    }

    AsyncOutcome<std::unordered_map<std::string, std::string>, FormParseError, BodyLimitExceeded>
    Body::read_form(std::size_t limit) {
        auto d = co_await drain(*this, limit);
        if (!d.is_success()) co_return gears::err(d.error<BodyLimitExceeded>());
        const std::string body = std::move(d).value();
        std::unordered_map<std::string, std::string> out;
        std::size_t i = 0;
        while (i < body.size()) {
            std::size_t amp = body.find('&', i);
            std::string_view pair{body.data() + i, (amp == std::string::npos ? body.size() - i : amp - i)};
            std::size_t eq = pair.find('=');
            std::string_view rk = (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
            std::string_view rv = (eq == std::string_view::npos) ? std::string_view{} : pair.substr(eq + 1);
            if (rk.empty()) co_return gears::err(FormParseError{"empty key"});
            auto k = url_decode(rk); auto v = url_decode(rv);
            if (!k || !v) co_return gears::err(FormParseError{"invalid percent-escape"});
            out[*std::move(k)] = *std::move(v);
            if (amp == std::string::npos) break;
            i = amp + 1;
        }
        co_return out;
    }

    AsyncOutcome<std::vector<MultipartField>, MultipartParseError, BodyLimitExceeded>
    Body::read_multipart(std::size_t limit, std::string_view boundary) {
        if (boundary.empty()) co_return gears::err(MultipartParseError{"empty boundary"});
        auto d = co_await drain(*this, limit);
        if (!d.is_success()) co_return gears::err(d.error<BodyLimitExceeded>());
        const std::string body = std::move(d).value();
        const std::string delim = "--" + std::string{boundary};
        std::vector<MultipartField> out;

        std::size_t pos = body.find(delim);
        if (pos == std::string::npos) co_return gears::err(MultipartParseError{"no boundary found"});
        pos += delim.size();
        while (pos < body.size()) {
            if (pos + 2 <= body.size() && body[pos] == '-' && body[pos + 1] == '-') break;  // end
            if (pos + 1 < body.size() && body[pos] == '\r' && body[pos + 1] == '\n') pos += 2;
            std::size_t header_end = body.find("\r\n\r\n", pos);
            if (header_end == std::string::npos) co_return gears::err(MultipartParseError{"unterminated headers"});
            std::string_view header_block{body.data() + pos, header_end - pos};
            std::size_t body_start = header_end + 4;
            std::size_t next = body.find("\r\n" + delim, body_start);
            if (next == std::string::npos) co_return gears::err(MultipartParseError{"unterminated part"});
            std::string_view part_body{body.data() + body_start, next - body_start};

            MultipartField field;
            std::size_t hp = 0;
            while (hp < header_block.size()) {
                std::size_t eol = header_block.find("\r\n", hp);
                std::string_view line{header_block.data() + hp,
                                      (eol == std::string_view::npos ? header_block.size() - hp : eol - hp)};
                auto ci_starts = [](std::string_view l, std::string_view p) {
                    if (l.size() < p.size()) return false;
                    for (std::size_t k = 0; k < p.size(); ++k) {
                        char a = l[k], b = p[k];
                        if (a >= 'A' && a <= 'Z') a += 32;
                        if (b >= 'A' && b <= 'Z') b += 32;
                        if (a != b) return false;
                    }
                    return true;
                };
                if (ci_starts(line, "content-disposition:")) {
                    auto param = [&](std::string_view key) -> std::string {
                        auto p = line.find(key);
                        if (p == std::string_view::npos) return {};
                        p += key.size();
                        if (p < line.size() && line[p] == '=') ++p;
                        if (p < line.size() && line[p] == '"') {
                            ++p; auto e = line.find('"', p);
                            if (e == std::string_view::npos) return {};
                            return std::string{line.substr(p, e - p)};
                        }
                        return {};
                    };
                    field.name = param("name");
                    field.filename = param("filename");
                } else if (ci_starts(line, "content-type:")) {
                    auto colon = line.find(':');
                    auto v = line.substr(colon + 1);
                    while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.remove_prefix(1);
                    field.content_type = std::string{v};
                }
                if (eol == std::string_view::npos) break;
                hp = eol + 2;
            }
            field.value = std::string{part_body};
            out.emplace_back(std::move(field));
            pos = next + 2 + delim.size();
        }
        co_return out;
    }

}  // namespace demiplane::http
