#include <chrono>
#include <iostream>
#include <thread>
#include <demiplane/chrono>

using namespace std::chrono_literals;

int find_prims(int a) {
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

void heavy_work(const demiplane::chrono::cancellation_token& token) {
    for (int i = 0; !token.stop_requested(); ++i) {
        demiplane::chrono::sleep_for(100ms);
        std::cout << "heavy work " << i << std::endl;
    }
}

int main() {
    auto token = demiplane::chrono::cancellation_token{};
    std::cout << "main: " << std::this_thread::get_id() << std::endl;
    demiplane::chrono::Timer t{5000ms};
    std::jthread th{
        [&token]() {
            demiplane::chrono::sleep_for(500ms);
            token.cancel();
            std::cout << "cancel from thread " << std::this_thread::get_id() << std::endl;
        }
    };
    // 1. Polite vanish: callable accepts token
    std::future<void> future_ok = t.execute_polite_vanish(token, &heavy_work);
    //


    future_ok.wait();
    // 2. Violent kill: legacy callable
    auto future_legacy = t.execute_violent_kill(token, &find_prims, 123456789);
    // cancel from outside


    return 0;
}
