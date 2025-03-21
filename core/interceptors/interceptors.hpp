#pragma once
#include <chrono>
#include <iostream>
#include <ostream>
#include <thread>

namespace demiplane {
    inline void fatalize() {
        std::cerr << "Process has been finished due to manually call. " << std::endl;
        std::abort();
    }
    inline void wait(const std::chrono::milliseconds ms) {
        std::this_thread::sleep_for(ms);
    }
    inline void wait(const uint ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds{ms});
    }
    inline int8_t console_wait() {
        std::string str;
        std::cin >> str;
        std::cout << str << std::endl;
        if (str == "exit" || str == "quit" || str == "q" || str == "Q" || str == "drop") {
            std::terminate();
        }
        if (str == "break") {
            return 1;
        }
        return 0;
    }
} // namespace demiplane
