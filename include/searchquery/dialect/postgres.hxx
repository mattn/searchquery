#ifndef SEARCHQUERY_DIALECT_POSTGRES_HXX
#define SEARCHQUERY_DIALECT_POSTGRES_HXX

#include <searchquery/base.hxx>

namespace searchquery {
namespace dialect {
namespace postgres {

static std::string escape_tsquery_term(std::string term);
static std::string node_to_tsquery(const node_t& node);

static std::string escape_tsquery_term(std::string term) {
  // Remove quotes if present
  if (!term.empty() && term.front() == '"' && term.back() == '"') {
    term = term.substr(1, term.size() - 2);
  }
  
  // Escape single quotes by doubling them
  std::string escaped;
  for (char c : term) {
    if (c == '\'') {
      escaped += "''";
    } else {
      escaped += c;
    }
  }
  
  // Check if term needs quoting
  bool needs_quoting = false;
  for (char c : escaped) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
          (c >= '0' && c <= '9') || c == '_')) {
      needs_quoting = true;
      break;
    }
  }
  
  if (needs_quoting) {
    return "'" + escaped + "'";
  }
  return escaped;
}

static inline std::string node_to_tsquery(const node_t& node) {
  switch (node.type) {
    case NODE_TERM: {
      // Check if it's a phrase (contains spaces)
      if (node.phrase.find(' ') != std::string::npos) {
        std::string result;
        std::string word;
        std::vector<std::string> words;
        for (char c : node.phrase) {
          if (std::isspace(c)) {
            if (!word.empty()) {
              words.push_back(word);
              word.clear();
            }
          } else {
            word += c;
          }
        }
        if (!word.empty()) {
          words.push_back(word);
        }
        
        for (size_t i = 0; i < words.size(); i++) {
          if (i > 0) result += " <-> ";
          result += escape_tsquery_term(words[i]);
        }
        return result;
      }
      // Single term
      return escape_tsquery_term(node.phrase);
    }
    case NODE_AND: {
      std::string left = node_to_tsquery(*node.left);
      std::string right = node_to_tsquery(*node.right);
      return "(" + left + " & " + right + ")";
    }
    case NODE_OR: {
      std::string left = node_to_tsquery(*node.left);
      std::string right = node_to_tsquery(*node.right);
      return "(" + left + " | " + right + ")";
    }
    default:
      return "";
  }
}

inline std::string to_tsquery(std::string query,
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
  
  return node_to_tsquery(*node);
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

} // namespace postgres
} // namespace dialect
} // namespace searchquery

#endif // SEARCHQUERY_DIALECT_POSTGRES_HXX
