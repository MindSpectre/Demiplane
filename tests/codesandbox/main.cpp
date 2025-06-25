
#include <iostream>

class X {
public:
    int a{1};
    int b{2};
    constexpr static std::uint32_t nx_id{3};
};

int main() {
    X x1;
    return 0;
}
