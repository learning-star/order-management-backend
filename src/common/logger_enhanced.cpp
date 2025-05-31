#include "common/logger_enhanced.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>
#include <sys/stat.h>
#include <filesystem>
#include <cstdio>
#include <thread>
#include <unistd.h>

EnhancedLogger::EnhancedLogger() 
    : currentLevel(LogLevel::INFO), consoleOutput(true), colorOutput(true),
      logDirectory("logs"), logBaseName("app"), processName("Unknown") {
    // 确保日志目录存在
    createLogDirectory(logDirectory);
}

EnhancedLogger::~EnhancedLogger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

EnhancedLogger& EnhancedLogger::getInstance() {
    static EnhancedLogger instance;
    return instance;
}

void EnhancedLogger::setLogLevel(LogLevel level) {
    currentLevel = level;
}

void EnhancedLogger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    if (logFile.is_open()) {
        logFile.close();
    }
    
    // 解析文件名，提取目录和基本文件名
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        logDirectory = filename.substr(0, lastSlash);
        logBaseName = filename.substr(lastSlash + 1);
        
        // 如果基本文件名包含扩展名，去掉扩展名
        size_t lastDot = logBaseName.find_last_of(".");
        if (lastDot != std::string::npos) {
            logBaseName = logBaseName.substr(0, lastDot);
        }
    } else {
        logBaseName = filename;
        
        // 如果基本文件名包含扩展名，去掉扩展名
        size_t lastDot = logBaseName.find_last_of(".");
        if (lastDot != std::string::npos) {
            logBaseName = logBaseName.substr(0, lastDot);
        }
    }
    
    // 确保日志目录存在
    createLogDirectory(logDirectory);
    
    // 生成日志文件名
    currentLogFilename = generateLogFilename(logBaseName);
    
    // 打开日志文件
    logFile.open(currentLogFilename, std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "无法打开日志文件: " << currentLogFilename << std::endl;
    } else {
        std::cout << "[" << processName << "] 日志将写入: " << currentLogFilename << std::endl;
    }
}

void EnhancedLogger::setProcessName(const std::string& name) {
    processName = name;
}

void EnhancedLogger::enableConsoleOutput(bool enable) {
    consoleOutput = enable;
}

void EnhancedLogger::enableColorOutput(bool enable) {
    colorOutput = enable;
}

void EnhancedLogger::debug(const std::string& message, const LogContext& context) {
    log(LogLevel::DEBUG, message, context);
}

void EnhancedLogger::info(const std::string& message, const LogContext& context) {
    log(LogLevel::INFO, message, context);
}

void EnhancedLogger::warning(const std::string& message, const LogContext& context) {
    log(LogLevel::WARNING, message, context);
}

void EnhancedLogger::error(const std::string& message, const LogContext& context) {
    log(LogLevel::ERROR, message, context);
}

void EnhancedLogger::fatal(const std::string& message, const LogContext& context) {
    log(LogLevel::FATAL, message, context);
}

void EnhancedLogger::logRequest(const std::string& requestId, const std::string& method, 
                               const std::string& path, const std::string& clientIp) {
    LogContext context(requestId, clientIp);
    std::stringstream ss;
    ss << "🌐 HTTP请求 [" << method << "] " << path;
    if (!clientIp.empty()) {
        ss << " 来自 " << clientIp;
    }
    info(ss.str(), context);
}

void EnhancedLogger::logResponse(const std::string& requestId, int statusCode, 
                                const std::string& message, double responseTime) {
    LogContext context(requestId);
    std::stringstream ss;
    ss << "📤 HTTP响应 [" << statusCode << "]";
    if (!message.empty()) {
        ss << " " << message;
    }
    if (responseTime > 0) {
        ss << " (" << std::fixed << std::setprecision(2) << responseTime << "ms)";
    }
    
    if (statusCode >= 400) {
        error(ss.str(), context);
    } else {
        info(ss.str(), context);
    }
}

void EnhancedLogger::logApiCall(const std::string& requestId, const std::string& apiType,
                               const std::string& operation, const std::string& params) {
    LogContext context(requestId, "", "", operation);
    std::stringstream ss;
    ss << "🔗 API调用 [" << apiType << "." << operation << "]";
    if (!params.empty()) {
        ss << " 参数: " << params;
    }
    debug(ss.str(), context);
}

void EnhancedLogger::logDatabase(const std::string& requestId, const std::string& operation,
                                const std::string& table, const std::string& query, double execTime) {
    LogContext context(requestId);
    std::stringstream ss;
    ss << "🗄️  数据库操作 [" << operation << "] 表: " << table;
    if (execTime > 0) {
        ss << " (" << std::fixed << std::setprecision(2) << execTime << "ms)";
    }
    if (!query.empty() && query.length() < 200) {
        ss << " SQL: " << query;
    }
    debug(ss.str(), context);
}

void EnhancedLogger::logError(const std::string& requestId, const std::string& errorType,
                             const std::string& errorMessage, const std::string& stackTrace) {
    LogContext context(requestId);
    std::stringstream ss;
    ss << "❌ 错误 [" << errorType << "] " << errorMessage;
    error(ss.str(), context);
    
    if (!stackTrace.empty()) {
        error("堆栈跟踪: " + stackTrace, context);
    }
}

void EnhancedLogger::logPerformance(const std::string& operation, double duration, 
                                   const std::string& details) {
    std::stringstream ss;
    ss << "⚡ 性能监控 [" << operation << "] " 
       << std::fixed << std::setprecision(2) << duration << "ms";
    if (!details.empty()) {
        ss << " " << details;
    }
    
    if (duration > 1000) {  // 超过1秒警告
        warning(ss.str());
    } else {
        debug(ss.str());
    }
}

void EnhancedLogger::logSystem(const std::string& component, const std::string& event,
                              const std::string& details) {
    std::stringstream ss;
    ss << "🔧 系统事件 [" << component << "] " << event;
    if (!details.empty()) {
        ss << " " << details;
    }
    info(ss.str());
}

void EnhancedLogger::log(LogLevel level, const std::string& message, const LogContext& context) {
    if (level < currentLevel) {
        return;
    }
    
    std::string formattedMessage = formatLogMessage(level, message, context);
    
    std::lock_guard<std::mutex> lock(logMutex);
    
    // 输出到控制台
    if (consoleOutput) {
        std::string consoleMessage = formattedMessage;
        if (colorOutput) {
            consoleMessage = levelToColorString(level) + formattedMessage + "\033[0m";
        }
        
        if (level == LogLevel::ERROR || level == LogLevel::FATAL) {
            std::cerr << consoleMessage << std::endl;
        } else {
            std::cout << consoleMessage << std::endl;
        }
    }
    
    // 输出到文件
    if (logFile.is_open()) {
        logFile << formattedMessage << std::endl;
        logFile.flush();
        
        // 检查日志文件大小，如果需要则轮转
        checkAndRotateLogFile();
    } else if (!currentLogFilename.empty()) {
        // 如果日志文件未打开但有文件名，尝试重新打开
        logFile.open(currentLogFilename, std::ios::app);
        if (logFile.is_open()) {
            logFile << formattedMessage << std::endl;
            logFile.flush();
        }
    }
}

std::string EnhancedLogger::formatLogMessage(LogLevel level, const std::string& message, const LogContext& context) const {
    std::stringstream ss;
    
    // 时间戳
    ss << getCurrentTime();
    
    // 进程名和线程ID
    ss << " [" << processName << "/" << getThreadId() << "]";
    
    // 日志级别
    ss << " [" << std::setw(7) << std::left << levelToString(level) << "]";
    
    // 请求ID（如果有）
    if (!context.requestId.empty()) {
        ss << " [ReqID:" << context.requestId << "]";
    }
    
    // 客户端IP（如果有）
    if (!context.clientIp.empty()) {
        ss << " [IP:" << context.clientIp << "]";
    }
    
    // 用户ID（如果有）
    if (!context.userId.empty()) {
        ss << " [User:" << context.userId << "]";
    }
    
    // 操作类型（如果有）
    if (!context.operation.empty()) {
        ss << " [Op:" << context.operation << "]";
    }
    
    // 消息内容
    ss << " " << message;
    
    return ss.str();
}

std::string EnhancedLogger::getCurrentTime() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string EnhancedLogger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:                return "UNKNOWN";
    }
}

std::string EnhancedLogger::levelToColorString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return "\033[36m";  // 青色
        case LogLevel::INFO:    return "\033[32m";  // 绿色
        case LogLevel::WARNING: return "\033[33m";  // 黄色
        case LogLevel::ERROR:   return "\033[31m";  // 红色
        case LogLevel::FATAL:   return "\033[35m";  // 紫色
        default:                return "\033[0m";   // 默认色
    }
}

std::string EnhancedLogger::getThreadId() const {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::string threadId = ss.str();
    
    // 只保留后6位以简化显示
    if (threadId.length() > 6) {
        threadId = threadId.substr(threadId.length() - 6);
    }
    
    return threadId;
}

bool EnhancedLogger::checkAndRotateLogFile() {
    if (!logFile.is_open() || currentLogFilename.empty()) {
        return false;
    }
    
    // 获取当前文件大小
    logFile.flush();
    struct stat fileStat;
    if (stat(currentLogFilename.c_str(), &fileStat) != 0) {
        return false;
    }
    
    // 检查文件大小是否超过限制
    if (static_cast<size_t>(fileStat.st_size) >= maxLogFileSize) {
        // 关闭当前日志文件
        logFile.close();
        
        // 生成新的日志文件名
        currentLogFilename = generateLogFilename(logBaseName);
        
        // 打开新的日志文件
        logFile.open(currentLogFilename, std::ios::app);
        if (!logFile.is_open()) {
            std::cerr << "无法打开新的日志文件: " << currentLogFilename << std::endl;
            return false;
        }
        
        std::cout << "[" << processName << "] 日志文件已轮转，新日志文件: " << currentLogFilename << std::endl;
        return true;
    }
    
    return false;
}

void EnhancedLogger::createLogDirectory(const std::string& path) {
    // 使用std::filesystem创建目录
    try {
        std::filesystem::create_directories(path);
    } catch (const std::exception& e) {
        std::cerr << "创建日志目录失败: " << path << " - " << e.what() << std::endl;
    }
}

std::string EnhancedLogger::generateLogFilename(const std::string& baseFilename) {
    // 获取当前时间作为文件名的一部分
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << logDirectory << "/" << baseFilename << "_";
    ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
    ss << ".log";
    
    return ss.str();
}