#include <searchquery/base.hxx>
#include <searchquery/dialect/postgres.hxx>
#include <searchquery/dialect/sqlite.hxx>
#include <iostream>
#include <string>
#include <cassert>
#include <vector>

using namespace searchquery;

struct test_case {
  std::string name;
  std::string query;
  std::string content;
  bool want;
};

static int test_count = 0;
static int pass_count = 0;

static void
run_match_test(const test_case &tc) {
  test_count++;
  
  if (tc.query.empty()) {
    // Empty query should match everything
    if (tc.want) {
      std::cout << "PASS: " << tc.name << std::endl;
      pass_count++;
    } else {
      std::cout << "FAIL: " << tc.name << " - empty query should match" << std::endl;
    }
    return;
  }
  
  std::string err;
  bool got = searchquery::match_expression(tc.content, tc.query, err);
  
  if (!err.empty() && tc.want) {
    std::cout << "FAIL: " << tc.name << " - parse error: " << err << std::endl;
    return;
  }
  
  if (got == tc.want) {
    std::cout << "PASS: " << tc.name << std::endl;
    pass_count++;
  } else {
    std::cout << "FAIL: " << tc.name << " - MatchExpression(\"" << tc.content << "\", \"" << tc.query
              << "\") = " << got << ", want " << tc.want << std::endl;
  }
}

static void
test_match_simple_terms() {
  std::vector<test_case> tests = {
    {"single term match", "hello", "Hello World", true},
    {"single term no match", "goodbye", "Hello World", false},
    {"case insensitive match", "HELLO", "hello world", true},
    {"partial word match", "wor", "Hello World", true},
  };
  
  for (const auto &tc : tests) {
    run_match_test(tc);
  }
}

static void
test_match_implicit_and() {
  std::vector<test_case> tests = {
    {"two terms both present", "hello world", "Hello there World", true},
    {"two terms one missing", "hello mars", "Hello there World", false},
    {"multiple terms all present", "nostr apps", "Best Nostr Apps 2025", true},
    {"three terms all present", "cat dog bird", "I have a cat, a dog, and a bird", true},
    {"three terms one missing", "cat dog fish", "I have a cat and a dog", false},
  };
  
  for (const auto &tc : tests) {
    run_match_test(tc);
  }
}

static void
test_match_phrases() {
  std::vector<test_case> tests = {
    {"exact phrase match", "\"hello world\"", "Say hello world today", true},
    {"phrase not contiguous", "\"hello world\"", "world hello", false},
    {"phrase with case insensitive", "\"Hello World\"", "say hello world today", true},
    {"phrase in middle of content", "\"quick brown\"", "The quick brown fox jumps", true},
    {"phrase words present but not together", "\"brown fox\"", "The brown and red fox", false},
  };
  
  for (const auto &tc : tests) {
    run_match_test(tc);
  }
}

static void
test_match_explicit_and() {
  std::vector<test_case> tests = {
    {"explicit AND both present", "hello AND world", "Hello World", true},
    {"explicit AND first missing", "goodbye AND world", "Hello World", false},
    {"explicit AND second missing", "hello AND mars", "Hello World", false},
  };
  
  for (const auto &tc : tests) {
    run_match_test(tc);
  }
}

static void
test_match_explicit_or() {
  std::vector<test_case> tests = {
    {"explicit OR both present", "hello OR world", "Hello World", true},
    {"explicit OR first present", "hello OR mars", "Hello World", true},
    {"explicit OR second present", "goodbye OR world", "Hello World", true},
    {"explicit OR both missing", "goodbye OR mars", "Hello World", false},
  };
  
  for (const auto &tc : tests) {
    run_match_test(tc);
  }
}

static void
test_match_parentheses() {
  std::vector<test_case> tests = {
    {"simple grouping with implicit AND", "(cat dog)", "I have a cat and a dog", true},
    {"AND with OR in parentheses - match", "cat AND (dog OR bird)", "I have a cat and a bird", true},
    {"AND with OR in parentheses - no match", "cat AND (dog OR bird)", "I have a cat", false},
    {"OR with AND in parentheses", "(cat AND dog) OR bird", "I have a bird", true},
    {"precedence: AND higher than OR", "cat AND dog OR bird AND fish", "I have a bird and a fish", true},
  };
  
  for (const auto &tc : tests) {
    run_match_test(tc);
  }
}

static void
test_match_edge_cases() {
  std::vector<test_case> tests = {
    {"empty query", "", "Hello World", true},
    {"empty content", "hello", "", false},
    {"both empty", "", "", true},
    {"special characters in term", "hello@world", "Contact hello@world.com", true},
  };
  
  for (const auto &tc : tests) {
    run_match_test(tc);
  }
}

static void
test_match_complex_queries() {
  std::vector<test_case> tests = {
    {"phrase with implicit AND", "\"hello world\" test", "This is a hello world test", true},
    {"multiple phrases with implicit AND", "\"hello world\" \"test case\"", "This hello world is a test case", true},
    {"phrase with explicit AND", "\"hello world\" AND test", "This is a hello world test", true},
    {"phrase with explicit OR", "\"hello world\" OR \"goodbye world\"", "Say goodbye world", true},
    {"real-world query with operators", "(golang OR go) AND (tutorial OR guide)", "A beginner's guide to golang programming", true},
  };
  
  for (const auto &tc : tests) {
    run_match_test(tc);
  }
}

struct tsquery_test_case {
  std::string name;
  std::string query;
  std::string want;
};

static void
test_to_tsquery() {
  std::vector<tsquery_test_case> tests = {
    {"single term", "hello", "hello"},
    {"implicit AND", "hello world", "(hello & world)"},
    {"phrase search", "\"hello world\"", "hello <-> world"},
    {"three terms", "cat dog bird", "((cat & dog) & bird)"},
    {"phrase with multiple words", "\"quick brown fox\"", "quick <-> brown <-> fox"},
    {"term with special characters", "hello@world", "'hello@world'"},
    {"empty query", "", ""},
    {"multiple phrases", "\"hello world\" \"test case\"", "(hello <-> world & test <-> case)"},
  };
  
  for (const auto &tc : tests) {
    test_count++;
    std::string got = dialect::postgres::to_tsquery(tc.query);
    if (got == tc.want) {
      std::cout << "PASS: ToTsQuery - " << tc.name << std::endl;
      pass_count++;
    } else {
      std::cout << "FAIL: ToTsQuery - " << tc.name << " - got \"" << got
                << "\", want \"" << tc.want << "\"" << std::endl;
    }
  }
}

static void
test_to_fts5_query() {
  std::vector<tsquery_test_case> tests = {
    {"single term", "hello", "hello"},
    {"implicit AND", "hello world", "hello AND world"},
    {"phrase search", "\"hello world\"", "\"hello world\""},
    {"three terms", "cat dog bird", "cat AND dog AND bird"},
    {"phrase with multiple words", "\"quick brown fox\"", "\"quick brown fox\""},
    {"empty query", "", ""},
    {"multiple phrases", "\"hello world\" \"test case\"", "\"hello world\" AND \"test case\""},
  };
  
  for (const auto &tc : tests) {
    test_count++;
    std::string got = dialect::sqlite::to_fts5_query(tc.query);
    if (got == tc.want) {
      std::cout << "PASS: ToFTS5Query - " << tc.name << std::endl;
      pass_count++;
    } else {
      std::cout << "FAIL: ToFTS5Query - " << tc.name << " - got \"" << got
                << "\", want \"" << tc.want << "\"" << std::endl;
    }
  }
}

int
main() {
  std::cout << "Running tests..." << std::endl << std::endl;
  
  test_match_simple_terms();
  test_match_implicit_and();
  test_match_phrases();
  test_match_explicit_and();
  test_match_explicit_or();
  test_match_parentheses();
  test_match_edge_cases();
  test_match_complex_queries();
  test_to_tsquery();
  test_to_fts5_query();
  
  std::cout << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << "Tests: " << pass_count << "/" << test_count << " passed" << std::endl;
  std::cout << "========================================" << std::endl;
  
  return (pass_count == test_count) ? 0 : 1;
}
