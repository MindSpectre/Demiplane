#include <memory>
#include <cassert>

// A simple structure to manage
struct MyData {
    int value;
    constexpr MyData(int v) : value(v) {}
};

// A constexpr function that returns a shared_ptr
constexpr std::shared_ptr<MyData> create_constexpr_ptr() {
    // In C++23, dynamic allocation is possible within a constexpr context
    return std::make_shared<MyData>(42);
}

// Declare a static constexpr shared_ptr
// This will be initialized at compile time
static constexpr std::shared_ptr<MyData> static_shared_ptr = create_constexpr_ptr();

int main() {
    // The value can be used in a static_assert
    static_assert(static_shared_ptr->value == 42, "Value should be 42");

    // The shared_ptr can also be used at runtime
    assert(static_shared_ptr->value == 42);

    return 0;
}
