#include <cstdint>
#include <expected>

#include <gears_outcome.hpp>
enum ErrorType : std::uint8_t {
    ErrorType_None,
    ErrorType_Unknown,
    ErrorType_Invalid,
    ErrorType_Timeout,
    ErrorType_Busy
};

enum ErrorTypeOut : std::uint8_t { OutOfMemory, OutOfResources, OutOfCapacity };
using namespace demiplane::gears;
Outcome<int, ErrorType, ErrorTypeOut> test() {
    return Err(ErrorType_Busy);
}

std::expected<int, int> g() {
    return 1;
}

 int main() {
    test();

}
