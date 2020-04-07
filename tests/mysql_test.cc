#include "log.hh"
#include "iomanager.hh"
#include "db/mysql.hh"
#include <map>
#include <string>
#include <iostream>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();


bool show_sql_result(char** row, int field_count, int row_no) {
    //SYLAR_LOG_DEBUG(g_logger) << *row << " " << field_count << " " << row_no;
    for (int i = 0; i < field_count; i++) {
        std::cout << row[i] << " ";
    }
    std::cout << std::endl;
    return true;
}

void mysql_test() {
    std::map<std::string, std::string> params;
    params["host"] = "127.0.0.1";
    params["user"] = "root";
    params["passwd"] = "123";
    params["dbname"] = "bjxw_log";

    sylar::MySQL::ptr mysql(new sylar::MySQL(params));
    if (!mysql->connect()) {
        SYLAR_LOG_DEBUG(g_logger) << "connect failed";
        return;
    }
    SYLAR_LOG_DEBUG(g_logger) << "connect success";

    if (!mysql->ping()) {
        SYLAR_LOG_DEBUG(g_logger) << "ping failed";
        return;
    }
    SYLAR_LOG_DEBUG(g_logger) << "ping success";

    //int ret = mysql->cmd("show tables");
    auto ret = mysql->query("show tables");
    SYLAR_LOG_DEBUG(g_logger) << "cmd ret: " << ret << " "
    << mysql->getError();
    //ret->foreach(show_sql_result);

    sylar::MySQLRes::ptr ret1 = std::dynamic_pointer_cast<sylar::MySQLRes>(mysql->query("select * from query"));
    ret1->foreach(show_sql_result);

}

int main() {
    sylar::IOManager iom(1, false, "io");
    iom.schedule(mysql_test);
    iom.stop();
    return 0;
}