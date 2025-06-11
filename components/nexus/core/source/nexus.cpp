#include "nexus.hpp"
#include <iostream>

using namespace demiplane::nexus;

Nexus::Nexus() : janitor_([this]{ janitor_loop(); }) {
    std::cerr << "[Nexus] ctor\n"; // Todo: replace with logger
}

Nexus::~Nexus() {
    stop_.store(true, std::memory_order_relaxed);
    std::cerr << "[Nexus] dtor\n"; //Todo: replace with logger
}

void Nexus::clear() {
    boost::unique_lock lk{mtx_};
    map_.clear();
}

// ───────── janitor helpers ─────────

void Nexus::janitor_loop() {
    using namespace std::chrono_literals;
    while (!stop_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(5s);
        sweep();
    }
}

void Nexus::sweep() {
    const auto now = std::chrono::steady_clock::now();
   boost::unique_lock w{mtx_};
    for (auto it = map_.begin(); it != map_.end(); ) {
        const bool erase = std::visit(
            [&]<typename U>(U& tag){
            using Tag = std::decay_t<U>;
            if constexpr (std::is_same_v<Tag, Scoped>)
                return it->second.scoped_refs == 0;
            else if constexpr (std::is_same_v<Tag, Timed>)
                return (now - it->second.last_touch) > tag.idle;
            else
                return false; // Flex / Immortal
        }, it->second.lt);

        if (erase) it = map_.erase(it); else ++it;
    }
}