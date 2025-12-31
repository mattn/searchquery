#ifndef SEARCHQUERY_BASE_HXX
#define SEARCHQUERY_BASE_HXX

#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include <cctype>

namespace searchquery {

typedef enum _token_type {
  TOKEN_TERM,
  TOKEN_AND,
  TOKEN_OR,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_EOF
} token_type;

typedef struct _token_t {
  token_type type;
  std::string value;
} token_t;

typedef enum _node_type {
  NODE_TERM,
  NODE_AND,
  NODE_OR
} node_type;

typedef struct _node_t {
  node_type type;
  std::string phrase;
  std::shared_ptr<struct _node_t> left;
  std::shared_ptr<struct _node_t> right;
} node_t;

inline std::vector<token_t> tokenize_input(const std::string& input,
    std::function<std::string(const std::string&)> apply_lookup = nullptr) {

  size_t i = 0;

  std::vector<token_t> tokens;
  while (i < input.size()) {
    if (std::isspace(input.at(i))) {
      i++;
      continue;
    }

    if (input.at(i) == '(') {
      tokens.push_back({TOKEN_LPAREN, ""});
      i++;
      continue;
    }

    if (input.at(i) == ')') {
      tokens.push_back({TOKEN_RPAREN, ""});
      i++;
      continue;
    }

    // Match AND or OR (case-sensitive, uppercase only)
    if (input.substr(i, 3) == "AND" &&
        (i + 3 == input.size() || std::isspace(input.at(i + 3)) ||
         input.at(i + 3) == '(' || input.at(i + 3) == ')')) {
      tokens.push_back({TOKEN_AND, ""});
      i += 3;
      continue;
    }
    if (input.substr(i, 2) == "OR" &&
        (i + 2 == input.size() || std::isspace(input.at(i + 2)) ||
         input.at(i + 2) == '(' || input.at(i + 2) == ')')) {
      tokens.push_back({TOKEN_OR, "OR"});
      i += 2;
      continue;
    }


    // Quoted phrase or unquoted term
    if (input.at(i) == '\"') {
      i++; // skip opening quote
      auto start = i;
      std::string term;
      while (i < input.size() && input.at(i) != '\"') {
        i++;
      }

      if (i >= input.size()) {
        // Unclosed quote: treat remaining text as term (simple fallback)
        auto term = input.substr(start);
        if (apply_lookup) {
          term = apply_lookup(term);
        }
        if (!term.empty()) {
          tokens.push_back({TOKEN_TERM, term});
        }
        i = input.size();
      } else {
        auto term = input.substr(start, i - start);
        if (apply_lookup) {
          term = apply_lookup(term);
        }
        if (!term.empty()) {
          tokens.push_back({TOKEN_TERM, term});
        }
        i++; // skip closing quote
      }
      continue;
    }

    // Regular term (key:value extensions are treated as terms and ignored)
    auto start = i;

    while (i < input.size() &&
        !std::isspace(input.at(i)) &&
        input.at(i) != '(' &&
        input.at(i) != ')') {
      i++;
    }
    auto term = input.substr(start, i - start);
    if (!term.empty()) {
      if (apply_lookup) {
        term = apply_lookup(term);
      }
      if (!term.empty()) {
        tokens.push_back({TOKEN_TERM, term});
      }
    }
  }
  tokens.push_back({TOKEN_EOF, ""});

  return tokens;
}

inline std::optional<node_t> parse_expression(const std::vector<token_t>& tokens, std::string& err) {
  std::vector<std::shared_ptr<node_t>> stack;
  std::vector<token_t> op_stack;
  
  size_t current = 0;
  
  auto apply_op = [&]() -> bool {
    if (stack.size() < 2 || op_stack.empty()) {
      err = "invalid expression";
      return false;
    }
    auto op = op_stack.back();
    op_stack.pop_back();
    auto right = stack.back();
    stack.pop_back();
    auto left = stack.back();
    stack.pop_back();
    
    auto node = std::make_shared<node_t>();
    node->type = (op.type == TOKEN_AND) ? NODE_AND : NODE_OR;
    node->left = left;
    node->right = right;
    stack.push_back(node);
    return true;
  };
  
  while (current < tokens.size()) {
    auto token = tokens[current++];
    if (token.type == TOKEN_EOF) {
      break;
    }
    if (token.type == TOKEN_TERM) {
      auto node = std::make_shared<node_t>();
      node->type = NODE_TERM;
      node->phrase = token.value;
      stack.push_back(node);
    } else if (token.type == TOKEN_LPAREN) {
      op_stack.push_back(token);
    } else if (token.type == TOKEN_RPAREN) {
      while (!op_stack.empty() && op_stack.back().type != TOKEN_LPAREN) {
        if (!apply_op()) {
          return std::nullopt;
        }
      }
      if (op_stack.empty()) {
        err = "mismatched parentheses";
        return std::nullopt;
      }
      op_stack.pop_back(); // pop LPAREN
    } else if (token.type == TOKEN_AND || token.type == TOKEN_OR) {
      while (!op_stack.empty()) {
        auto top = op_stack.back();
        if (top.type == TOKEN_LPAREN) {
          break;
        }
        if ((token.type == TOKEN_OR && top.type == TOKEN_AND) ||
            (token.type == TOKEN_AND && top.type == TOKEN_AND) ||
            (token.type == TOKEN_OR && top.type == TOKEN_OR)) {
          if (!apply_op()) {
            return std::nullopt;
          }
        } else {
          break;
        }
      }
      op_stack.push_back(token);
    }
  }
  
  // Apply remaining operators
  while (!op_stack.empty()) {
    if (op_stack.back().type == TOKEN_LPAREN) {
      err = "mismatched parentheses";
      return std::nullopt;
    }
    if (!apply_op()) {
      return std::nullopt;
    }
  }
  
  // Implicit AND for multiple terms
  while (stack.size() > 1) {
    auto left = stack[0];
    auto right = stack[1];
    stack.erase(stack.begin(), stack.begin() + 2);
    auto node = std::make_shared<node_t>();
    node->type = NODE_AND;
    node->left = left;
    node->right = right;
    stack.insert(stack.begin(), node);
  }
  
  if (stack.size() != 1) {
    err = "invalid expression";
    return std::nullopt;
  }
  
  return *stack[0];
}

inline bool eval(const node_t& node, const std::string& content) {
  switch (node.type) {
    case NODE_TERM: {
      std::string term_lower = node.phrase;
      std::transform(term_lower.begin(), term_lower.end(), term_lower.begin(),
          [](unsigned char c){ return std::tolower(c); });
      return content.find(term_lower) != std::string::npos;
    }
    case NODE_AND:
      return eval(*node.left, content) && eval(*node.right, content);
    case NODE_OR:
      return eval(*node.left, content) || eval(*node.right, content);
    default:
      return false;
  }
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

} // namespace searchquery

#endif // SEARCHQUERY_BASE_HXX
