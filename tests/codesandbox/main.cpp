
#include <atomic>
#include <demiplane/gears>
#include <iostream>
#include <utility>

class X {
public:
};

int main() {
    std::atomic<int> x = 10;
    std::cout << x.fetch_sub(5, std::memory_order::release);
    std::cout << x << std::endl;
    X x1;
    demiplane::gears::unused_value(std::as_const(x1));
    return 0;
}
