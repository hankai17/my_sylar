#include "my_sylar/db/mysql.hh"
#include "my_sylar/log.hh"
#include "my_sylar/util.hh"

namespace sylar {
    static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    bool mysqltime_to_timet(const MYSQL_TIME& mt, time_t& ts) {
         struct tm tm;
         ts = 0;
         localtime_r(&ts, &tm);
         tm.tm_year = mt.year - 1900;
         tm.tm_mon = mt.month - 1;
         tm.tm_mday = mt.day;
         tm.tm_hour = mt.hour;
         tm.tm_min = mt.minute;
         tm.tm_sec = mt.second;
         ts = mktime(&tm);
         return true;
    }

    bool timet_to_mysqltime(const time_t& ts, MYSQL_TIME& mt) {
        struct tm tm;
        localtime_r(&ts, &tm);
        mt.year = tm.tm_year + 1900;
        mt.month = tm.tm_mon + 1;
        mt.day = tm.tm_mday;
        mt.hour = tm.tm_hour;
        mt.minute = tm.tm_min;
        mt.second = tm.tm_sec;
        return true;
    }

    MySQLRes::MySQLRes(MYSQL_RES *res, int eno, const char *estr)
            : m_errno(eno),
              m_strerr(estr),
              m_cur(nullptr),
              m_curLength(nullptr) {
        if (res) {
            m_data.reset(res, mysql_free_result);
        }
    }

    int MySQLRes::getRowCount() {
        return mysql_num_rows(m_data.get());
    }

    int MySQLRes::getColumnCount() {
        return mysql_num_fields(m_data.get());
    }

    int MySQLRes::getColumnBytes(int idx) {
        return m_curLength[idx];
    }

    int MySQLRes::getColumnType(int idx) {
        return 0;
    }

    std::string MySQLRes::getColumName(int idx) {
        return "";
    }

    bool MySQLRes::isNull(int idx) {
        if (m_cur[idx] == nullptr) {
            return true;
        }
        return false;
    }

    int8_t MySQLRes::getInt8(int idx) {
        return getInt64(idx);
    }

    uint8_t MySQLRes::getUint8(int idx) {
        return getInt64(idx);
    }

    int16_t MySQLRes::getInt16(int idx) {
        return getInt64(idx);
    }

    uint16_t MySQLRes::getUint16(int idx) {
        return getInt64(idx);
    }

    int32_t MySQLRes::getInt32(int idx) {
        return getInt64(idx);
    }

    uint32_t MySQLRes::getUint32(int idx) {
        return getInt64(idx);
    }

    int64_t MySQLRes::getInt64(int idx) {
        return sylar::TypeUtil::Atoi(m_cur[idx]);
    }

    uint64_t MySQLRes::getUint64(int idx) {
        return getInt64(idx);
    }

    float MySQLRes::getFloat(int idx) {
        return getDouble(idx);
    }

    double MySQLRes::getDouble(int idx) {
        return sylar::TypeUtil::Atof(m_cur[idx]);
    }

    std::string MySQLRes::getString(int idx) {
        return std::string(m_cur[idx], m_curLength[idx]);
    }

    std::string MySQLRes::getBlob(int idx) {
        return std::string(m_cur[idx], m_curLength[idx]);
    }

    time_t MySQLRes::getTime(int idx) {
        if (!m_cur[idx]) {
            return 0;
        }
        return sylar::Str2Time(m_cur[idx]);
    }

    bool MySQLRes::next() {
        m_cur = mysql_fetch_row(m_data.get()); // 返回指针字符串数组 //https://blog.csdn.net/qq_25908839/article/details/102753510
        m_curLength = mysql_fetch_lengths(m_data.get());
        return m_cur;
    }

    bool MySQLRes::foreach(data_cb cb) {
        MYSQL_ROW row;
        uint64_t fields = getColumnCount();
        int i = 0;
        while ((row = mysql_fetch_row(m_data.get()))) {
            if (!cb(row, fields, i++)) {
                break;
            }
        }
        return true;
    }

    MySQL::MySQL(const std::map<std::string, std::string> &args)
            : m_params(args),
              m_lastUseTime(0),
              m_hasError(false) {
    }

    static MYSQL *mysql_init(std::map<std::string, std::string> &params,
                             const int &timeout) {
        //static thread_local MySQLThreadIniter s_thread_initer;
        MYSQL *mysql = ::mysql_init(nullptr);
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

        int port = sylar::GetParaValue(params, "port", 3306);
        std::string host = sylar::GetParaValue<std::string>(params, "host");
        std::string user = sylar::GetParaValue<std::string>(params, "user");
        std::string pwd = sylar::GetParaValue<std::string>(params, "passwd");
        std::string dbname = sylar::GetParaValue<std::string>(params, "dbname");

        if (mysql_real_connect(mysql, host.c_str(), user.c_str(), pwd.c_str(),
                               dbname.c_str(), port, NULL, 0) == nullptr) {
            SYLAR_LOG_ERROR(g_logger) << "mysql_real_connect(" << host << ", "
                                      << user << ", " << pwd << ", " << dbname << ", " << port << ")"
                                      << " err:" << mysql_error(mysql);
            mysql_close(mysql);
            return nullptr;
        }
        return mysql;
    }

    bool MySQL::connect() {
        if (m_mysql && !m_hasError) {
            return true;
        }
        MYSQL *m = mysql_init(m_params, 0);
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

    int MySQL::execute(const char* format, va_list ap) {
        m_cmd = sylar::StringUtil::Formatv(format, ap);
        int ret = ::mysql_query(m_mysql.get(), m_cmd.c_str());
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "cmd: " << m_cmd
                                      << " failed: " << getError();
            m_hasError = true;
        } else {
            m_hasError = false;
        }
        return ret;
    }

    int MySQL::execute(const char* format, ...) {
        va_list ap;
        va_start(ap, format);
        int ret = execute(format, ap);
        va_end(ap);
        return ret;
    }

    int MySQL::execute(const std::string &sql) {
        m_cmd = sql;
        int ret = ::mysql_query(m_mysql.get(), m_cmd.c_str());
        if (ret) {
            SYLAR_LOG_ERROR(g_logger) << "cmd: " << m_cmd
                                      << " failed: " << getError();
            m_hasError = true;
        } else {
            m_hasError = false;
        }
        return ret;
    }

    int64_t MySQL::getLastInsertId() {
        if (m_mysql) {
            return mysql_insert_id(m_mysql.get());
        }
        return 0;
    }

    IStmt::ptr MySQL::prepare(const std::string& stmt) {
        return MySQLStmt::Create(shared_from_this(), stmt);
    }

    uint64_t MySQL::getAffectedRows() {
        if (!m_mysql) {
            return 0;
        }
        return mysql_affected_rows(m_mysql.get());
    }

    static MYSQL_RES *my_mysql_query(MYSQL* mysql, const char* sql) {
        if (mysql == nullptr) {
            SYLAR_LOG_ERROR(g_logger) << "mysql_query mysql handler is null";
            return nullptr;
        }
        if (sql == nullptr) {
            SYLAR_LOG_ERROR(g_logger) << "mysql_query sql is null";
            return nullptr;
        }
        if (::mysql_query(mysql, sql)) {
            SYLAR_LOG_ERROR(g_logger) << "mysql_query(" << sql << ") failed: "
                                      << mysql_error(mysql);
            return nullptr;
        }
        MYSQL_RES *res = mysql_store_result(mysql);
        if (res == nullptr) {
            SYLAR_LOG_ERROR(g_logger) << "mysql_store_result failed: "
                                      << mysql_error(mysql);
            return nullptr;
        }
        return res;
    }

    ISQLData::ptr MySQL::query(const char* format, ...) {
        va_list ap;
        va_start(ap, format);
        auto ret = query(format, ap);
        va_end(ap);
        return ret;
    }

    ISQLData::ptr MySQL::query(const char* format, va_list ap) {
        m_cmd = sylar::StringUtil::Formatv(format, ap);
        MYSQL_RES *res = my_mysql_query(m_mysql.get(), m_cmd.c_str());
        if (!res) {
            m_hasError = true;
            return nullptr;
        }
        m_hasError = false;
        MySQLRes::ptr ret(new MySQLRes(res, mysql_errno(m_mysql.get()),
                                       mysql_error(m_mysql.get())
        ));
        return ret;
    }

    ISQLData::ptr MySQL::query(const std::string &sql) {
        m_cmd = sql;
        MYSQL_RES *res = my_mysql_query(m_mysql.get(), m_cmd.c_str());
        if (!res) {
            m_hasError = true;
            return nullptr;
        }
        m_hasError = false;
        MySQLRes::ptr ret(new MySQLRes(res, mysql_errno(m_mysql.get()),
                                       mysql_error(m_mysql.get())
        ));
        return ret;
    }

    const char* MySQL::cmd() {
        return m_cmd.c_str();
    }

    bool MySQL::use(const std::string &dbname) {
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

    const char *MySQL::getError() {
        if (!m_mysql) {
            return "";
        }
        const char *str = mysql_error(m_mysql.get());
        if (str) {
            return str;
        }
        return "";
    }

    bool MySQL::isNeedCheck() {
        if ((time(0) - m_lastUseTime) < 5 &&
            !m_hasError) {
            return false;
        }
        return true;
    }

    MySQLTransaction::MySQLTransaction(MySQL::ptr mysql, bool auto_commit)
            : m_mysql(mysql),
              m_autoCommit(auto_commit) {
    }

    MySQLTransaction::ptr MySQLTransaction::Create(MySQL::ptr mysql, bool auto_commit) {
        MySQLTransaction::ptr ret(new MySQLTransaction(mysql, auto_commit));
        if (ret->execute("BEGIN") == 0) {
            return ret;
        }
        return nullptr;
    }

    bool MySQLTransaction::commmit() {
        if (m_isFinished || m_hasError) {
            return !m_hasError;
        }
        int ret = execute("COMMIT");
        if (ret == 0) {
            m_isFinished = true;
        } else {
            m_hasError = true;
        }
        return ret == 0;
    }

    bool MySQLTransaction::rollback() {
        if (m_isFinished) {
            return true;
        }
        int ret = execute("ROLLBACK");
        if (ret == 0) {
            m_isFinished = true;
        } else {
            m_hasError = true;
        }
        return ret == 0;
    }

    MySQLTransaction::~MySQLTransaction() {
        if (m_autoCommit) {
            commmit();
        } else {
            rollback();
        }
    }

    int MySQLTransaction::execute(const char *format, ...) {
        va_list ap;
        va_start(ap, format);
        return execute(format, ap);
    }

    int MySQLTransaction::execute(const char *format, va_list ap) {
        if (m_isFinished) {
            SYLAR_LOG_ERROR(g_logger) << "transaction is finished, format: " << format;
            return -1;
        }
        int ret = m_mysql->execute(format, ap);
        if (ret) {
            m_hasError = true;
        }
        return ret;
    }

    int MySQLTransaction::execute(const std::string &sql) {
        if (m_isFinished) {
            SYLAR_LOG_ERROR(g_logger) << "transaction is finished, sql: " << sql;
            return -1;
        }
        int ret = m_mysql->execute(sql);
        if (ret) {
            m_hasError = true;
        }
        return ret;
    }

    int64_t MySQLTransaction::getLastInsertId() {
        return m_mysql->getLastInsertId();
    }

    std::shared_ptr<MySQL> MySQLTransaction::getMySQL() {
        return m_mysql;
    }

    MySQLManager::MySQLManager()
            : m_maxConn(10) {
        mysql_library_init(0, nullptr, nullptr);
    }

    MySQLManager::~MySQLManager() {
        mysql_library_end();
    }

    MySQL::ptr MySQLManager::get(const std::string &name) {
        MutexType::Lock lock(m_mutex);
        auto it = m_conns.find(name);
        if (it != m_conns.end()) {
            if (!it->second.empty()) {
                MySQL *ret = it->second.front();
                it->second.pop_front();
                lock.unlock();
                if (!ret->isNeedCheck()) {
                    ret->m_lastUseTime = time(0);
                    return MySQL::ptr(ret, std::bind(&MySQLManager::freeMySQL,
                                                     this, name, std::placeholders::_1));
                }
                if (ret->ping()) {
                    ret->m_lastUseTime = time(0);
                    return MySQL::ptr(ret, std::bind(&MySQLManager::freeMySQL,
                                                     this, name, std::placeholders::_1));
                } else {
                    SYLAR_LOG_ERROR(g_logger) << "reconnect: " << name << " fail";
                    return nullptr;
                }
            }
        }

        std::map<std::string, std::map<std::string, std::string> > g_mysql_db;
        auto config = g_mysql_db;
        auto sit = config.find(name);
        std::map<std::string, std::string> args;
        if (sit != config.end()) {
            args = sit->second;
        } else {
            sit = m_dbDefines.find(name);
            if (sit != m_dbDefines.end()) {
                args = sit->second;
            } else {
                return nullptr;
            }
        }
        lock.unlock();
        MySQL *ret = new MySQL(args);
        if (ret->connect()) {
            ret->m_lastUseTime = time(0);
            return MySQL::ptr(ret, std::bind(&MySQLManager::freeMySQL,
                                             this, name, std::placeholders::_1));
        } else {
            delete ret;
            return nullptr;
        }
    }

    void MySQLManager::registerMySQL(const std::string &name, const std::map<std::string, std::string> &params) {
        MutexType::Lock lock(m_mutex);
        m_dbDefines[name] = params;
    }

    void MySQLManager::checkConnection(int sec) {
        time_t now = time(0);
        MutexType::Lock lock(m_mutex);
        for (auto &i : m_conns) {
            for (auto it = i.second.begin(); it != i.second.end();) {
                if ((int) (now - (*it)->m_lastUseTime) >= sec) {
                    auto tmp = *it;
                    i.second.erase(it++);
                    delete tmp;
                } else {
                    ++it;
                }
            }
        }
    }

    int MySQLManager::execute(const std::string &name, const char *format, ...) {
        va_list ap;
        va_start(ap, format);
        int ret = execute(name, format, ap);
        va_end(ap);
        return ret;
    }

    int MySQLManager::execute(const std::string &name, const char *format, va_list ap) {
        auto conn = get(name);
        if (!conn) {
            SYLAR_LOG_ERROR(g_logger) << "MySQLManager::cmd get(" << name
                                      << ") failed format: " << format;
            return -1;
        }
        return conn->execute(format, ap);
    }

    int MySQLManager::execute(const std::string &name, const std::string &sql) {
        auto conn = get(name);
        if (!conn) {
            SYLAR_LOG_ERROR(g_logger) << "MySQLManager::cmd get(" << name
                                      << ") failed sql: " << sql;
            return -1;
        }
        return conn->execute(sql);
    }

    ISQLData::ptr MySQLManager::query(const std::string &name, const char *format, ...) {
        va_list ap;
        va_start(ap, format);
        auto ret = query(name, format, ap);
        va_end(ap);
        return ret;
    }

    ISQLData::ptr MySQLManager::query(const std::string &name, const char *format, va_list ap) {
        auto conn = get(name);
        if (!conn) {
            SYLAR_LOG_ERROR(g_logger) << "MySQLManager::query get(" << name
                                      << ") failed format: " << format;
            return nullptr;
        }
        return conn->query(format, ap);
    }

    ISQLData::ptr MySQLManager::query(const std::string &name, const char *format, const std::string &sql) {
        auto conn = get(name);
        if (!conn) {
            SYLAR_LOG_ERROR(g_logger) << "MySQLManager::query get(" << name
                                      << ") failed sql: " << sql;
            return nullptr;
        }
        return conn->query(sql);
    }

    MySQLTransaction::ptr MySQLManager::openTransaction(const std::string &name, bool auto_commit) {
        auto conn = get(name);
        if (!conn) {
            SYLAR_LOG_ERROR(g_logger) << "MySQLManager::query get(" << name
                                      << ") failed";
            return nullptr;
        }
        return MySQLTransaction::Create(conn, auto_commit);
    }

    void MySQLManager::freeMySQL(const std::string &name, MySQL *m) {
        if (m->m_mysql) {
            MutexType::Lock lock(m_mutex);
            if (m_conns[name].size() < m_maxConn) {
                m_conns[name].push_back(m);
                return;
            }
        }
        delete m;
    }

    // https://blog.csdn.net/a1173356881/article/details/94357853
    MySQLStmt::ptr MySQLStmt::Create(MySQL::ptr db, const std::string& stmt) {
        auto st = mysql_stmt_init(db->getRaw().get());
        if (!st) {
            return nullptr;
        }
        if (mysql_stmt_prepare(st, stmt.c_str(), stmt.size())) { // stmt.c_str = "INSERT INTO %s(col1,col2,col3) VALUES(?,?,?)"
            SYLAR_LOG_ERROR(g_logger) <<  "Create stmt: " << stmt
            << " falied, errno: " << mysql_stmt_errno(st)
            << " strerror: " << mysql_stmt_error(st);
            mysql_stmt_close(st);
            return nullptr;
        }
        int count = mysql_stmt_param_count(st); // 获取sql语言中占位符的个数 3个
        MySQLStmt::ptr ret(new MySQLStmt(db, st));
        ret->m_binds.resize(count);
        memset(&ret->m_binds[0], 0, sizeof(ret->m_binds[0]) * count);
        return ret;
    }

    MySQLStmt::MySQLStmt(MySQL::ptr db, MYSQL_STMT* stmt)
    : m_mysql(db),
    m_stmt(stmt) {
    }

    MySQLStmt::~MySQLStmt() {
        if (!m_stmt) {
            mysql_stmt_close(m_stmt);
        }
        for (const auto& i : m_binds) {
            if (i.buffer_type == MYSQL_TYPE_TIMESTAMP ||
            i.buffer_type == MYSQL_TYPE_DATETIME ||
            i.buffer_type == MYSQL_TYPE_DATE ||
            i.buffer_type == MYSQL_TYPE_TIME) {
                if (i.buffer) {
                    free(i.buffer);
                }
            }
        }
    }

    int MySQLStmt::bind(int idx, int8_t& value) {
        return bindInt8(idx, value);
    }

    int MySQLStmt::bind(int idx, uint8_t& value) {
        return bindUint8(idx, value);
    }

    int MySQLStmt::bind(int idx, int16_t& value) {
        return bindInt16(idx, value);
    }

    int MySQLStmt::bind(int idx, uint16_t& value) {
        return bindUint16(idx, value);
    }

    int MySQLStmt::bind(int idx, int32_t& value) {
        return bindInt32(idx, value);
    }

    int MySQLStmt::bind(int idx, uint32_t& value) {
        return bindUint32(idx, value);
    }

    int MySQLStmt::bind(int idx, int64_t& value) {
        return bindInt64(idx, value);
    }

    int MySQLStmt::bind(int idx, uint64_t& value) {
        return bindUint64(idx, value);
    }

    int MySQLStmt::bind(int idx, float& value) {
        return bindFloat(idx, value);
    }

    int MySQLStmt::bind(int idx, double& value) {
        return bindDouble(idx, value);
    }

    int MySQLStmt::bind(int idx, char* value) {
        return bindString(idx, value);
    }

    int MySQLStmt::bind(int idx, std::string& value) {
        return bindString(idx, value);
    }

    int MySQLStmt::bind(int idx, void* value, int len) {
        return bindBlob(idx, value, len);
    }

    int MySQLStmt::bind(int idx) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_NULL;
        return 0;
    }

    int MySQLStmt::bindInt8(int idx, int8_t& value) { // 第idx个参数 设为int类型 待拿到结果后把值写到value
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_TINY;
        m_binds[idx].buffer = &value;
        m_binds[idx].is_unsigned = false;
        m_binds[idx].buffer_length = sizeof(value);
        return 0;
    }

    int MySQLStmt::bindUint8(int idx, uint8_t& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_TINY;
        m_binds[idx].buffer = &value;
        m_binds[idx].is_unsigned = true;
        m_binds[idx].buffer_length = sizeof(value);
        return 0;
    }

    int MySQLStmt::bindInt16(int idx, int16_t& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_SHORT;
        m_binds[idx].buffer = &value;
        m_binds[idx].is_unsigned = false;
        m_binds[idx].buffer_length = sizeof(value);
        return 0;
    }

    int MySQLStmt::bindUint16(int idx, uint16_t& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_SHORT;
        m_binds[idx].buffer = &value;
        m_binds[idx].is_unsigned = true;
        m_binds[idx].buffer_length = sizeof(value);
        return 0;
    }

    int MySQLStmt::bindInt32(int idx, int32_t& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_LONG;
        m_binds[idx].buffer = &value;
        m_binds[idx].is_unsigned = false;
        m_binds[idx].buffer_length = sizeof(value);
        return 0;
    }

    int MySQLStmt::bindUint32(int idx, uint32_t& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_LONG;
        m_binds[idx].buffer = &value;
        m_binds[idx].is_unsigned = true;
        m_binds[idx].buffer_length = sizeof(value);
        return 0;
    }

    int MySQLStmt::bindInt64(int idx, int64_t& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
        m_binds[idx].buffer = &value;
        m_binds[idx].is_unsigned = false;
        m_binds[idx].buffer_length = sizeof(value);
        return 0;
    }

    int MySQLStmt::bindUint64(int idx, uint64_t& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
        m_binds[idx].buffer = &value;
        m_binds[idx].is_unsigned = true;
        m_binds[idx].buffer_length = sizeof(value);
        return 0;
    }

    int MySQLStmt::bindFloat(int idx, float& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_FLOAT;
        m_binds[idx].buffer = &value;
        m_binds[idx].buffer_length = sizeof(value);
        return 0;
    }

    int MySQLStmt::bindDouble(int idx, double& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_DOUBLE;
        m_binds[idx].buffer = &value;
        m_binds[idx].buffer_length = sizeof(value);
        return 0;
    }

    int MySQLStmt::bindString(int idx, char* value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_STRING;
        m_binds[idx].buffer = value;
        m_binds[idx].buffer_length = strlen(value);
        return 0;
    }

    int MySQLStmt::bindString(int idx, std::string& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_STRING;
        m_binds[idx].buffer = &value[0];
        m_binds[idx].buffer_length = value.size();
        return 0;
    }

    int MySQLStmt::bindBlob(int idx, void* value, int64_t size) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
        m_binds[idx].buffer = value;
        m_binds[idx].buffer_length = size;
        return 0;
    }

    int MySQLStmt::bindBlob(int idx, std::string& value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
        m_binds[idx].buffer = &value[0];
        m_binds[idx].buffer_length = value.size();
        return 0;
    }

    int MySQLStmt::bindTime(int idx, time_t value) {
        idx -= 1;
        m_binds[idx].buffer_type = MYSQL_TYPE_TIMESTAMP;
        MYSQL_TIME* mt = (MYSQL_TIME*)malloc(sizeof(MYSQL_TIME));
        timet_to_mysqltime(value, *mt);
        m_binds[idx].buffer_length = sizeof(MYSQL_TIME);
        return 0;
    }

    int MySQLStmt::bindNull(int idx) {
        return bind(idx);
    }

    int MySQLStmt::execute() {
        mysql_stmt_bind_param(m_stmt, &m_binds[0]);
        return mysql_stmt_execute(m_stmt);
    }

    int64_t MySQLStmt::getLastInsertId() {
        return mysql_stmt_insert_id(m_stmt);
    }

    ISQLData::ptr MySQLStmt::query() {
        mysql_stmt_bind_param(m_stmt, &m_binds[0]);
        return MySQLStmtRes::Create(shared_from_this());
    }

    int MySQLStmt::getErrno() {
        return mysql_stmt_errno(m_stmt);
    }

    std::string MySQLStmt::getErrStr() {
        const char* e = mysql_stmt_error(m_stmt);
        if (e) {
            return e;
        }
        return "";
    }

    int MySQLStmtRes::getRowCount() {
        return mysql_stmt_num_rows(m_stmt->getRaw());
    }

    int MySQLStmtRes::getColumnCount() {
        return mysql_stmt_field_count(m_stmt->getRaw());
    }

    int MySQLStmtRes::getColumnBytes(int idx) {
        return m_datas[idx].length;
    }

    int MySQLStmtRes::getColumnType(int idx) {
        return m_datas[idx].type;
    }

    std::string MySQLStmtRes::getColumName(int idx) {
        return "";
    }

    bool MySQLStmtRes::isNull(int idx) {
        return m_datas[idx].is_null;
    }

    int8_t MySQLStmtRes::getInt8(int idx) {
        return *(int8_t*)m_datas[idx].data;
    }

    uint8_t MySQLStmtRes::getUint8(int idx) {
        return *(uint8_t*)m_datas[idx].data;
    }

    int16_t MySQLStmtRes::getInt16(int idx) {
        return *(int16_t*)m_datas[idx].data;
    }

    uint16_t MySQLStmtRes::getUint16(int idx) {
        return *(uint16_t*)m_datas[idx].data;
    }

    int32_t MySQLStmtRes::getInt32(int idx) {
        return *(int32_t*)m_datas[idx].data;
    }

    uint32_t MySQLStmtRes::getUint32(int idx) {
        return *(uint32_t*)m_datas[idx].data;
    }

    int64_t MySQLStmtRes::getInt64(int idx) {
        return *(int64_t*)m_datas[idx].data;
    }

    uint64_t MySQLStmtRes::getUint64(int idx) {
        return *(uint64_t*)m_datas[idx].data;
    }

    float MySQLStmtRes::getFloat(int idx) {
        return *(float*)m_datas[idx].data;
    }

    double MySQLStmtRes::getDouble(int idx) {
        return *(double*)m_datas[idx].data;
    }

    std::string MySQLStmtRes::getString(int idx) {
        return std::string(m_datas[idx].data, m_datas[idx].length);
    }

    std::string MySQLStmtRes::getBlob(int idx) {
        return std::string(m_datas[idx].data, m_datas[idx].length);
    }

    time_t MySQLStmtRes::getTime(int idx) {
        MYSQL_TIME* v = (MYSQL_TIME*)m_datas[idx].data;
        time_t ts = 0;
        mysqltime_to_timet(*v, ts);
        return ts;
    }

    bool MySQLStmtRes::next() {
        return !mysql_stmt_fetch(m_stmt->getRaw());
    }

    MySQLStmtRes::Data::Data()
    : is_null(0),
    error(false),
    type(),
    length(0),
    data_length(0),
    data(nullptr) {
    }

    MySQLStmtRes::Data::~Data() {
        if (data) {
            delete [] data;
        }
    }

    void MySQLStmtRes::Data::alloc(size_t size) {
        if (data) {
            delete [] data;
        }
        data = new char[size]();
        length = size;
        data_length = size;
    }

    MySQLStmtRes::MySQLStmtRes(std::shared_ptr<MySQLStmt> stmt, int eno, const std::string& err)
    : m_errno(eno),
    m_strerr(err),
    m_stmt(stmt) {
    }

    MySQLStmtRes::ptr MySQLStmtRes::Create(std::shared_ptr<MySQLStmt> stmt) {
        int eno = mysql_stmt_errno(stmt->getRaw());
        const char* errstr = mysql_stmt_error(stmt->getRaw());
        MySQLStmtRes::ptr ret(new MySQLStmtRes(stmt, eno, errstr));
        if (eno) {
            return ret;
        }
        MYSQL_RES* res = mysql_stmt_result_metadata(stmt->getRaw()); // 获取结果集元信息
        if (!res) {
            return MySQLStmtRes::ptr(new MySQLStmtRes(stmt, stmt->getErrno(),
                    stmt->getErrStr()));
        }
        int num = mysql_num_fields(res);
        MYSQL_FIELD* fields = mysql_fetch_field(res);

        ret->m_binds.resize(num);
        memset(&ret->m_binds[0], 0, sizeof(ret->m_binds[0]) * num);
        ret->m_datas.resize(num);

        for (int i = 0; i < num; i++) {
            ret->m_datas[i].type = fields[i].type;
            switch (fields[i].type) {
#define XX(m, t) \
                case m: \
                ret->m_datas[i].alloc(sizeof(t)); \
                break;
                XX(MYSQL_TYPE_TINY, int8_t);
                XX(MYSQL_TYPE_SHORT, int16_t);
                XX(MYSQL_TYPE_LONG, int32_t);
                XX(MYSQL_TYPE_LONGLONG, int64_t);
                XX(MYSQL_TYPE_FLOAT, float);
                XX(MYSQL_TYPE_DOUBLE, double);
                XX(MYSQL_TYPE_TIMESTAMP, MYSQL_TIME);
                XX(MYSQL_TYPE_DATETIME, MYSQL_TIME);
                XX(MYSQL_TYPE_DATE, MYSQL_TIME);
                XX(MYSQL_TYPE_TIME, MYSQL_TIME);
#undef XX
                default:
                    ret->m_datas[i].alloc(fields[i].length);
                    break;
            }
            ret->m_binds[i].buffer_type = ret->m_datas[i].type;
            ret->m_binds[i].buffer = ret->m_datas[i].data;
            ret->m_binds[i].buffer_length = ret->m_datas[i].data_length;
            ret->m_binds[i].length = &ret->m_datas[i].length;
            ret->m_binds[i].is_null = &ret->m_datas[i].is_null;
            ret->m_binds[i].error = &ret->m_datas[i].error;
        }

        if (mysql_stmt_bind_result(stmt->getRaw(), &ret->m_binds[0])) {
            return MySQLStmtRes::ptr(new MySQLStmtRes(stmt, stmt->getErrno(),
                    stmt->getErrStr()));
        }

        stmt->execute();

        if (mysql_stmt_store_result(stmt->getRaw())) {
            return MySQLStmtRes::ptr(new MySQLStmtRes(stmt, stmt->getErrno(),
                    stmt->getErrStr()));
        }

        return ret;
    }

    MySQLStmtRes::~MySQLStmtRes() {
        if (!m_errno) {
            mysql_stmt_free_result(m_stmt->getRaw());
        }
    }

};

