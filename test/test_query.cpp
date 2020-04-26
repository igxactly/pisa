#define CATCH_CONFIG_MAIN

#include <sstream>

#include <catch2/catch.hpp>

#include "query.hpp"

using pisa::QueryContainer;

TEST_CASE("Construct from raw string")
{
    auto raw_query = "brooklyn tea house";
    auto query = QueryContainer::raw(raw_query);
    REQUIRE(*query.string() == raw_query);
}

TEST_CASE("Construct from terms")
{
    std::vector<std::string> terms{"brooklyn", "tea", "house"};
    auto query = QueryContainer::from_terms(terms, std::nullopt);
    REQUIRE(*query.terms() == std::vector<std::string>{"brooklyn", "tea", "house"});
}

TEST_CASE("Construct from terms with processor")
{
    std::vector<std::string> terms{"brooklyn", "tea", "house"};
    auto proc = [](std::string term) -> std::optional<std::string> {
        if (term.size() > 3) {
            return term.substr(0, 4);
        }
        return std::nullopt;
    };
    auto query = QueryContainer::from_terms(terms, proc);
    REQUIRE(*query.terms() == std::vector<std::string>{"broo", "hous"});
}

TEST_CASE("Construct from term IDs")
{
    std::vector<std::uint32_t> term_ids{1, 0, 3};
    auto query = QueryContainer::from_term_ids(term_ids);
    REQUIRE(*query.term_ids() == std::vector<std::uint32_t>{1, 0, 3});
}

TEST_CASE("Parse query")
{
    auto raw_query = "brooklyn tea house brooklyn";
    auto query = QueryContainer::raw(raw_query);
    std::vector<std::string> lexicon{"house", "brooklyn"};
    auto term_proc = [](std::string term) -> std::optional<std::string> { return term; };
    query.parse([&](auto&& q) {
        std::istringstream is(q);
        std::string term;
        std::vector<pisa::ParsedTerm> parsed_terms;
        while (is >> term) {
            if (auto t = term_proc(term); t) {
                if (auto pos = std::find(lexicon.begin(), lexicon.end(), *t); pos != lexicon.end()) {
                    auto id = static_cast<std::uint32_t>(std::distance(lexicon.begin(), pos));
                    parsed_terms.push_back(pisa::ParsedTerm{id, *t});
                }
            }
        }
        return parsed_terms;
    });
    REQUIRE(*query.term_ids() == std::vector<std::uint32_t>{1, 0, 1});
}

TEST_CASE("Parsing throws without raw query")
{
    std::vector<std::uint32_t> term_ids{1, 0, 3};
    auto query = QueryContainer::from_term_ids(term_ids);
    REQUIRE_THROWS_AS(
        query.parse([](auto&& str) { return std::vector<pisa::ParsedTerm>{}; }), std::domain_error);
}

TEST_CASE("Parse query container from colon-delimited format")
{
    auto query = QueryContainer::from_colon_format("");
    REQUIRE(query.string()->empty());
    REQUIRE_FALSE(query.id());

    query = QueryContainer::from_colon_format("brooklyn tea house");
    REQUIRE(*query.string() == "brooklyn tea house");
    REQUIRE_FALSE(query.id());

    query = QueryContainer::from_colon_format("BTH:brooklyn tea house");
    REQUIRE(*query.string() == "brooklyn tea house");
    REQUIRE(*query.id() == "BTH");

    query = QueryContainer::from_colon_format("BTH:");
    REQUIRE(query.string()->empty());
    REQUIRE(*query.id() == "BTH");
}

TEST_CASE("Parse query container from JSON")
{
    REQUIRE_THROWS_AS(QueryContainer::from_json(""), std::runtime_error);
    REQUIRE_THROWS_AS(QueryContainer::from_json(R"({"id":"ID"})"), std::invalid_argument);

    auto query = QueryContainer::from_json(R"(
    {
        "id": "ID",
        "query": "brooklyn tea house"
    }
    )");
    REQUIRE(*query.id() == "ID");
    REQUIRE(*query.string() == "brooklyn tea house");
    REQUIRE_FALSE(query.terms());
    REQUIRE_FALSE(query.term_ids());
    REQUIRE_FALSE(query.threshold());

    query = QueryContainer::from_json(R"(
    {
        "term_ids": [1, 0, 3],
        "terms": ["brooklyn", "tea", "house"],
        "threshold": 10.8
    }
    )");
    REQUIRE(*query.terms() == std::vector<std::string>{"brooklyn", "tea", "house"});
    REQUIRE(*query.term_ids() == std::vector<std::uint32_t>{1, 0, 3});
    REQUIRE(*query.threshold() == Approx(10.8));
    REQUIRE_FALSE(query.id());
    REQUIRE_FALSE(query.string());

    REQUIRE_THROWS_AS(QueryContainer::from_json(R"({"terms":[1, 2]})"), std::runtime_error);
}

TEST_CASE("Serialize query container to JSON")
{
    auto query = QueryContainer::from_json(R"(
    {
        "id": "ID",
        "query": "brooklyn tea house",
        "terms": ["brooklyn", "tea", "house"],
        "term_ids": [1, 0, 3],
        "threshold": 10.0
    }
    )");
    auto serialized = query.to_json();
    REQUIRE(
        serialized
        == R"({"id":"ID","query":"brooklyn tea house","term_ids":[1,0,3],"terms":["brooklyn","tea","house"],"threshold":10.0})");
}
