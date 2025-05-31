#ifndef LOGGER_ENHANCED_H
#define LOGGER_ENHANCED_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <thread>
#include <map>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

// 日志上下文信息
struct LogContext {
    std::string requestId;
    std::string clientIp;
    std::string userId;
    std::string operation;
    
    LogContext() = default;
    LogContext(const std::string& reqId, const std::string& ip = "", 
               const std::string& uid = "", const std::string& op = "")
        : requestId(reqId), clientIp(ip), userId(uid), operation(op) {}
};

class EnhancedLogger {
public:
    static EnhancedLogger& getInstance();
    
    void setLogLevel(LogLevel level);
    void setLogFile(const std::string& filename);
    void setProcessName(const std::string& name);
    void enableConsoleOutput(bool enable);
    void enableColorOutput(bool enable);
    
    // 基础日志方法
    void debug(const std::string& message, const LogContext& context = LogContext());
    void info(const std::string& message, const LogContext& context = LogContext());
    void warning(const std::string& message, const LogContext& context = LogContext());
    void error(const std::string& message, const LogContext& context = LogContext());
    void fatal(const std::string& message, const LogContext& context = LogContext());
    
    // 请求追踪日志
    void logRequest(const std::string& requestId, const std::string& method, 
                   const std::string& path, const std::string& clientIp = "");
    void logResponse(const std::string& requestId, int statusCode, 
                    const std::string& message, double responseTime = 0.0);
    void logApiCall(const std::string& requestId, const std::string& apiType,
                   const std::string& operation, const std::string& params = "");
    void logDatabase(const std::string& requestId, const std::string& operation,
                    const std::string& table, const std::string& query = "", double execTime = 0.0);
    void logError(const std::string& requestId, const std::string& errorType,
                 const std::string& errorMessage, const std::string& stackTrace = "");
    
    // 性能监控
    void logPerformance(const std::string& operation, double duration, 
                       const std::string& details = "");
    
    // 系统监控
    void logSystem(const std::string& component, const std::string& event,
                  const std::string& details = "");
    
private:
    EnhancedLogger();
    ~EnhancedLogger();
    EnhancedLogger(const EnhancedLogger&) = delete;
    EnhancedLogger& operator=(const EnhancedLogger&) = delete;
    
    void log(LogLevel level, const std::string& message, const LogContext& context = LogContext());
    std::string getCurrentTime() const;
    std::string levelToString(LogLevel level) const;
    std::string levelToColorString(LogLevel level) const;
    std::string formatLogMessage(LogLevel level, const std::string& message, const LogContext& context) const;
    bool checkAndRotateLogFile();
    void createLogDirectory(const std::string& path);
    std::string generateLogFilename(const std::string& baseFilename);
    std::string getThreadId() const;
    
    LogLevel currentLevel;
    std::ofstream logFile;
    std::mutex logMutex;
    bool consoleOutput;
    bool colorOutput;
    std::string currentLogFilename;
    std::string logDirectory;
    std::string logBaseName;
    std::string processName;
    const size_t maxLogFileSize = 200 * 1024 * 1024; // 200MB
};

// 便捷宏 - 增强版
#define LOG_DEBUG_CTX(msg, ctx) EnhancedLogger::getInstance().debug(msg, ctx)
#define LOG_INFO_CTX(msg, ctx) EnhancedLogger::getInstance().info(msg, ctx)
#define LOG_WARNING_CTX(msg, ctx) EnhancedLogger::getInstance().warning(msg, ctx)
#define LOG_ERROR_CTX(msg, ctx) EnhancedLogger::getInstance().error(msg, ctx)
#define LOG_FATAL_CTX(msg, ctx) EnhancedLogger::getInstance().fatal(msg, ctx)

// 兼容性宏
#define LOG_DEBUG(msg) EnhancedLogger::getInstance().debug(msg)
#define LOG_INFO(msg) EnhancedLogger::getInstance().info(msg)
#define LOG_WARNING(msg) EnhancedLogger::getInstance().warning(msg)
#define LOG_ERROR(msg) EnhancedLogger::getInstance().error(msg)
#define LOG_FATAL(msg) EnhancedLogger::getInstance().fatal(msg)

// 专用日志宏
#define LOG_REQUEST(reqId, method, path, ip) EnhancedLogger::getInstance().logRequest(reqId, method, path, ip)
#define LOG_RESPONSE(reqId, code, msg, time) EnhancedLogger::getInstance().logResponse(reqId, code, msg, time)
#define LOG_API_CALL(reqId, type, op, params) EnhancedLogger::getInstance().logApiCall(reqId, type, op, params)
#define LOG_DATABASE(reqId, op, table, query, time) EnhancedLogger::getInstance().logDatabase(reqId, op, table, query, time)
#define LOG_ERROR_DETAIL(reqId, type, msg, stack) EnhancedLogger::getInstance().logError(reqId, type, msg, stack)
#define LOG_PERFORMANCE(op, duration, details) EnhancedLogger::getInstance().logPerformance(op, duration, details)
#define LOG_SYSTEM(component, event, details) EnhancedLogger::getInstance().logSystem(component, event, details)

#endif // LOGGER_ENHANCED_H