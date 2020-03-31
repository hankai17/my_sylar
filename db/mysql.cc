#include "db/mysql.hh"
#include "log.hh"

namespace sylar {
  static Logger::ptr g_logger = SYLARY_LOG_NAME("system");

  MySQLRes::MySQLRes(MYSQL_RES* res, int eno, const char* estr)
  : m_errno(eno),
    m_strerr(estr) {
      if (res) {
        m_data.reset(res, mysql_free_result);
      }
    }

  uint64_t MySQLRes::getRows() {
    return mysql_num_rows(m_data.get());
  }

  uint64_t MySQLRes::getFields() {
    return mysql_num_fields(m_data.get());
  }

  bool MySQLRes::foreach(data_cb cb) {
    MYSQL_ROW row;
    uint64_t fields = getFields();
    int i = 0;
    while ((row = mysql_fetch_row(m_data.get()))) {
      if (!cb(row, fields, i++)) {
        break;
      }
    }
    return true;
  }

  MySQL::MySQL(const std::map<std::string, std::string>& args)
  : m_params(args),
    m_lastUseTime(0),
    m_hasError(false) {
  }

  static MYSQL* mysql_init(std::map<std::string, std::string>& params, 
        const int& timeout) {
    static thread_local MySQLThreadIniter s_thread_initer;
    MYSQL* mysql = ::mysql_init(nullptr);
    if (mysql == nullptr) {
      SYLAR_LOG_ERROR(g_logger) << "mysql_init failed";
      return nullptr;
    }
    if (timeout > 0) {
      mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    }
    bool close = false;
    mysql_options(mysql, MYSQL_OPT_RECONNECT, &close);
    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "UTF8");

    int port = 5503;
    std::string host("root");
    std::string user("root");
    std::string pwd("123");
    std::string dbname("db1");

    if (mysql_real_connect(mysql, host.c_str(), user.c_str(), pwd.c_str(),
            dbname.c_str(), port, NULL, 0) == nullptr) {
      SYLAR_LOG_ERROR(g_logger) << "mysql_real_connect(" << host << ", "
        << user << ", " << pwd << ", " << dbname << ", " << port << ")"
        << " err:" <<   mysql_error(mysql);
      mysql_close(mysql);
      return nullptr;
    }
    return mysql;
  }

  bool MySQL::connect() {
    if (m_mysql && !m_hasError) {
      return true;
    }
    MYSQL* m = mysql_init(m_params, 0);
    if (!m) {
      m_hasError = true;
      return false;
    }
    m_hasError = false;
    m_mysql.reset(m, mysql_close);
    return true;
  }

  bool MySQL::ping() {
    if (!m_mysql) {
      return false;
    }
    if (mysql_ping(m_mysql.get())) {
      m_hasError = true;
      return false;
    }
    m_hasError = false;
    return true;
  }

  int MySQL::cmd(const char* format, va_list ap) {
    m_cmd = sylar::StringUtil::Formatv(format, ap);
    int ret = ::mysql_query(m_mysql.get(), m_cmd.c_str());
    if (ret) {
      SYLARY_LOG_ERROR(g_logger) << "cmd: " << m_cmd
        << " failed: " << getError();
      m_hasError = true;
    } else {
      m_hasError = false;
    }
    return ret;
  }

  int MySQL::cmd(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = cmd(format, ap);
    va_end(ap);
    return ret;
  }

  int MySQL::cmd(const std::string& sql) {
    m_cmd = sql;
    int ret = ::mysql_query(m_myql.get(), m_cmd.c_str());
    if (ret) {
      SYLARY_LOG_ERROR(g_logger) << "cmd: " << m_cmd
        << " failed: " << getError();
      m_hasError = true;
    } else {
      m_hasError = false;
    }
    return ret;
  }

  std::shared_ptr<MySQL> MySQL::getMySQL() {
    return MySQL::ptr(this, sylar::nop<MySQL>); // ??????
  }

  uint64_t MySQL::getAffectedRows() {
    if (!m_mysql) {
      return 0;
    }
    return mysql_affected_rows(m_mysql.get());
  }

  static MYSQL_RES* my_myql_query(MYSQL* mysql, const char* sql) {
    if (mysql == nullptr) {
      SYLARY_LOG_ERROR(g_logger) << "mysql_query mysql handler is null";
      return nullptr;
    }
    if (sql == nullptr) {
      SYLARY_LOG_ERROR(g_logger) << "mysql_query sql is null";
      return nullptr;
    }
    if (::mysql_query(mysql, sql)) {
      SYLARY_LOG_ERROR(g_logger) << "mysql_query(" << sql << ") failed: "
        << mysql_error(mysql);
      return nullptr;
    }
    MYSQL_RES* res = mysql_store_result(mysql);
    if (res == nullptr) {
      SYLARY_LOG_ERROR(g_logger) << "mysql_store_result failed: "
        << mysql_error(mysql);
      return nullptr;
    }
    return res;
  }

  MySQLRes::ptr MySQL::query(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    auto ret = query(format, ap);
    va_end(ap);
    return ret;
  }

  MySQLRes::ptr MySQL::query(const char* format, va_list ap) {
    m_cmd = sylar::StringUtil::Formatv(format, ap);
    MYSQL_RES* res = my_mysql_query(m_mysql.get(), m_cmd.c_str());
    if (!res) {
      m_hasError = true;
      return nullptr;
    }
    m_hasError = false;
    MySQLRes::ptr ret(new MySQLRes::(res, mysql_errno(m_mysql.get(), 
              mysql_errno(m_mysql.get()))));
    return ret;
  }

  MySQLRes::ptr MySQL::query(const std::string& sql) {
    m_cmd = sql;
    MYSQL_RES* res = my_mysql_query(m_mysql.get(), m_cmd.c_str());
    if (!res) {
      m_hasError = true;
      return nullptr;
    }
    m_hasError = false;
    MySQLRes::ptr ret(new MySQLRes::(res, mysql_errno(m_mysql.get(), 
              mysql_errno(m_mysql.get()))));
    return ret;
  }

  const char* MySQL::cmd() {
    return m_cmd.c_str();
  }

  bool MySQL::use(const std::string& dbname) {
    if (!m_mysql) {
      return false;
    }
    if (m_dbname == dbname) {
      return true;
    }
    if (mysql_select_db(m_mysql.get(), dbname.c_str()) == 0) {
      m_dbname = dbname;
      m_hasError = false;
      return true;
    } else {
      m_dbname = "";
      m_hasError = true;
      return false;
    }
  }

  const char* MySQL::getError() {
    if (!m_mysql) {
      return "";
    }
    const char* str = mysql_error(m_mysql.get());
    if (str) {
      return str;
    }
    return "";
  }

  uint64_t getInsertId() {
    if (m_mysql) {
      return mysql_insert_id(m_mysql.get());
    }
    return 0;
  }

};
