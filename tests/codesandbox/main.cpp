#include <chrono>
#include <demiplane/chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int find_prims(const int a) {
    std::cout << "executor: " << std::this_thread::get_id() << std::endl;
    demiplane::chrono::sleep_for(100ms);
    std::cout << "find prims " << a << std::endl;
    demiplane::chrono::sleep_for(200ms);
    std::cout << "find prims " << a << std::endl;
    demiplane::chrono::sleep_for(500ms);
    std::cout << "find prims " << a << std::endl;
    demiplane::chrono::sleep_for(1000ms);
    std::cout << "find prims " << a << std::endl;
    return a;
}

void heavy_work(const std::shared_ptr<demiplane::chrono::CancellationToken>& token) {
    for (int i = 0; !token->stop_requested(); ++i) {
        demiplane::chrono::sleep_for(10ms);
        std::cout << "heavy work " << i << std::endl;
    }
}

int main() {
    auto token = std::make_shared<demiplane::chrono::CancellationToken>();
    constexpr demiplane::multithread::ThreadPoolConfig cfg;
    std::cout << "main: " << std::this_thread::get_id() << std::endl;
    demiplane::chrono::Timer t(cfg);
    std::jthread th{[&token] {
        demiplane::chrono::sleep_for(100ms);
        token->cancel();
        std::cout << "cancel from thread1 " << std::this_thread::get_id() << std::endl;
    }};
    // 1. Polite vanish: callable accepts token
    const std::future<void> future_ok = t.execute_polite_vanish(50ms, &heavy_work, token);
    //


    future_ok.wait();
    token->renew();
    std::jthread th2{[&token] {
        demiplane::chrono::sleep_for(900ms);
        token->cancel();
        std::cout << "cancel from thread2 " << std::this_thread::get_id() << std::endl;
    }};
    // 2. Violent kill: legacy callable
    const auto future_legacy = t.execute_violent_kill(3000ms, token, &find_prims, 123456789);
    // cancel from outside
    future_legacy.wait();

    return 0;
}
