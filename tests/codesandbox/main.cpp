#include <cstdint>
#include <expected>
#include <iostream>

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
Outcome<void, ErrorType, ErrorTypeOut> test() {
    Outcome<void, ErrorType, ErrorTypeOut> out;
    return Ok();
}
Outcome<int, ErrorType, ErrorTypeOut> test2() {
    return Ok(2);
    return Err(ErrorType_Busy);
}

std::expected<int, int> g() {
    return 1;
}

 int main() {
    auto res = test2();
    std::cout << res.holds_error<ErrorType>() << std::endl;
    std::cout << res.holds_error<ErrorTypeOut>() << std::endl;
    std::cout << res.error<ErrorType>() << std::endl;

}
