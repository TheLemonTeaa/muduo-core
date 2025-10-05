#include <iostream>

#include "Logger.h"
#include "Timestamp.h"

// 获取日志的唯一实例 单例
Logger& Logger::instance() {
    static Logger logger; // 局部静态变量
    return logger;
}

// 设置日志级别
void Logger::setLogLevel(int level) {
    logLevel_ = level;
}

// 记录日志 [级别] 时间 : 信息
void Logger::log(std::string msg) {
    std::string pre = "";
    switch (logLevel_) {
        case INFO:
            pre = "[INFO] ";
            break;
        case ERROR:
            pre = "[ERROR] ";
            break;
        case FATAL:
            pre = "[FATAL] ";
            break;
        case DEBUG:
            pre = "[DEBUG] ";
            break;
        default:
            break;
    }
    // 打印时间戳和日志信息
    std::cout << pre << Timestamp::now().toString() << " : " << msg << std::endl;
}