#ifndef __MYSQL_HH__
#define __MYSQL_HH__

#include <memory>
#include <functional>
#include <mysql/mysql.h>

namespace sylar {
  class IMySQLUpdate {
    public:
      typedef std::shared_ptr<IMySQLUpdate> ptr;
      virtual ~IMySQLUpdate();
      virtual int cmd(const char* format, ...) = 0;
      virtual int cmd(const char* format, va_list ap) = 0;
      virtual int cmd(const std::string& sql) = 0;
      virtual std::shared_ptr<MySQL> getMySQL() = 0;
  };

  class MySQLRes {
    public:
      typedef std::shared_ptr<MySQLRes> ptr;
      typedef std::function<bool(MYSQL_ROW row,
            int field_count, int row_no)> data_cb;
      MySQLRes(MYSQL_RES* res, int eno, const char* estr);
      MYSQL_RES* get() const { return m_data.get(); }
      uint64_t getRows();
      uint64_t getFields();
      int getErrno() const { return m_errno; }
      const std::string& getStrErr() const { return m_strerr; }
      bool foreach(data_cb cb);

    private:
      int                           m_errno;
      std::shared_ptr<MYSQL_RES>    m_data;
      std::string                   m_strerr;
  };

  class MySQL : public IMySQLUpdate {
    public:
      typedef std::shared_ptr<MySQL> ptr;
      MySQL(const std::map<std::string, std::string>& args);
      bool connect();
      bool ping();
      virtual int cmd(const char* format, ...) override; // ::mysql_query
      virtual int cmd(const char* format, va_list ap) override;
      virtual int cmd(const std::string& sql) override;
      virtual std::shared_ptr<MySQL> getMySQL() override;
      uint64_t getAffectedRows();

      MySQLRes::ptr query(const char* format, ...); // ::mysql_query & mysql_store_result
      MySQLRes::ptr query(const char* format, va_list ap);
      MySQLRes::ptr query(const std::string& sql);

      const char* cmd();
      bool use(const std::string& dbname);
      const char* getError();
      uint64_t getInsertId();

    private:
      bool isNeedCheck();

    private:
      std::map<std::string, std::string>    m_params;
      std::shared_ptr<MYSQL>                m_mysql;
      std::string                           m_cmd;
      std::string                           m_dbname;
      uint64_t                              m_lastUseTime;
      bool                                  m_hasError;
  };

  class 

};

#endif
