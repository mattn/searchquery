# searchquery

A general-purpose C++ library for parsing and matching search queries, similar to X (Twitter) search syntax.

## Features

- **Intuitive Search Syntax**: Works like familiar search engines (X/Twitter, Google, etc.)
- **Implicit AND**: Multiple terms are automatically combined with AND logic
- **Phrase Search**: Support for quoted phrases with exact matching
- **Explicit Operators**: Support for AND, OR operators and parentheses
- **Case-Insensitive**: All matching is case-insensitive by default
- **Substring Matching**: Terms match anywhere within the content
- **Database Dialects**: Convert queries to various database formats
- **Header-Only**: Easy to integrate into your project

## Installation

Simply copy the `include/searchquery` directory to your project and include the headers:

```cpp
#include <searchquery/base.hxx>
#include <searchquery/dialect/postgres.hxx>
#include <searchquery/dialect/sqlite.hxx>
```

Or use the provided Makefile to build the example:

```bash
make
```

## Basic Usage

### Matching Content

```cpp
#include <searchquery/base.hxx>
#include <iostream>

using namespace searchquery;

int main() {
    std::string err;
    
    // Simple term search
    bool result = match_expression("Hello World", "hello", err);
    if (!err.empty()) {
        std::cerr << "Error: " << err << std::endl;
        return 1;
    }
    std::cout << result << std::endl; // true
    
    // Implicit AND (multiple terms)
    result = match_expression("Best Nostr Apps 2025", "nostr apps", err);
    std::cout << result << std::endl; // true
    
    // Phrase search
    result = match_expression("Say hello world today", "\"hello world\"", err);
    std::cout << result << std::endl; // true
    
    // Phrase must be contiguous
    result = match_expression("world hello", "\"hello world\"", err);
    std::cout << result << std::endl; // false
    
    return 0;
}
```

## Database Dialects

Convert search queries to database-specific formats.

### PostgreSQL Dialect

Converts search queries to PostgreSQL `tsquery` format for full-text search.

```cpp
#include <searchquery/dialect/postgres.hxx>
#include <iostream>

using namespace searchquery::dialect::postgres;

int main() {
    // Convert user query to tsquery
    std::string query = to_tsquery("hello world");
    std::cout << query << std::endl;
    // Output: (hello & world)
    
    // Phrase search example
    query = to_tsquery("\"hello world\"");
    std::cout << query << std::endl;
    // Output: hello <-> world
    
    return 0;
}
```

#### PostgreSQL Usage Example

```cpp
#include <searchquery/dialect/postgres.hxx>
#include <libpq-fe.h>

PGconn *conn = PQconnectdb("...");

// Convert user query to tsquery
std::string user_query = "hello world";
std::string tsquery = searchquery::dialect::postgres::to_tsquery(user_query);
// tsquery = "(hello & world)"

// Use in PostgreSQL query
std::string sql = "SELECT title, content FROM articles "
                  "WHERE to_tsvector('english', content) @@ to_tsquery('english', $1)";
PGresult *res = PQexecParams(conn, sql.c_str(), 1, 
                             NULL, &tsquery.c_str(), NULL, NULL, 0);
```

#### PostgreSQL Query Conversion Examples

| Input Query | PostgreSQL tsquery |
|-------------|-------------------|
| `hello world` | `(hello & world)` |
| `"hello world"` | `hello <-> world` |
| `cat dog bird` | `((cat & dog) & bird)` |
| `"quick brown fox"` | `quick <-> brown <-> fox` |
| `hello@world` | `'hello@world'` |

#### PostgreSQL Features

- **Implicit AND**: Multiple terms are combined with `&` operator
- **Phrase Search**: Quoted phrases use `<->` (followed-by) operator
- **Special Character Escaping**: Handles special characters properly
- **Explicit Operators**: Supports AND/OR operators

### SQLite Dialect

Converts search queries to SQLite FTS5 MATCH format for full-text search.

```cpp
#include <searchquery/dialect/sqlite.hxx>
#include <iostream>

using namespace searchquery::dialect::sqlite;

int main() {
    // Convert user query to FTS5 format
    std::string query = to_fts5_query("hello world");
    std::cout << query << std::endl;
    // Output: hello AND world
    
    // Phrase search example
    query = to_fts5_query("\"hello world\"");
    std::cout << query << std::endl;
    // Output: "hello world"
    
    return 0;
}
```

#### SQLite Usage Example

```cpp
#include <searchquery/dialect/sqlite.hxx>
#include <sqlite3.h>

sqlite3 *db;
sqlite3_open("test.db", &db);

// Create FTS5 table
sqlite3_exec(db, "CREATE VIRTUAL TABLE articles USING fts5(title, content)", 
             NULL, NULL, NULL);

// Convert user query to FTS5 format
std::string user_query = "hello world";
std::string fts_query = searchquery::dialect::sqlite::to_fts5_query(user_query);
// fts_query = "hello AND world"

// Use in SQLite FTS5 query
sqlite3_stmt *stmt;
const char *sql = "SELECT title, content FROM articles WHERE articles MATCH ?";
sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
sqlite3_bind_text(stmt, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
```

#### SQLite Query Conversion Examples

| Input Query | SQLite FTS5 Query |
|-------------|-------------------|
| `hello world` | `hello AND world` |
| `"hello world"` | `"hello world"` |
| `cat dog bird` | `cat AND dog AND bird` |
| `hello OR world` | `hello OR world` |

#### SQLite Features

- **Implicit AND**: Multiple terms use `AND` keyword
- **Phrase Search**: Quoted phrases remain quoted
- **OR Support**: Explicit OR operators are preserved
- **Simple Syntax**: Clean, readable query format

## Query Syntax

This library implements a search syntax similar to popular search engines like X (Twitter):

### Core Features

- **Implicit AND**: `hello world` matches content containing both "hello" AND "world"
- **Explicit AND**: `hello AND world` explicitly requires both terms
- **OR Operator**: `hello OR world` matches content with either term
- **Parentheses**: `(cat AND dog) OR bird` for grouping and precedence
- **Case-Insensitive**: `HELLO` matches "hello", "Hello", "HELLO", etc.
- **Substring Matching**: `wor` matches "world", "work", "sword", etc.
- **Phrase Search**: `"hello world"` matches the exact phrase (contiguous)

### Examples

```cpp
// Implicit AND - both terms must be present
eval(node, "i have a cat and a dog")  // query: "cat dog" -> true
eval(node, "i have a cat")            // query: "cat dog" -> false

// Phrase search - terms must be contiguous
eval(node, "say hello world today")   // query: "\"hello world\"" -> true
eval(node, "world hello")             // query: "\"hello world\"" -> false

// Explicit operators
eval(node, "i have a cat")            // query: "cat OR dog" -> true
eval(node, "i have a bird")           // query: "(cat AND dog) OR bird" -> true

// Case insensitive
eval(node, "hello world")             // query: "HELLO" -> true

// Substring matching
eval(node, "hello world")             // query: "wor" -> true
```

## Examples

See the `example/` directory for complete working examples:

- `example/sqlite/` - Full SQLite FTS5 integration with initialization, listing, and search

### Building Examples

```bash
cd example/sqlite
make
./example_sqlite -init           # Initialize database with sample data
./example_sqlite -list            # List all records
./example_sqlite "hello"          # Search for "hello"
./example_sqlite "\"hello world\""  # Phrase search
```

## Testing

Run the test suite:

```bash
make test
```

The test suite includes comprehensive coverage of:
- Simple term matching
- Implicit AND behavior
- Phrase search
- Explicit AND/OR operators
- Parentheses and grouping
- Edge cases
- Database dialect conversions

## Building

The library is header-only, but you can build the command-line tool:

```bash
make          # Build the searchquery command-line tool
make clean    # Clean build artifacts
```

### Compiler Requirements

- C++17 or later
- GCC 7+, Clang 5+, or compatible compiler

## Use Cases

This library is suitable for:

- **Text Search Applications**: Add search functionality to your C++ applications
- **Database Integration**: Convert queries for PostgreSQL, SQLite, and other databases
- **Log Filtering**: Search through log files with intuitive query syntax
- **Content Management**: Filter and search user-generated content
- **Any Application**: That needs X/Twitter-style search functionality

## Architecture

The library consists of:

- **`searchquery/base.hxx`**: Core tokenizer, parser, and evaluator
- **`searchquery/dialect/postgres.hxx`**: PostgreSQL tsquery converter
- **`searchquery/dialect/sqlite.hxx`**: SQLite FTS5 converter

All functions are `inline` to avoid ODR violations when included in multiple translation units.

## License

MIT

## Author

Yasuhiro Matsumoto (a.k.a. mattn)
