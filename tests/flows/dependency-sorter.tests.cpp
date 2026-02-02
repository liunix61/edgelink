#include <edgelink/edgelink.hpp>
#include <edgelink/flows/dependency-sorter.hpp>

TEST_CASE("Test DependencySorter") {
    edgelink::DependencySorter<int> sorter;

    SECTION("Test adding edges and sorting") {
        sorter.add_edge(1, 2);
        sorter.add_edge(2, 3);
        sorter.add_edge(3, 4);

        std::vector<int> result = sorter.sort();

        REQUIRE(result == std::vector<int>{4, 3, 2, 1});
    }

    SECTION("Test clearing the adjacency list") {
        sorter.add_edge(1, 2);
        sorter.clear();

        REQUIRE(sorter.sort() == std::vector<int>{});
    }

    SECTION("Test sorting when there are no dependencies") {
        sorter.add_edge(1, 2);
        sorter.add_edge(3, 4);

        std::vector<int> result = sorter.sort();

        REQUIRE(result == std::vector<int>{4, 2, 3, 1});
    }

    SECTION("Test sorting with a single node") {
        sorter.add_edge(1, 1); // Self-loop
        sorter.add_edge(2, 3);

        std::vector<int> result = sorter.sort();

        REQUIRE(result == std::vector<int>{3, 2});
    }

    SECTION("Test sorting with cyclic dependencies") {
        sorter.add_edge(1, 2);
        sorter.add_edge(2, 3);
        sorter.add_edge(3, 1); // Create a cycle

        std::vector<int> result = sorter.sort();

        REQUIRE(result.empty());
    }

    SECTION("Test clearing the adjacency list") {
        sorter.add_edge(1, 2);
        sorter.clear();

        REQUIRE(sorter.sort() == std::vector<int>{});
    }
}
