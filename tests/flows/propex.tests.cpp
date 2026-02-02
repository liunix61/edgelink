#include <edgelink/edgelink.hpp>
#include <edgelink/propex.hpp>

using namespace edgelink;

namespace pe = edgelink::propex;

TEST_CASE("Test Property Expression") {

    SECTION("Can parse simple strings Property Expression") {
        std::string pe1 = "name";
        auto result = pe::parse(pe1);
         REQUIRE(result == pe::PropertySegments{"name"});
    }

    SECTION("Can parse complicated Property Expression") {
        std::string pe1 = "test1.hello .world[ 'aaa' ].name_of";
        auto result = pe::parse(pe1);
        REQUIRE(result == pe::PropertySegments{"test1", "hello", "world", "aaa", "name_of"});
    }

    SECTION("Can parse hybird Property Expression") {
        std::string pe1 = " test1[100] . hello['aaa'] [42 ] .world[99][ 'bbb'].name_of [ 100]";
        auto result = pe::parse(pe1);

        auto expected = pe::PropertySegments{
            pe::PropertySegment("test1"), pe::PropertySegment(100U),  pe::PropertySegment("hello"),
            pe::PropertySegment("aaa"),   pe::PropertySegment(42U),   pe::PropertySegment("world"),
            pe::PropertySegment(99U),     pe::PropertySegment("bbb"), pe::PropertySegment("name_of"),
            pe::PropertySegment(100U),
        };
        REQUIRE(result == expected);
    }
}