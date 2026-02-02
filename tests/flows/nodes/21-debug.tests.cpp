#include <edgelink/edgelink.hpp>

/*
TEST_CASE("debug node", "[debug][load]") {

    auto json_text = R"(
    [
        {
            "id": "flow1",
            "type": "tab",
            "label": "Flow1"
        },
        {
            "id": "n1",
            "type": "debug",
            "name": "Debug",
            "complete": "false"
        }
    ]
    )";
    auto json = boost::json::parse(json_text).as_array();

    auto flow_type = rttr::type::get("edgelink::details::Flow");
    // auto flow = flow_type.create(json.at(0).as_object(), nullptr);
    auto debug_node_provider = rttr::type::get("edgelink::DebugNodeProvider");

        var flow = [ {id : "n1", type : "debug", name : "Debug", complete : "false"} ];
        helper.load(
            debugNode, flow, function() {
                var n1 = helper.getNode("n1");
                n1.should.have.property('name', 'Debug');
                n1.should.have.property('complete', "payload");
                done();
            });

        REQUIRE(factorial(1) == 1);
        REQUIRE(factorial(2) == 2);
        REQUIRE(factorial(3) == 6);
        REQUIRE(factorial(10) == 3'628'800);
}
*/