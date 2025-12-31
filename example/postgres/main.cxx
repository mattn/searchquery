#include <searchquery/dialect/postgres.hxx>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <memory>

#include <postgresql/libpq-fe.h>

static PGconn *conn = nullptr;

static bool
init_database() {
  const char *sql = "CREATE TABLE IF NOT EXISTS example (id SERIAL PRIMARY KEY, data TEXT)";
  PGresult *res = PQexec(conn, sql);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    std::cerr << "Failed to create table: " << PQerrorMessage(conn) << std::endl;
    PQclear(res);
    return false;
  }
  PQclear(res);

  sql = "CREATE TABLE IF NOT EXISTS example_tsvector (id INTEGER PRIMARY KEY, data_tsv tsvector)";
  res = PQexec(conn, sql);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    std::cerr << "Failed to create tsvector table: " << PQerrorMessage(conn) << std::endl;
    PQclear(res);
    return false;
  }
  PQclear(res);

  sql = "CREATE OR REPLACE FUNCTION example_ai() RETURNS TRIGGER AS $$ BEGIN "
        "INSERT INTO example_tsvector(id, data_tsv) VALUES (NEW.id, to_tsvector('simple', NEW.data)) "
        "ON CONFLICT (id) DO UPDATE SET data_tsv = to_tsvector('simple', NEW.data); RETURN NEW; END; $$ LANGUAGE plpgsql";
  res = PQexec(conn, sql);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    std::cerr << "Failed to create function: " << PQerrorMessage(conn) << std::endl;
    PQclear(res);
    return false;
  }
  PQclear(res);

  sql = "DROP TRIGGER IF EXISTS example_ai_trigger ON example";
  res = PQexec(conn, sql);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    std::cerr << "Failed to drop trigger: " << PQerrorMessage(conn) << std::endl;
    PQclear(res);
    return false;
  }
  PQclear(res);

  sql = "CREATE TRIGGER example_ai_trigger AFTER INSERT ON example FOR EACH ROW EXECUTE FUNCTION example_ai()";
  res = PQexec(conn, sql);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    std::cerr << "Failed to create trigger: " << PQerrorMessage(conn) << std::endl;
    PQclear(res);
    return false;
  }
  PQclear(res);

  sql = "TRUNCATE TABLE example RESTART IDENTITY";
  res = PQexec(conn, sql);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    std::cerr << "Failed to truncate table: " << PQerrorMessage(conn) << std::endl;
    PQclear(res);
    return false;
  }
  PQclear(res);

  sql = "TRUNCATE TABLE example_tsvector";
  res = PQexec(conn, sql);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    std::cerr << "Failed to truncate tsvector table: " << PQerrorMessage(conn) << std::endl;
    PQclear(res);
    return false;
  }
  PQclear(res);

  sql = "INSERT INTO example(data) VALUES($1)";
  std::vector<std::string> data = {
    "Hello World",
    "Great World",
    "Hello Go",
    "Golang programming",
    "Rust language"
  };

  for (const auto &d : data) {
    const char *param_values[1] = {d.c_str()};
    res = PQexecParams(conn, sql, 1, nullptr, param_values, nullptr, nullptr, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      std::cerr << "Failed to insert record: " << PQerrorMessage(conn) << std::endl;
      PQclear(res);
      return false;
    }
    PQclear(res);
  }

  return true;
}

static bool
list_all() {
  const char *sql = "SELECT e.id, e.data FROM example e ORDER BY e.id";
  PGresult *res = PQexec(conn, sql);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    std::cerr << "Failed to execute query: " << PQerrorMessage(conn) << std::endl;
    PQclear(res);
    return false;
  }

  int nrows = PQntuples(res);
  for (int i = 0; i < nrows; ++i) {
    int id = std::atoi(PQgetvalue(res, i, 0));
    const char *text = PQgetvalue(res, i, 1);
    std::cout << "ID: " << id << ", Data: " << text << std::endl;
  }

  PQclear(res);
  return true;
}

static bool
search(const std::string &query) {
  auto tsquery = searchquery::dialect::postgres::to_tsquery(query);
  if (tsquery.empty()) {
    std::cerr << "Invalid query" << std::endl;
    return false;
  }

  const char *sql = "SELECT e.id, e.data FROM example e JOIN example_tsvector f ON e.id = f.id "
                    "WHERE f.data_tsv @@ to_tsquery('simple', $1) ORDER BY e.id";
  const char *param_values[1] = {tsquery.c_str()};
  PGresult *res = PQexecParams(conn, sql, 1, nullptr, param_values, nullptr, nullptr, 0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    std::cerr << "Failed to execute query: " << PQerrorMessage(conn) << std::endl;
    PQclear(res);
    return false;
  }

  int nrows = PQntuples(res);
  for (int i = 0; i < nrows; ++i) {
    int id = std::atoi(PQgetvalue(res, i, 0));
    const char *text = PQgetvalue(res, i, 1);
    std::cout << "ID: " << id << ", Data: " << text << std::endl;
  }

  PQclear(res);
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

  const char *conninfo = std::getenv("DATABASE_URL");
  if (!conninfo) {
    conninfo = "dbname=postgres";
  }

  conn = PQconnectdb(conninfo);
  if (PQstatus(conn) != CONNECTION_OK) {
    std::cerr << "Failed to connect to database: " << PQerrorMessage(conn) << std::endl;
    PQfinish(conn);
    return 1;
  }

  if (init_flag) {
    if (!init_database()) {
      PQfinish(conn);
      return 1;
    }
  } else if (list_flag) {
    if (!list_all()) {
      PQfinish(conn);
      return 1;
    }
  } else {
    if (!search(query)) {
      PQfinish(conn);
      return 1;
    }
  }

  PQfinish(conn);
  return 0;
}
