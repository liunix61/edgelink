#include <edgelink/edgelink.hpp>
#include <edgelink/propex.hpp>

using namespace edgelink;

TEST_CASE("Test Msg class") {

    constexpr char MSG_JSON[] = R"(
        {
            "topic": "/test/test1",
            "payload": {
                "hostInfo": {
                    "cpuLoad": 0.9,
                    "memoryUsage": 300,
                    "memoryTotal": 2048
                },
                "version": "0.1.0"
            }
        }
    )";

    SECTION("Test adding edges and sorting") {
        auto MSG_JSON_VALUE = boost::json::parse(MSG_JSON).as_object();
        auto msg1 = Msg(MSG_JSON_VALUE);

        msg1.at_propex("payload.hostInfo.memoryUsage") = 100;
        REQUIRE(msg1.at_propex("payload.hostInfo.memoryUsage") == 100);
    }
}