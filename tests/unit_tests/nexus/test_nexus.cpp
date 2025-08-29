#include <atomic>
#include <chrono>
#include <demiplane/nexus>
#include <future>
#include <memory>
#include <random>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using namespace demiplane::nexus;
using namespace std::chrono_literals;

// ═══════════════════════════════════════════════════════════════════════════════
// Test Fixtures & Helpers (Global scope)
// ═══════════════════════════════════════════════════════════════════════════════

struct LifecycleTracker {
    static constexpr uint32_t nexus_id = 0x9001;
    static std::atomic<int> constructed;
    static std::atomic<int> destructed;
    static std::atomic<int> live_count;

    int id;

    explicit LifecycleTracker(const int id = 0)
        : id(id) {
        ++constructed;
        ++live_count;
    }

    ~LifecycleTracker() {
        ++destructed;
        --live_count;
    }

    static void reset_counters() {
        constructed = 0;
        destructed  = 0;
        live_count  = 0;
    }
};

std::atomic<int> LifecycleTracker::constructed{0};
std::atomic<int> LifecycleTracker::destructed{0};
std::atomic<int> LifecycleTracker::live_count{0};

struct Service {
    static constexpr uint32_t nexus_id = 0x1001;
    int value                          = 42;
};

struct ServiceWithDeps {
    static constexpr uint32_t nexus_id = 0x1002;
    std::shared_ptr<Service> dep;

    explicit ServiceWithDeps(std::shared_ptr<Service> s)
        : dep(std::move(s)) {
    }
};

struct ExpensiveService {
    static constexpr uint32_t nexus_id = 0x1003;
    static std::atomic<int> creation_count;

    ExpensiveService() {
        ++creation_count;
        std::this_thread::sleep_for(10ms);  // Simulate expensive construction
    }
};

std::atomic<int> ExpensiveService::creation_count{0};

// Test-specific services with explicit IDs
struct DatabaseService {
    static constexpr uint32_t nexus_id = 0x2001;
    int connections                    = 5;
};

struct LoggerService {
    static constexpr uint32_t nexus_id = 0x2002;
    std::string level                  = "INFO";
};

struct ConfigService {
    static constexpr uint32_t nexus_id = 0x2003;
    int timeout                        = 30;
};

struct Application {
    static constexpr uint32_t nexus_id = 0x2004;
    std::shared_ptr<DatabaseService> db;
    std::shared_ptr<LoggerService> logger;
    std::shared_ptr<ConfigService> config;

    Application(std::shared_ptr<DatabaseService> d, std::shared_ptr<LoggerService> l, std::shared_ptr<ConfigService> c)
        : db(std::move(d)),
          logger(std::move(l)),
          config(std::move(c)) {
    }
};

struct SessionManager {
    static constexpr uint32_t nexus_id = 0x3001;
    std::atomic<int> active_sessions{0};
};

struct RequestHandler {
    static constexpr uint32_t nexus_id = 0x3002;
    std::shared_ptr<SessionManager> session_mgr;

    explicit RequestHandler(const std::shared_ptr<SessionManager>& sm)
        : session_mgr(sm) {
        ++session_mgr->active_sessions;
    }

    ~RequestHandler() {
        --session_mgr->active_sessions;
    }
};

class NexusTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        LifecycleTracker::reset_counters();
        ExpensiveService::creation_count = 0;
        nexus.set_sweep_interval(2s);
    }

    void TearDown() override {
        nexus.clear();
    }

    Nexus nexus;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Basic Registration & Spawning Tests
// ═══════════════════════════════════════════════════════════════════════════════

class BasicOperationsTest : public NexusTestFixture {};

TEST_F(BasicOperationsTest, RegisterFactory_LazyCreation) {
    nexus.register_factory<Service>([] { return std::make_shared<Service>(); });

    EXPECT_EQ(nexus.size(), 1);

    const auto service = nexus.spawn<Service>();
    EXPECT_NE(service, nullptr);
    EXPECT_EQ(service->value, 42);
}

TEST_F(BasicOperationsTest, RegisterFactory_SingletonBehavior) {
    nexus.register_factory<Service>([] { return std::make_shared<Service>(); });

    const auto service1 = nexus.spawn<Service>();
    const auto service2 = nexus.spawn<Service>();

    EXPECT_EQ(service1.get(), service2.get());
    EXPECT_EQ(service1.use_count(), 3);  // nexus + service1 + service2
}

TEST_F(BasicOperationsTest, RegisterShared_PreExistingObject) {
    const auto original = std::make_shared<Service>();
    original->value     = 99;

    nexus.register_shared<Service>(original);
    const auto retrieved = nexus.spawn<Service>();

    EXPECT_EQ(retrieved.get(), original.get());
    EXPECT_EQ(retrieved->value, 99);
}

TEST_F(BasicOperationsTest, RegisterInstance_ValueSemantics) {
    Service instance;
    instance.value = 77;

    nexus.register_instance<Service>(instance);
    const auto retrieved = nexus.spawn<Service>();

    EXPECT_EQ(retrieved->value, 77);
}

TEST_F(BasicOperationsTest, MultipleTypes_IndependentLifecycles) {
    nexus.register_factory<Service>([] { return std::make_shared<Service>(); });
    nexus.register_factory<LifecycleTracker>([] { return std::make_shared<LifecycleTracker>(1); });

    const auto service = nexus.spawn<Service>();
    const auto tracker = nexus.spawn<LifecycleTracker>();

    EXPECT_NE(service, nullptr);
    EXPECT_NE(tracker, nullptr);
    EXPECT_EQ(LifecycleTracker::live_count.load(), 1);
}

TEST_F(BasicOperationsTest, CustomIds_SameTypeMultipleInstances) {
    // Register Logger with default ID (general purpose)
    nexus.register_factory<LoggerService>([] {
        auto logger   = std::make_shared<LoggerService>();
        logger->level = "INFO";
        return logger;
    });

    // Register Logger with specific ID for debug purposes
    constexpr uint32_t DEBUG_LOGGER_ID = 0x1111;
    nexus.register_factory<LoggerService>(
        [] {
            auto logger   = std::make_shared<LoggerService>();
            logger->level = "DEBUG";
            return logger;
        },
        Resettable{},
        DEBUG_LOGGER_ID);

    // Register Logger with another specific ID for error handling
    constexpr uint32_t ERROR_LOGGER_ID = 0x2222;
    nexus.register_factory<LoggerService>(
        [] {
            auto logger   = std::make_shared<LoggerService>();
            logger->level = "ERROR";
            return logger;
        },
        Resettable{},
        ERROR_LOGGER_ID);

    const auto general_logger = nexus.spawn<LoggerService>();  // Uses default nexus_id
    const auto debug_logger   = nexus.spawn<LoggerService>(DEBUG_LOGGER_ID);
    const auto error_logger   = nexus.spawn<LoggerService>(ERROR_LOGGER_ID);

    EXPECT_NE(general_logger.get(), debug_logger.get());
    EXPECT_NE(general_logger.get(), error_logger.get());
    EXPECT_NE(debug_logger.get(), error_logger.get());

    EXPECT_EQ(general_logger->level, "INFO");
    EXPECT_EQ(debug_logger->level, "DEBUG");
    EXPECT_EQ(error_logger->level, "ERROR");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Dependency Injection Tests
// ═══════════════════════════════════════════════════════════════════════════════

class DependencyInjectionTest : public NexusTestFixture {};

TEST_F(DependencyInjectionTest, SimpleDependency_AutoResolution) {
    nexus.register_factory<Service>([] { return std::make_shared<Service>(); });

    nexus.register_factory<ServiceWithDeps>(
        [this] { return std::make_shared<ServiceWithDeps>(nexus.spawn<Service>()); });

    const auto service_with_deps = nexus.spawn<ServiceWithDeps>();
    EXPECT_NE(service_with_deps, nullptr);
    EXPECT_NE(service_with_deps->dep, nullptr);
    EXPECT_EQ(service_with_deps->dep->value, 42);
}

TEST_F(DependencyInjectionTest, SharedDependency_SameInstance) {
    nexus.register_factory<Service>([] { return std::make_shared<Service>(); });

    nexus.register_factory<ServiceWithDeps>(
        [this] { return std::make_shared<ServiceWithDeps>(nexus.spawn<Service>()); });

    const auto service1       = nexus.spawn<ServiceWithDeps>();
    const auto service2       = nexus.spawn<ServiceWithDeps>();
    const auto direct_service = nexus.spawn<Service>();

    EXPECT_EQ(service1->dep.get(), service2->dep.get());
    EXPECT_EQ(service1->dep.get(), direct_service.get());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Lifetime Policy Tests
// ═══════════════════════════════════════════════════════════════════════════════

class LifetimePolicyTest : public NexusTestFixture {};

TEST_F(LifetimePolicyTest, Resettable_ResetBehavior) {
    nexus.register_factory<LifecycleTracker>([] { return std::make_shared<LifecycleTracker>(1); }, Resettable{});

    {
        auto tracker = nexus.spawn<LifecycleTracker>();
        EXPECT_EQ(LifecycleTracker::live_count.load(), 1);
    }

    EXPECT_NO_THROW(nexus.reset<LifecycleTracker>());
    EXPECT_EQ(LifecycleTracker::live_count.load(), 0);
}

TEST_F(LifetimePolicyTest, Immortal_NoReset) {
    nexus.register_factory<LifecycleTracker>([] { return std::make_shared<LifecycleTracker>(2); }, Immortal{});

    auto tracker = nexus.spawn<LifecycleTracker>();
    EXPECT_EQ(LifecycleTracker::live_count.load(), 1);

    EXPECT_THROW(nexus.reset<LifecycleTracker>(), std::runtime_error);
}

TEST_F(LifetimePolicyTest, Scoped_AutoCleanup) {
    nexus.register_factory<LifecycleTracker>([] { return std::make_shared<LifecycleTracker>(3); }, Scoped{});

    {
        auto tracker = nexus.spawn<LifecycleTracker>();
        EXPECT_EQ(LifecycleTracker::live_count.load(), 1);
    }

    // Wait for janitor to clean up
    std::this_thread::sleep_for(7s);
    EXPECT_EQ(LifecycleTracker::live_count.load(), 0);
}

TEST_F(LifetimePolicyTest, Timed_ExpirationBehavior) {
    nexus.register_factory<LifecycleTracker>([] { return std::make_shared<LifecycleTracker>(4); }, Timed{1s});
    {
        auto tracker = nexus.spawn<LifecycleTracker>();
        EXPECT_EQ(LifecycleTracker::live_count.load(), 1);
    }
    // Wait for expiration + janitor sweep
    std::this_thread::sleep_for(5s);
    EXPECT_EQ(LifecycleTracker::live_count.load(), 0);
}

TEST_F(LifetimePolicyTest, Timed_AccessRenewsLease) {
    nexus.register_factory<LifecycleTracker>([] { return std::make_shared<LifecycleTracker>(5); }, Timed{2s});

    auto tracker = nexus.spawn<LifecycleTracker>();

    // Access repeatedly to renew lease
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(1500ms);
        nexus.spawn<LifecycleTracker>();  // Renews lease
        EXPECT_EQ(LifecycleTracker::live_count.load(), 1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Thread Safety Tests
// ═══════════════════════════════════════════════════════════════════════════════

class ThreadSafetyTest : public NexusTestFixture {};

TEST_F(ThreadSafetyTest, ConcurrentSpawn_SingletonConsistency) {
    nexus.register_factory<ExpensiveService>([] { return std::make_shared<ExpensiveService>(); });

    constexpr int num_threads = 16;
    std::vector<std::future<std::shared_ptr<ExpensiveService>>> futures;

    futures.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        futures.emplace_back(std::async(std::launch::async, [this] { return nexus.spawn<ExpensiveService>(); }));
    }

    std::vector<std::shared_ptr<ExpensiveService>> results;
    results.reserve(futures.size());
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    // All should point to same instance
    for (std::size_t i = 1; i < num_threads; ++i) {
        EXPECT_EQ(results[0].get(), results[i].get());
    }

    // Should only be created once despite concurrent access
    EXPECT_EQ(ExpensiveService::creation_count.load(), 1);
}

TEST_F(ThreadSafetyTest, ConcurrentRegistration_ThreadSafe) {
    constexpr int num_threads = 8;
    std::vector<std::thread> threads;

    threads.reserve(num_threads);
    for (std::uint32_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i] {
            // Register with custom IDs to avoid conflicts
            constexpr uint32_t BASE_ID = 0x4000;
            nexus.register_factory<LifecycleTracker>(
                [i] { return std::make_shared<LifecycleTracker>(i); }, Resettable{}, BASE_ID + i);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(nexus.size(), num_threads);

    // Verify all registrations work
    for (std::uint32_t i = 0; i < num_threads; ++i) {
        constexpr uint32_t BASE_ID = 0x4000;
        const auto tracker         = nexus.spawn<LifecycleTracker>(BASE_ID + i);
        EXPECT_EQ(tracker->id, i);
    }
}

TEST_F(ThreadSafetyTest, MixedOperations_StressTest) {
    // Register some base services
    nexus.register_factory<Service>([] { return std::make_shared<Service>(); });
    nexus.register_factory<LifecycleTracker>([] { return std::make_shared<LifecycleTracker>(0); });

    constexpr int num_threads           = 10;
    constexpr int operations_per_thread = 100;
    constexpr uint32_t STRESS_BASE_ID   = 0x5000;
    std::vector<std::thread> threads;
    std::atomic errors{0};

    threads.reserve(num_threads);
    for (std::uint32_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &errors, t] {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution op_dist(0, 3);

            for (std::uint32_t op = 0; op < operations_per_thread; ++op) {
                try {
                    switch (op_dist(gen)) {
                        case 0:  // Spawn Service
                            nexus.spawn<Service>();
                            break;
                        case 1:  // Spawn LifecycleTracker
                            nexus.spawn<LifecycleTracker>();
                            break;
                        case 2:  // Register new service with unique ID
                            nexus.register_factory<LifecycleTracker>(
                                [t, op] { return std::make_shared<LifecycleTracker>(t * 1000 + op); },
                                Resettable{},
                                STRESS_BASE_ID + t * 1000 + op);
                            break;
                        case 3:  // Get size
                            std::cout << nexus.size();
                            break;
                        default:
                            std::unreachable();
                    }
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Error Handling Tests
// ═══════════════════════════════════════════════════════════════════════════════

class ErrorHandlingTest : public NexusTestFixture {};

TEST_F(ErrorHandlingTest, SpawnUnregistered_ThrowsException) {
    EXPECT_THROW(nexus.spawn<Service>(), std::runtime_error);
}

TEST_F(ErrorHandlingTest, ResetUnregistered_ThrowsException) {
    EXPECT_THROW(nexus.reset<Service>(), std::runtime_error);
}

TEST_F(ErrorHandlingTest, ResetWrongLifetime_ThrowsException) {
    nexus.register_factory<Service>([] { return std::make_shared<Service>(); }, Immortal{});
    nexus.spawn<Service>();

    EXPECT_THROW(nexus.reset<Service>(), std::runtime_error);
}

TEST_F(ErrorHandlingTest, FactoryException_Propagated) {
    nexus.register_factory<Service>([]() -> std::shared_ptr<Service> { throw std::runtime_error("Factory failed"); });

    EXPECT_THROW(nexus.spawn<Service>(), std::runtime_error);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Performance Tests
// ═══════════════════════════════════════════════════════════════════════════════

class PerformanceTest : public NexusTestFixture {};

TEST_F(PerformanceTest, FastPath_CachedObjects) {
    nexus.register_factory<Service>([] {
        std::this_thread::sleep_for(20ns);
        return std::make_shared<Service>();
    });

    // First spawn creates the object
    auto start                  = std::chrono::high_resolution_clock::now();
    auto service1               = nexus.spawn<Service>();
    const auto first_spawn_time = std::chrono::high_resolution_clock::now() - start;

    // Subsequent spawns should be much faster (cached)
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        auto service = nexus.spawn<Service>();
    }
    const auto cached_spawns_time = std::chrono::high_resolution_clock::now() - start;

    // Cached access should be very fast
    EXPECT_LT(cached_spawns_time, first_spawn_time * 100);
}

TEST_F(PerformanceTest, ScalabilityTest_ManyTypes) {
    constexpr int num_types         = 1000;
    constexpr uint32_t PERF_BASE_ID = 0x6000;

    // Register many types with unique IDs
    auto start = std::chrono::high_resolution_clock::now();
    for (std::uint32_t i = 0; i < num_types; ++i) {
        nexus.register_factory<LifecycleTracker>(
            [i] { return std::make_shared<LifecycleTracker>(i); }, Resettable{}, PERF_BASE_ID + i);
    }
    const auto registration_time = std::chrono::high_resolution_clock::now() - start;

    // Spawn all types
    start = std::chrono::high_resolution_clock::now();
    for (std::uint32_t i = 0; i < num_types; ++i) {
        auto tracker = nexus.spawn<LifecycleTracker>(PERF_BASE_ID + i);
    }
    const auto spawn_time = std::chrono::high_resolution_clock::now() - start;

    EXPECT_EQ(nexus.size(), num_types);
    EXPECT_LT(registration_time, 1s);
    EXPECT_LT(spawn_time, 1s);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Integration Tests
// ═══════════════════════════════════════════════════════════════════════════════

class IntegrationTest : public NexusTestFixture {};

TEST_F(IntegrationTest, ComplexDependencyGraph) {
    // Register dependencies using default IDs
    nexus.register_factory<DatabaseService>([] { return std::make_shared<DatabaseService>(); });
    nexus.register_factory<LoggerService>([] { return std::make_shared<LoggerService>(); });
    nexus.register_factory<ConfigService>([] { return std::make_shared<ConfigService>(); });

    nexus.register_factory<Application>([this] {
        return std::make_shared<Application>(
            nexus.spawn<DatabaseService>(), nexus.spawn<LoggerService>(), nexus.spawn<ConfigService>());
    });

    // Test the complete dependency graph
    const auto app = nexus.spawn<Application>();

    EXPECT_NE(app, nullptr);
    EXPECT_NE(app->db, nullptr);
    EXPECT_NE(app->logger, nullptr);
    EXPECT_NE(app->config, nullptr);
    EXPECT_EQ(app->db->connections, 5);
    EXPECT_EQ(app->logger->level, "INFO");
    EXPECT_EQ(app->config->timeout, 30);

    // Verify shared dependencies
    const auto direct_logger = nexus.spawn<LoggerService>();
    EXPECT_EQ(app->logger.get(), direct_logger.get());  // Same instance
}

struct SessionManager2 {
    static constexpr uint32_t nexus_id = 0x2001;
    std::atomic<int> active_sessions{0};
};

struct RequestHandler2 {
    static constexpr uint32_t nexus_id = 0x2002;
    std::shared_ptr<SessionManager2> session_mgr;

    explicit RequestHandler2(const std::shared_ptr<SessionManager2>& sm)
        : session_mgr(sm) {
        ++session_mgr->active_sessions;
    }

    ~RequestHandler2() {
        --session_mgr->active_sessions;
    }
};

TEST_F(IntegrationTest, LifecycleManagement_RealWorldScenario) {
    // Session manager is immortal, request handlers are scoped
    nexus.register_factory<SessionManager2>([] { return std::make_shared<SessionManager2>(); }, Immortal{});
    nexus.register_factory<RequestHandler2>(
        [this] { return std::make_shared<RequestHandler2>(nexus.spawn<SessionManager2>()); }, Scoped{});

    const auto session_mgr = nexus.spawn<SessionManager2>();
    EXPECT_EQ(session_mgr->active_sessions.load(), 0);

    // Simulate request handling
    {
        auto handler1 = nexus.spawn<RequestHandler2>();
        auto handler2 = nexus.spawn<RequestHandler2>();
        EXPECT_EQ(session_mgr->active_sessions.load(), 1);
    }

    // Wait for scoped cleanup
    std::this_thread::sleep_for(7s);
    EXPECT_EQ(session_mgr->active_sessions.load(), 0);
}
