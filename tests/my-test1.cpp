#include <catch2/catch_test_macros.hpp>

// 去 Boost：boost/config.hpp 已移除——BOOST_SYMBOL_VISIBLE 用自研等价宏（导出符号）
#if defined(_WIN32)
#define MY_PLUGIN_API __declspec(dllexport)
#else
#define MY_PLUGIN_API __attribute__((visibility("default")))
#endif

class MY_PLUGIN_API my_plugin_api {
public:
   virtual std::string name() const = 0;
   virtual float calculate(float x, float y) = 0;

   virtual ~my_plugin_api() {}
};


unsigned int Factorial(unsigned int number) {
    //
    return number <= 1 ? number : Factorial(number - 1) * number;
}

TEST_CASE("Factorials are computed", "[factorial]") {
    REQUIRE(Factorial(1) == 1);
    REQUIRE(Factorial(2) == 2);
    REQUIRE(Factorial(3) == 6);
    REQUIRE(Factorial(10) == 3628800);
}
