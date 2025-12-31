#ifndef SEARCHQUERY_DIALECT_SQLITE_HXX
#define SEARCHQUERY_DIALECT_SQLITE_HXX

#include <searchquery/base.hxx>

namespace searchquery {
namespace dialect {
namespace sqlite {

static std::string escape_fts5_term(std::string term);
static std::string node_to_fts5_query(const node_t& node);

static std::string escape_fts5_term(std::string term) {
  // Remove quotes if present
  if (!term.empty() && term.front() == '"' && term.back() == '"') {
    term = term.substr(1, term.size() - 2);
  }
  
  // FTS5 special characters that need quoting: " *
  bool needs_quoting = false;
  for (char c : term) {
    if (c == '"' || c == '*') {
      needs_quoting = true;
      break;
    }
  }
  
  if (needs_quoting) {
    // Escape quotes by doubling them
    std::string escaped;
    for (char c : term) {
      if (c == '"') {
        escaped += "\"\"";
      } else {
        escaped += c;
      }
    }
    return "\"" + escaped + "\"";
  }
  return term;
}

static inline std::string node_to_fts5_query(const node_t& node) {
  switch (node.type) {
    case NODE_TERM: {
      // Check if it's a phrase (contains spaces)
      if (node.phrase.find(' ') != std::string::npos) {
        // Phrase: keep quotes
        std::string phrase = node.phrase;
        if (!phrase.empty() && phrase.front() == '"' && phrase.back() == '"') {
          phrase = phrase.substr(1, phrase.size() - 2);
        }
        return "\"" + phrase + "\"";
      }
      // Single term - escape if needed
      return escape_fts5_term(node.phrase);
    }
    case NODE_AND: {
      std::string left = node_to_fts5_query(*node.left);
      std::string right = node_to_fts5_query(*node.right);
      return left + " AND " + right;
    }
    case NODE_OR: {
      std::string left = node_to_fts5_query(*node.left);
      std::string right = node_to_fts5_query(*node.right);
      return left + " OR " + right;
    }
    default:
      return "";
  }
}

inline std::string to_fts5_query(std::string query,
    std::function<std::string(const std::string&)> apply_lookup = nullptr) {
  if (query.empty()) {
    return "";
  }
  auto tokens = tokenize_input(query, apply_lookup);
  
  // Return empty for empty token list (only EOF)
  if (tokens.size() <= 1) {
    return "";
  }
  
  std::string err;
  auto node = parse_expression(tokens, err);
  if (!node) {
    return "";
  }
  
  return node_to_fts5_query(*node);
}

inline bool match_expression(std::string content, std::string query, std::string& err,
    std::function<std::string(const std::string&)> apply_lookup = nullptr) {
  if (query.empty()) {
    return true;
  }
  auto tokens = tokenize_input(query, apply_lookup);
  
  // Return true for empty token list (only EOF)
  if (tokens.size() <= 1) {
    return true;
  }
  
  auto node = parse_expression(tokens, err);
  if (!node) {
    return false;
  }

  std::string content_lower = content;
  std::transform(content_lower.begin(), content_lower.end(),
      content_lower.begin(), [](unsigned char c){ return std::tolower(c); });

  return eval(*node, content_lower);
}

} // namespace sqlite
} // namespace dialect
} // namespace searchquery

#endif // SEARCHQUERY_DIALECT_SQLITE_HXX
