#include <iostream>
#include "log.hh"

int main(int argc, char** argv) {
    sylar::Logger::ptr logger(new sylar::Logger);

    sylar::LogEvent::ptr event(new sylar::LogEvent(time(0), sylar::LogLevel::DEBUG, __FILE__, __LINE__, 1, 1));
    event->getSS() << "hello sylar log";

    sylar::LogAppender::ptr apr(new sylar::StdoutLogAppender);
    apr->setLevel(sylar::LogLevel::DEBUG);
    logger->addAppender(apr);

    logger->log(sylar::LogLevel::DEBUG, event);

    logger->delAppender(apr);

    return 0;
}
