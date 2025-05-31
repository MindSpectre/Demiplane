// ──────────────────────────────────────────────────────────────────────────────
//   g++ -std=c++23 -pthread -I/path/to/googletest/include \
//       test_nexus.cpp ../src/*.cpp -lgtest -lgtest_main -o test && ./test
// ──────────────────────────────────────────────────────────────────────────────
#include <atomic>
#include <chrono>
#include <demiplane/nexus>
#include <gtest/gtest.h>
#include <thread>

using namespace demiplane::nexus;

// -----------------------------------------------------------------------------
// Helpers
struct Counter {
    static constexpr uint32_t nx_id = 0xFFF;
    static std::atomic<int> live;
    Counter() {
        std::cerr << "Counter::Counter\n";
        ++live;
    }
    ~Counter() {
        std::cerr << "Counter::~Counter\n";
        --live;
    }
    static int val() {
        return 123;
    }
};
std::atomic<int> Counter::live{0};

struct MoveOnly {
    static constexpr uint32_t nx_id = 0xFFA;
    explicit MoveOnly(int x) : v(x) {}
    MoveOnly(MoveOnly&&) noexcept            = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
    int v;
};

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

TEST(Nexus, FactorySingleton) {
    Nexus nx;
    nx.register_factory<Counter>([](Nexus&) { return std::make_shared<Counter>(); });

    EXPECT_EQ(Counter::live.load(), 0);

    auto a = nx.spawn<Counter>();
    EXPECT_EQ(Counter::live.load(), 1);
    auto b = nx.spawn<Counter>();
    EXPECT_EQ(a.get(), b.get());
    EXPECT_EQ(a->val(), 123);
}

TEST(Nexus, RegisterShared) {
    Nexus nx;
    auto original = std::make_shared<Counter>();
    nx.register_shared<Counter>(original);

    auto s = nx.spawn<Counter>();
    EXPECT_EQ(s.get(), original.get());
}

TEST(Nexus, RegisterInstanceMove) {
    Nexus nx;
    nx.register_instance<MoveOnly>(MoveOnly{77});

    auto m = nx.spawn<MoveOnly>();
    EXPECT_EQ(m->v, 77);
}

TEST(Nexus, ResetFlexOK) {
    Nexus nx;
    nx.register_factory<Counter>([](Nexus&) { return std::make_shared<Counter>(); }, Flex{});

    nx.spawn<Counter>(); // construct
    // EXPECT_NO_THROW(nx.reset<Counter>());
    EXPECT_EQ(Counter::live.load(), 1); // shared_ptr in test keeps alive
    nx.clear();
    EXPECT_EQ(Counter::live.load(), 0);
}

TEST(Nexus, ResetImmortalThrows) {
    Nexus nx;
    nx.register_factory<Counter>([](Nexus&) { return std::make_shared<Counter>(); }, Immortal{});

    nx.spawn<Counter>();
    EXPECT_THROW(nx.reset<Counter>(), std::runtime_error);
}

TEST(Nexus, ScopedGarbageCollected) {
    Nexus nx;
    nx.register_factory<Counter>([](Nexus&) { return std::make_shared<Counter>(); }, Scoped{});

    {
        auto h1 = nx.spawn<Counter>();
        EXPECT_EQ(Counter::live.load(), 1);
    }
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(7s); // > janitor interval
    EXPECT_EQ(Counter::live.load(), 0);
}

TEST(Nexus, TimedEviction) {
    Nexus nx;
    nx.register_factory<Counter>([](Nexus&) { return std::make_shared<Counter>(); }, Timed{std::chrono::seconds{1}});

    nx.spawn<Counter>();
    EXPECT_EQ(Counter::live.load(), 1);

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(2s); // idle > ttl
    std::this_thread::sleep_for(7s); // wait janitor
    EXPECT_EQ(Counter::live.load(), 0);
}

TEST(Nexus, ThreadSafetySingleton) {
    Nexus nx;
    nx.register_factory<Counter>([](Nexus&) { return std::make_shared<Counter>(); });

    std::shared_ptr<Counter> ptrs[16];
    std::thread threads[16];

    for (int i = 0; i < 16; ++i) {
        threads[i] = std::thread([&nx, &ptrs, i] { ptrs[i] = nx.spawn<Counter>(); });
    }
    for (auto& t : threads) {
        t.join();
    }

    for (int i = 1; i < 16; ++i) {
        EXPECT_EQ(ptrs[0].get(), ptrs[i].get());
    }

    EXPECT_EQ(Counter::live.load(), 1);
}
struct With {
    static constexpr uint32_t nx_id = 0xBEEF;
};
struct Without {};
TEST(Nexus, DefaultIdTrait) {

    static_assert(default_id_v<With> == 0xBEEF);
    // static_assert(default_id_v<Without> == 0);
}
