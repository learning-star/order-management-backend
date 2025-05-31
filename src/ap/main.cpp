#include "ap/processor.h"
#include "ap/db_manager.h"
#include "common/config.h"
#include "common/logger_enhanced.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>

// 信号处理函数
void signalHandler(int signum) {
    LOG_INFO("接收到信号: " + std::to_string(signum));
    Processor::getInstance().stopService();
}

int main(int argc, char* argv[]) {
    // 设置日志
    EnhancedLogger::getInstance().setLogFile("logs/ap");
    
    // 加载配置
    if (!Config::getInstance().loadConfig("config/server.conf")) {
        LOG_ERROR("加载配置文件失败");
        return 1;
    }
    
    // 获取配置
    int port = Config::getInstance().getInt("ap.port", 8081);
    std::string dbHost = Config::getInstance().getString("db.host", "localhost");
    std::string dbUser = Config::getInstance().getString("db.user", "root");
    std::string dbPassword = Config::getInstance().getString("db.password", "");
    std::string dbName = Config::getInstance().getString("db.name", "myapp");
    int dbPort = Config::getInstance().getInt("db.port", 3306);
    
    // 连接数据库
    if (!DBManager::getInstance().connect(dbHost, dbUser, dbPassword, dbName, dbPort)) {
        LOG_ERROR("连接数据库失败");
        return 1;
    }
    
    // 初始化处理器
    if (!Processor::getInstance().init()) {
        LOG_ERROR("初始化处理器失败");
        return 1;
    }
    
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 注册更多处理函数
    Processor::getInstance().registerProcessor("user.create", [](const std::string& data) -> std::string {
        // 这里应该解析data，然后插入数据库
        // 简化示例：
        std::string query = "INSERT INTO users (name, email) VALUES ('测试用户', 'test@example.com')";
        if (DBManager::getInstance().executeQuery(query)) {
            unsigned long long id = DBManager::getInstance().getLastInsertId();
            return "{\"id\":" + std::to_string(id) + ",\"success\":true}";
        } else {
            return "{\"error\":\"创建用户失败\",\"success\":false}";
        }
    });
    
    Processor::getInstance().registerProcessor("user.update", [](const std::string& data) -> std::string {
        // 简化示例：
        std::string query = "UPDATE users SET name = '更新用户' WHERE id = 1";
        if (DBManager::getInstance().executeQuery(query)) {
            return "{\"success\":true}";
        } else {
            return "{\"error\":\"更新用户失败\",\"success\":false}";
        }
    });
    
    Processor::getInstance().registerProcessor("user.delete", [](const std::string& data) -> std::string {
        // 简化示例：
        std::string query = "DELETE FROM users WHERE id = 1";
        if (DBManager::getInstance().executeQuery(query)) {
            return "{\"success\":true}";
        } else {
            return "{\"error\":\"删除用户失败\",\"success\":false}";
        }
    });
    
    // 启动处理服务
    if (!Processor::getInstance().startService(port)) {
        LOG_ERROR("启动处理服务失败");
        return 1;
    }
    
    LOG_INFO("AP处理服务已启动，监听端口: " + std::to_string(port));
    
    // 主线程等待，直到收到信号
    while (Processor::getInstance().isRunning()) {
        sleep(1);
    }
    
    // 断开数据库连接
    DBManager::getInstance().disconnect();
    
    LOG_INFO("AP处理服务已停止");
    return 0;
}