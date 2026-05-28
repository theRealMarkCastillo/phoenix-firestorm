// fskeywordmatcher_test.cpp
//
// Unit tests for FSKeywordMatcher — the chat keyword-detection logic extracted
// from FSKeywords. Covers the algorithmic bug surface: case sensitivity, whole-
// word vs substring matching, regex-metacharacter escaping, CSV parsing/trimming,
// and the empty-keyword-matches-everything bug fixed during extraction.

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "fskeywordmatcher.h"

// Convenience: cs = case_sensitive, ww = whole_words.
namespace
{
    FSKeywordMatcher make(std::string_view csv, bool cs = false, bool ww = false)
    {
        return FSKeywordMatcher(csv, cs, ww);
    }
}

TEST_CASE("empty keyword list never matches", "[keywords]")
{
    CHECK_FALSE(make("").matches("anything at all"));
    CHECK(make("").empty());
}

TEST_CASE("substring match is case-insensitive by default", "[keywords]")
{
    auto m = make("cat");
    CHECK(m.matches("I have a cat"));
    CHECK(m.matches("I have a CAT"));     // source upper, keyword lower
    CHECK(m.matches("category counts"));  // substring inside a larger word
    CHECK_FALSE(m.matches("I have a fish"));
}

TEST_CASE("uppercase keyword still matches case-insensitively", "[keywords]")
{
    auto m = make("CAT");                 // keyword lowercased at build time
    CHECK(m.matches("a cat sat"));
}

TEST_CASE("case-sensitive mode respects case", "[keywords]")
{
    auto m = make("CAT", /*cs=*/true);
    CHECK(m.matches("a CAT sat"));
    CHECK_FALSE(m.matches("a cat sat"));
}

TEST_CASE("whole-word mode does not match substrings", "[keywords]")
{
    auto m = make("cat", /*cs=*/false, /*ww=*/true);
    CHECK(m.matches("the cat sat"));
    CHECK(m.matches("cat"));              // whole string is the word
    CHECK(m.matches("a cat."));           // word boundary at punctuation
    CHECK_FALSE(m.matches("category"));   // not a whole word
    CHECK_FALSE(m.matches("bobcat"));
}

TEST_CASE("multiple keywords match if any is present", "[keywords]")
{
    auto m = make("cat,dog,bird");
    CHECK(m.size() == 3);
    CHECK(m.matches("I walked the dog"));
    CHECK(m.matches("a little bird"));
    CHECK_FALSE(m.matches("just a hamster"));
}

TEST_CASE("keywords are trimmed of surrounding whitespace", "[keywords]")
{
    auto m = make("  cat ,  dog  ");
    CHECK(m.size() == 2);
    CHECK(m.matches("dog"));
    CHECK(m.matches("cat"));
}

TEST_CASE("regex metacharacters in a whole-word keyword are matched literally", "[keywords]")
{
    // "c.t" must match the literal text "c.t", NOT "cat" (the '.' is escaped).
    auto m = make("c.t", /*cs=*/false, /*ww=*/true);
    CHECK(m.matches("say c.t now"));
    CHECK_FALSE(m.matches("say cat now"));
}

TEST_CASE("a trailing comma does not create an empty keyword that matches everything", "[keywords][bugfix]")
{
    // Regression: "cat," used to yield an empty token; substring find("")==0 made
    // every message a hit. The empty token must be dropped.
    auto subm = make("cat,");
    CHECK(subm.size() == 1);
    CHECK(subm.matches("a cat"));
    CHECK_FALSE(subm.matches("totally unrelated text"));

    auto wwm = make("cat,", /*cs=*/false, /*ww=*/true);
    CHECK(wwm.size() == 1);
    CHECK_FALSE(wwm.matches("totally unrelated text"));
}

TEST_CASE("whitespace-only keywords are dropped", "[keywords][bugfix]")
{
    auto m = make("cat,   ,dog");
    CHECK(m.size() == 2);
    CHECK_FALSE(m.matches("unrelated"));
}

TEST_CASE("no keyword present yields no match in both modes", "[keywords]")
{
    CHECK_FALSE(make("alpha,beta").matches("gamma delta"));
    CHECK_FALSE(make("alpha,beta", false, true).matches("gamma delta"));
}
