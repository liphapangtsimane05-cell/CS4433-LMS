#include "../include/db.hpp"
#include <fstream>
#include <sstream>
#include <utility>
namespace lms {
Db::Db(const std::string& filename) {
 if(sqlite3_open_v2(filename.c_str(),&conn_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,nullptr)!=SQLITE_OK) {
  std::string msg=conn_?sqlite3_errmsg(conn_):"Unable to allocate SQLite handle";
  if(conn_) sqlite3_close(conn_);
  conn_=nullptr;
  throw DbError("Open database: "+msg);
 }
 sqlite3_busy_timeout(conn_,5000);
 exec("PRAGMA foreign_keys=ON;");
}
Db::~Db(){if(conn_) sqlite3_close(conn_);}
void Db::exec(const std::string& sql) {
 char* message=nullptr;
 int rc=sqlite3_exec(conn_,sql.c_str(),nullptr,nullptr,&message);
 if(rc!=SQLITE_OK){std::string err=message?message:sqlite3_errmsg(conn_); sqlite3_free(message);throw DbError(err);}
}
void Db::loadSchema(const std::string& filename){
 std::ifstream file(filename);
 if(!file) throw DbError("Could not read schema: "+filename);
 std::ostringstream data;data<<file.rdbuf();exec(data.str());
}
std::int64_t Db::lastId()const{return sqlite3_last_insert_rowid(conn_);}
int Db::changes()const{return sqlite3_changes(conn_);}
std::string Db::error()const{return sqlite3_errmsg(conn_);}
Statement::Statement(Db& db,const std::string& sql):db_(&db){
 if(sqlite3_prepare_v2(db.handle(),sql.c_str(),-1,&stmt_,nullptr)!=SQLITE_OK)
  throw DbError("Prepare statement: "+db.error());
}
Statement::~Statement(){if(stmt_)sqlite3_finalize(stmt_);}
void Statement::bind(int idx,const std::string& v){if(sqlite3_bind_text(stmt_,idx,v.c_str(),-1,SQLITE_TRANSIENT)!=SQLITE_OK)throw DbError("Bind text failed");}
void Statement::bind(int idx,std::int64_t v){if(sqlite3_bind_int64(stmt_,idx,v)!=SQLITE_OK)throw DbError("Bind integer failed");}
void Statement::bind(int idx,double v){if(sqlite3_bind_double(stmt_,idx,v)!=SQLITE_OK)throw DbError("Bind decimal failed");}
void Statement::bindNull(int idx){if(sqlite3_bind_null(stmt_,idx)!=SQLITE_OK)throw DbError("Bind null failed");}
bool Statement::row(){int code=sqlite3_step(stmt_);if(code==SQLITE_ROW)return true;if(code==SQLITE_DONE)return false;throw DbError("SQL operation failed: "+db_->error());}
std::int64_t Statement::integer(int n)const{return sqlite3_column_int64(stmt_,n);}
double Statement::decimal(int n)const{return sqlite3_column_double(stmt_,n);}
std::string Statement::text(int n)const{const unsigned char* p=sqlite3_column_text(stmt_,n);return p?reinterpret_cast<const char*>(p):"";}
bool Statement::isNull(int n)const{return sqlite3_column_type(stmt_,n)==SQLITE_NULL;}
Transaction::Transaction(Db& db):db_(db){db_.exec("BEGIN IMMEDIATE;");}
void Transaction::commit(){db_.exec("COMMIT;");committed_=true;}
Transaction::~Transaction(){if(!committed_){try{db_.exec("ROLLBACK;");}catch(const std::exception&){}}}
}
