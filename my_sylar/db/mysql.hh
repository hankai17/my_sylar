#ifndef __MYSQL_HH__
#define __MYSQL_HH__

#include <memory>
#include <functional>
#include <mysql/mysql.h>
#include <map>
#include <vector>
#include "my_sylar/thread.hh"

namespace sylar {
    class MySQL;

    class ISQLUpdate { // update请求封装
    public:
        typedef std::shared_ptr<ISQLUpdate> ptr;
        virtual ~ISQLUpdate() {};
        virtual int execute(const char *format, ...) = 0;
        virtual int execute(const std::string &sql) = 0;
        virtual int64_t getLastInsertId() = 0;
    };

    class ISQLData { // 操作res结果的封装
    public:
        typedef std::shared_ptr<ISQLData> ptr;
        virtual ~ISQLData() {}

        virtual int getErrno() const = 0;
        virtual const std::string& getErrStr() const = 0;
        virtual int getRowCount() = 0;
        virtual int getColumnCount() = 0;
        virtual int getColumnBytes(int idx) = 0;
        virtual int getColumnType(int idx) = 0;
        virtual std::string getColumName(int idx) = 0;

        virtual bool isNull(int idx) = 0;
        virtual int8_t getInt8(int idx) = 0;
        virtual uint8_t getUint8(int idx) = 0;
        virtual int16_t getInt16(int idx) = 0;
        virtual uint16_t getUint16(int idx) = 0;
        virtual int32_t getInt32(int idx) = 0;
        virtual uint32_t getUint32(int idx) = 0;
        virtual int64_t getInt64(int idx) = 0;
        virtual uint64_t getUint64(int idx) = 0;
        virtual float getFloat(int idx) = 0;
        virtual double getDouble(int idx) = 0;
        virtual std::string getString(int idx) = 0;
        virtual std::string getBlob(int idx) = 0;
        virtual time_t getTime(int idx) = 0;
        virtual bool next() = 0; // 获取一行数据
    };

    class ISQLQuery {
    public:
        virtual ~ISQLQuery() {}
        virtual ISQLData::ptr query(const char* format, ...) = 0;
        virtual ISQLData::ptr query(const std::string& sql) = 0;
    };

    class IStmt {
    public:
        typedef std::shared_ptr<IStmt> ptr;
        virtual ~IStmt() {}
        virtual int bindInt8(int idx, int8_t& value) = 0;
        virtual int bindUint8(int idx, uint8_t& value) = 0;
        virtual int bindInt16(int idx, int16_t& value) = 0;
        virtual int bindUint16(int idx, uint16_t& value) = 0;
        virtual int bindInt32(int idx, int32_t& value) = 0;
        virtual int bindUint32(int idx, uint32_t& value) = 0;
        virtual int bindInt64(int idx, int64_t& value) = 0;
        virtual int bindUint64(int idx, uint64_t& value) = 0;
        virtual int bindFloat(int idx, float& value) = 0;
        virtual int bindDouble(int idx, double& value) = 0;
        virtual int bindString(int idx, char* value) = 0;
        virtual int bindString(int idx, std::string& value) = 0;
        virtual int bindBlob(int idx, void* value, int64_t size) = 0;
        virtual int bindBlob(int idx, std::string& value) = 0;
        virtual int bindTime(int idx, time_t value) = 0;
        virtual int bindNull(int idx) = 0;

        virtual int execute() = 0;
        virtual int64_t getLastInsertId() = 0;
        virtual ISQLData::ptr query() = 0;
        virtual int getErrno() = 0;
        virtual std::string getErrStr() = 0;
    };

    struct MySQLTime {
        MySQLTime(time_t t)
        : ts(t) {}
        time_t ts;
    };


    bool mysqltime_to_timet(const MYSQL_TIME& mt, time_t& ts);
    bool timet_to_mysqltime(const time_t& ts, MYSQL_TIME& mt);

    class MySQLRes : public ISQLData { // 封装sql结果
    public:
        typedef std::shared_ptr<MySQLRes> ptr;
        typedef std::function<bool(MYSQL_ROW row,
                                   int field_count, int row_no)> data_cb;

        MySQLRes(MYSQL_RES *res, int eno, const char *estr);
        MYSQL_RES* get() const { return m_data.get(); }
        bool foreach(data_cb cb);

        int getErrno() const override { return m_errno; }
        const std::string& getErrStr() const override { return m_strerr; }
        int getRowCount() override;
        int getColumnCount() override;
        int getColumnBytes(int idx) override;
        int getColumnType(int idx) override;
        std::string getColumName(int idx) override;

        bool isNull(int idx) override;
        int8_t getInt8(int idx) override; // 对指定列强转
        uint8_t getUint8(int idx) override;
        int16_t getInt16(int idx) override;
        uint16_t getUint16(int idx) override;
        int32_t getInt32(int idx) override;
        uint32_t getUint32(int idx) override;
        int64_t getInt64(int idx) override;
        uint64_t getUint64(int idx) override;
        float getFloat(int idx) override;
        double getDouble(int idx) override;
        std::string getString(int idx) override;
        std::string getBlob(int idx) override;
        time_t getTime(int idx) override;
        bool next() override; // 取下一行

    private:
        int                         m_errno;
        std::string                 m_strerr;
        MYSQL_ROW                   m_cur;
        unsigned long*              m_curLength;
        std::shared_ptr<MYSQL_RES>  m_data; // 字符串指针数组
    };

    class MySQLStmt;
    class MySQLStmtRes : public ISQLData {
    public:
        typedef std::shared_ptr<MySQLStmtRes> ptr;
        friend class MySQLStmt;
        static MySQLStmtRes::ptr Create(std::shared_ptr<MySQLStmt> stmt);
        ~MySQLStmtRes();

        int getErrno() const override { return m_errno; }
        const std::string& getErrStr() const override { return m_strerr; }
        int getRowCount() override;
        int getColumnCount() override;
        int getColumnBytes(int idx) override;
        int getColumnType(int idx) override;
        std::string getColumName(int idx) override;

        bool isNull(int idx) override;
        int8_t getInt8(int idx) override;
        uint8_t getUint8(int idx) override;
        int16_t getInt16(int idx) override;
        uint16_t getUint16(int idx) override;
        int32_t getInt32(int idx) override;
        uint32_t getUint32(int idx) override;
        int64_t getInt64(int idx) override;
        uint64_t getUint64(int idx) override;
        float getFloat(int idx) override;
        double getDouble(int idx) override;
        std::string getString(int idx) override;
        std::string getBlob(int idx) override;
        time_t getTime(int idx) override;
        bool next() override;

    private:
        MySQLStmtRes(std::shared_ptr<MySQLStmt> stmt, int eno, const std::string& err);
        struct Data {
            Data();
            ~Data();
            void alloc(size_t size);
            my_bool             is_null;
            my_bool             error;
            enum_field_types    type;
            unsigned long       length;
            int32_t             data_length;
            char*               data;
        };

    private:
        int             m_errno;
        std::string     m_strerr;
        std::shared_ptr<MySQLStmt>  m_stmt;
        std::vector<MYSQL_BIND>     m_binds;
        std::vector<Data>           m_datas;
    };

    class MySQLManager;
    class MySQL : public ISQLUpdate, public ISQLQuery,
            public std::enable_shared_from_this<MySQL> {
    public:
        typedef std::shared_ptr<MySQL> ptr;
        friend MySQLManager;

        MySQL(const std::map<std::string, std::string> &args); // 传入host user pwd dbname
        bool connect();
        bool ping();

        // ISQLUpdate
        int execute(const char *format, ...) override; // ::mysql_query
        int execute(const std::string &sql) override;
        int execute(const char* format, va_list ap);
        int64_t getLastInsertId() override;

        // ISQLQuery
        ISQLData::ptr query(const char* format, ...) override; // ::mysql_query & mysql_store_result
        ISQLData::ptr query(const std::string& sql) override;
        ISQLData::ptr query(const char* format, va_list ap);

        IStmt::ptr prepare(const std::string& stmt); // 支持普通模式 也支持预处理功能
        template <typename... T>
        int execStmt(const char* stmt, T&&... args);

        template <typename... T>
        ISQLData::ptr queryStmt(const char* stmt, T**... args); // 下面

        uint64_t getAffectedRows();
        const char* cmd();
        bool use(const std::string &dbname);
        const char* getError();
        std::shared_ptr<MYSQL> getRaw() { return m_mysql; }
        std::shared_ptr<MYSQL> getMySQL() { return m_mysql; };

    private:
        bool isNeedCheck();

    private:
        std::map<std::string, std::string>  m_params;
        std::shared_ptr<MYSQL>              m_mysql; // handler
        std::string                         m_cmd;
        std::string                         m_dbname;
        uint64_t                            m_lastUseTime;
        bool                                m_hasError;
    };

    class MySQLTransaction : public ISQLUpdate { // transaction是对mysql的再次封装 // 在析构中调commit/rollback
    public:
        typedef std::shared_ptr<MySQLTransaction> ptr;
        static MySQLTransaction::ptr Create(MySQL::ptr mysql, bool auto_commit);
        ~MySQLTransaction();
        bool commmit();
        bool rollback();
        virtual int execute(const char *format, ...) override;
        virtual int execute(const std::string &sql) override;
        int execute(const char *format, va_list ap);
        int64_t getLastInsertId() override;

        std::shared_ptr<MySQL> getMySQL();
        bool isAutoCommit() const { return m_autoCommit; }
        bool isFinished() const { return m_isFinished; }
        bool isError() const { return m_hasError; }

    private:
        MySQLTransaction(MySQL::ptr mysql, bool auto_commit);

    private:
        MySQL::ptr m_mysql;
        bool m_autoCommit;
        bool m_isFinished;
        bool m_hasError;
    };

    class MySQLManager { // 连接池 结合配置一起用?
    public:
        typedef std::shared_ptr<MySQLManager> ptr;
        typedef sylar::Mutex MutexType;
        MySQLManager();
        ~MySQLManager();
        MySQL::ptr get(const std::string &name);
        void registerMySQL(const std::string &name, const std::map<std::string, std::string> &params);
        void checkConnection(int sec = 30);
        uint32_t getMaxConn() const { return m_maxConn; }
        void setMaxConn(uint32_t v) { m_maxConn = v; }
        int execute(const std::string &name, const char *format, ...);
        int execute(const std::string &name, const char *format, va_list ap);
        int execute(const std::string &name, const std::string &sql);
        ISQLData::ptr query(const std::string &name, const char *format, ...);
        ISQLData::ptr query(const std::string &name, const char *format, va_list ap);
        ISQLData::ptr query(const std::string &name, const char *format, const std::string &sql);
        MySQLTransaction::ptr openTransaction(const std::string &name, bool auto_commit);

    private:
        void freeMySQL(const std::string &name, MySQL *m);

    private:
        uint32_t m_maxConn;
        MutexType m_mutex;
        std::map<std::string, std::list<MySQL *> > m_conns;
        std::map<std::string, std::map<std::string, std::string> > m_dbDefines;
    };

    class MySQLStmt : public IStmt,
        public std::enable_shared_from_this<MySQLStmt> {
    public:
        typedef std::shared_ptr<MySQLStmt> ptr;
        static MySQLStmt::ptr Create(MySQL::ptr db, const std::string& stmt);
        ~MySQLStmt();
        int bindInt8(int idx, int8_t& value) override;
        int bindUint8(int idx, uint8_t& value) override;
        int bindInt16(int idx, int16_t& value) override;
        int bindUint16(int idx, uint16_t& value) override;
        int bindInt32(int idx, int32_t& value) override;
        int bindUint32(int idx, uint32_t& value) override;
        int bindInt64(int idx, int64_t& value) override;
        int bindUint64(int idx, uint64_t& value) override;
        int bindFloat(int idx, float& value) override;
        int bindDouble(int idx, double& value) override;
        int bindString(int idx, char* value) override;
        int bindString(int idx, std::string& value) override;
        int bindBlob(int idx, void* value, int64_t size) override;
        int bindBlob(int idx, std::string& value) override;
        int bindTime(int idx, time_t value) override;
        int bindNull(int idx) override;

        int execute() override;
        int64_t getLastInsertId() override;
        ISQLData::ptr query() override;
        int getErrno() override;
        std::string getErrStr() override;

        int bind(int idx, int8_t& value);
        int bind(int idx, uint8_t& value);
        int bind(int idx, int16_t& value);
        int bind(int idx, uint16_t& value);
        int bind(int idx, int32_t& value);
        int bind(int idx, uint32_t& value);
        int bind(int idx, int64_t& value);
        int bind(int idx, uint64_t& value);
        int bind(int idx, float& value);
        int bind(int idx, double& value);
        int bind(int idx, char* value);
        int bind(int idx, std::string& value);
        int bind(int idx, void* value, int len);
        int bind(int idx);

        MYSQL_STMT* getRaw() const { return m_stmt; }
    private:
        MySQLStmt(MySQL::ptr db, MYSQL_STMT* stmt);

    private:
        MySQL::ptr          m_mysql;
        MYSQL_STMT*         m_stmt;
        std::vector<MYSQL_BIND> m_binds; // n个上层参数 n由传递过来的stmt.c_str中参数个数决定
    };

    namespace {
        template <size_t N, typename... Args>
        struct MySQLBinder { // 递归结束
            static int Bind(std::shared_ptr<MySQLStmt> stmt) { return 0; }
        };

        template <typename... Args>
        int bindX(MySQLStmt::ptr stmt, Args&... args) { // 1开始调用
            return MySQLBinder<1, Args...>::Bind(stmt, args...);
        }
    }

    template <typename... T>
    int MySQL::execStmt(const char* stmt, T&&... args) {
        auto st = MySQLStmt::Create(shared_from_this(), stmt);
        if (!st) {
            return -1;
        }
        int ret = bindX(st, args...);
        if (ret != 0) {
            return ret;
        }
        return st->execute();
    }

    template <typename... T>
    ISQLData::ptr MySQL::queryStmt(const char* stmt, T**... args) { // queryStmp(stmt, char* str, int a, float f) // 应该是这样用
        auto st = MySQLStmt::Create(shared_from_this(), stmt); // 0开始调用
        if (!st) {
            return nullptr;
        }
        int ret = bindX(st, args...);
        if (ret != 0) {
            return nullptr;
        }
        return st->query();
    }

    namespace {
        template <size_t N, typename Head, typename... Tail>
        struct MySQLBinder<N, Head, Tail...> { // 递归结束
            static int Bind(MySQLStmt::ptr stmt, const Head&, Tail&...) {
                static_assert(sizeof...(Tail) < 0, "invalid type");
                return 0;
            }
        };

        ///////////////////////////////////////////////////   2 偏特化
#define XX(type, type2) \
template<size_t N, typename... Tail> \
struct MySQLBinder<N, type, Tail...> { \
    static int Bind(MySQLStmt::ptr stmt, type2 value, Tail&... tail) { \
        int ret = stmt->bind(N, value); \
        if (ret != 0) { \
            return ret; \
        } \
        return MySQLBinder<N + 1, Tail...>::Bind(stmt, tail...); \
    } \
};

    XX(char*, char*);
    XX(const char*, char*);
    XX(std::string, std::string&);
    XX(int8_t, int8_t&);
    XX(uint8_t, uint8_t&);
    XX(int16_t, int16_t&);
    XX(uint16_t, uint16_t&);
    XX(int32_t, int32_t&);
    XX(uint32_t, uint32_t&);
    XX(int64_t, int64_t&);
    XX(uint64_t, uint64_t&);
    XX(float, float&);
    XX(double, double&);
#undef XX
    }
};

#endif

