#include <iostream>
#include "log.hh"

/*
int main1(int argc, char** argv) {
    //sylar::Logger::ptr logger(new sylar::Logger);
    sylar::Logger::ptr logger = sylar::Logger::getLoggerInstance();

    sylar::LogEvent::ptr event(new sylar::LogEvent(time(0), sylar::LogLevel::DEBUG, __FILE__, __LINE__, 1, 1));
    event->getSS() << "hello sylar log";

    sylar::LogAppender::ptr apr(new sylar::StdoutLogAppender);
    apr->setLevel(sylar::LogLevel::DEBUG);
    logger->addAppender(apr);
    logger->log(sylar::LogLevel::DEBUG, event);
    logger->delAppender(apr);

    sylar::LogAppender::ptr apr1(new sylar::FileLogAppender("./1.log"));
    apr1->setLevel(sylar::LogLevel::DEBUG);
    logger->addAppender(apr1);
    logger->log(sylar::LogLevel::DEBUG, event);
    logger->delAppender(apr1);

    sylar::LogAppender::ptr apr2(new sylar::FileLogAppender("./1.log"));
    apr2->setLevel(sylar::LogLevel::DEBUG);
    sylar::LogFormatter::ptr lf(new sylar::LogFormatter("%d%T%p%T%m%n")); // ofstream so strange!!!
    apr2->setFormatter(lf);
    logger->addAppender(apr2);
    logger->log(sylar::LogLevel::DEBUG, event);
    logger->delAppender(apr2);
    // where to write then what formatter; hand real log event to logger

    sylar::LogAppender::ptr apr3(new sylar::StdoutLogAppender);
    apr3->setLevel(sylar::LogLevel::DEBUG);
    logger->addAppender(apr3);

    SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG) << "hello mike";
    SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG) << "hello mike";
    SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG) << "hello mike";
    SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, "hello %s", "dog");

    logger->delAppender(apr3);
    return 0;
}
 */

int main() {
    sylar::Logger::ptr logger = SYLAR_LOG_ROOT();
    logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));
    SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "hello world";
    return 0;
}
