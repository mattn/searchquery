#include <searchquery/dialect/sqlite.hxx>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#include <sqlite3.h>

static sqlite3 *conn = nullptr;

static bool
init_database() {
  auto ret = sqlite3_open("database.sqlite3", &conn);
  if (ret != SQLITE_OK) {
    std::cerr << "Failed to open database: " << sqlite3_errmsg(conn) << std::endl;
    return false;
  }

  const char *sql = "CREATE TABLE IF NOT EXISTS example (id INTEGER PRIMARY KEY, data TEXT)";
  char *err_msg = nullptr;
  ret = sqlite3_exec(conn, sql, nullptr, nullptr, &err_msg);
  if (ret != SQLITE_OK) {
    std::cerr << "Failed to create table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    return false;
  }

  sql = "CREATE VIRTUAL TABLE IF NOT EXISTS example_fts USING fts5(data, content='example', content_rowid='id')";
  ret = sqlite3_exec(conn, sql, nullptr, nullptr, &err_msg);
  if (ret != SQLITE_OK) {
    std::cerr << "Failed to create FTS table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    return false;
  }

  sql = "CREATE TRIGGER IF NOT EXISTS example_ai AFTER INSERT ON example BEGIN INSERT INTO example_fts(rowid, data) VALUES (new.id, new.data); END;";
  ret = sqlite3_exec(conn, sql, nullptr, nullptr, &err_msg);
  if (ret != SQLITE_OK) {
    std::cerr << "Failed to create trigger: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    return false;
  }

  sql = "DELETE FROM example";
  ret = sqlite3_exec(conn, sql, nullptr, nullptr, &err_msg);
  if (ret != SQLITE_OK) {
    std::cerr << "Failed to delete records: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    return false;
  }

  sqlite3_stmt *stmt = nullptr;
  sql = "INSERT INTO example(data) VALUES(?)";
  ret = sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr);
  if (ret != SQLITE_OK) {
    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(conn) << std::endl;
    return false;
  }

  std::vector<std::string> data = {
    "Hello World",
    "Great World",
    "Hello Go",
    "Golang programming",
    "Rust language"
  };

  for (const auto &d : data) {
    sqlite3_bind_text(stmt, 1, d.data(), (int)d.size(), SQLITE_TRANSIENT);
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) {
      std::cerr << "Failed to insert record: " << sqlite3_errmsg(conn) << std::endl;
      sqlite3_finalize(stmt);
      return false;
    }
    sqlite3_reset(stmt);
  }

  sqlite3_finalize(stmt);
  return true;
}

static bool
list_all() {
  const char *sql = "SELECT rowid, data FROM example_fts ORDER BY rowid";
  sqlite3_stmt *stmt = nullptr;
  auto ret = sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr);
  if (ret != SQLITE_OK) {
    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(conn) << std::endl;
    return false;
  }

  while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const char *text = (const char *)sqlite3_column_text(stmt, 1);
    std::cout << "ID: " << id << ", Data: " << text << std::endl;
  }

  sqlite3_finalize(stmt);
  return true;
}

static bool
search(const std::string &query) {
  auto fts_query = searchquery::dialect::sqlite::to_fts5_query(query);
  if (fts_query.empty()) {
    std::cerr << "Invalid query" << std::endl;
    return false;
  }

  const char *sql = "SELECT rowid, data FROM example_fts WHERE data MATCH ? ORDER BY rowid";
  sqlite3_stmt *stmt = nullptr;
  auto ret = sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr);
  if (ret != SQLITE_OK) {
    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(conn) << std::endl;
    return false;
  }

  sqlite3_bind_text(stmt, 1, fts_query.data(), (int)fts_query.size(), SQLITE_TRANSIENT);

  while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    const char *text = (const char *)sqlite3_column_text(stmt, 1);
    std::cout << "ID: " << id << ", Data: " << text << std::endl;
  }

  sqlite3_finalize(stmt);
  return true;
}

int
main(int argc, char *argv[]) {
  bool init_flag = false;
  bool list_flag = false;
  std::string query;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "-init") == 0) {
      init_flag = true;
    } else if (std::strcmp(argv[i], "-list") == 0) {
      list_flag = true;
    } else {
      query = argv[i];
    }
  }

  if (!init_flag && !list_flag && query.empty()) {
    std::cerr << "Please provide a search query or use -init to initialize the index, or -list to list all items" << std::endl;
    return 1;
  }

  auto ret = sqlite3_open("database.sqlite3", &conn);
  if (ret != SQLITE_OK) {
    std::cerr << "Failed to open database: " << sqlite3_errmsg(conn) << std::endl;
    return 1;
  }

  if (init_flag) {
    if (!init_database()) {
      sqlite3_close(conn);
      return 1;
    }
  } else if (list_flag) {
    if (!list_all()) {
      sqlite3_close(conn);
      return 1;
    }
  } else {
    if (!search(query)) {
      sqlite3_close(conn);
      return 1;
    }
  }

  sqlite3_close(conn);
  return 0;
}
