#include <iostream>
#include "log.hh"

int main(int argc, char** argv) {
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


    SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG) << "hello mike"; // just flush into event's ss
    SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, "hello %s", "dog"); // same as up

    return 0;
}
