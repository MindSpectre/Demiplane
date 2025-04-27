#include "nexus.hpp"

#include <pylon.hpp>
namespace demiplane::nexus {
    // в demiplane/nexus/nexus.hpp

    template <class T>
    void Nexus::register_instance(
        std::unique_ptr<T> up, LifePolicy policy, std::uint32_t id, TimedOpts topts, std::function<void()> on_dtor) {}
    template <class T, Access A /* = Access::Alias */>
    auto Nexus::warp(const std::uint32_t id) {
        // 1) Захватываем общий мьютекс для чтения карты сервисов
        std::shared_lock lk(map_mtx_);

        // 2) Ищем слот по (typeid(T), id)
        const detail::Key key{std::type_index(typeid(T)), id};
        const auto& it = map_.find(key);

        // 3) Если не найден — возвращаем пустой pylon
        if (it == map_.end()) {
            return std::nullopt;
            // TODO: Register or what
        }
        // 4) Получаем shared_ptr<T>
        auto sp = std::static_pointer_cast<T>(it->second.object);
        lk.unlock();

        // 5) Возвращаем нужный pylon
        if constexpr (A == Access::Default) {
            return pylon<T>{sp};
        } else if constexpr (A == Access::View) {
            return view_pylon<T>{sp};
        } else if constexpr (A == Access::Unique) {
            return unique_pylon<T>{sp};
        } else if constexpr (A == Access::Safe) {
            return safe_pylon<T>{sp};
        } else if constexpr (A == Access::Copy) {
            return view_pylon<T>{sp};
        } else {
            static_assert(A == Access::Default || A == Access::View, "warp<T,A>: only Alias and View are supported");
        }
    }
    template <class T>
    void Nexus::reset(std::uint32_t id) {}

    Nexus::Nexus() {
        std::cerr << "Nexus ctor" << std::endl;

    }
    Nexus::~Nexus() {
        std::cerr << "Nexus dtor" << std::endl;
    }
    void Nexus::clear() {}
    std::size_t Nexus::size() const {}
} // namespace demiplane::nexus
