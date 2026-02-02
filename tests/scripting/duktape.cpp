#include <rva/variant.hpp>

using json_value = rva::variant<                        //
    std::nullptr_t,                                     // json null
    bool,                                               // json boolean
    double,                                             // json number
    const std::string,                                  // json string
    std::unordered_map<const std::string, rva::self_t>, // json object, type is std::map<std::string, json_value>
    std::vector<rva::self_t>                            // json array, type is std::vector<json_value>
    >;

/*
TEST_CASE("Factorials are computed", "[factorial]") {
    REQUIRE(Factorial(1) == 1);
    REQUIRE(Factorial(2) == 2);
    REQUIRE(Factorial(3) == 6);
    REQUIRE(Factorial(10) == 3628800);
}

*/