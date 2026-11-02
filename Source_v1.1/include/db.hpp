#pragma once
#include <sqlite3.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace lms {
class DbError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class Db;
class Statement {
  sqlite3_stmt* stmt_ = nullptr;
  Db* db_ = nullptr;
public:
  Statement(Db& db, const std::string& sql);
  ~Statement();
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;
  Statement(Statement&&) = delete;
  void bind(int index, const std::string& value);
  void bind(int index, std::int64_t value);
  void bind(int index, double value);
  void bindNull(int index);
  bool row(); // true=SQLITE_ROW; false=SQLITE_DONE
  std::int64_t integer(int column) const;
  double decimal(int column) const;
  std::string text(int column) const;
  bool isNull(int column) const;
};
class Db {
 sqlite3* conn_ = nullptr;
public:
 explicit Db(const std::string& filename);
 ~Db();
 Db(const Db&) = delete;
 Db& operator=(const Db&) = delete;
 sqlite3* handle() const {return conn_;}
 void exec(const std::string& sql);
 void loadSchema(const std::string& filename);
 std::int64_t lastId() const;
 int changes() const;
 std::string error() const;
};
class Transaction {
 Db& db_;
 bool committed_ = false;
public:
 explicit Transaction(Db& db);
 void commit();
 ~Transaction();
 Transaction(const Transaction&)=delete;
};
}
