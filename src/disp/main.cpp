#include "disp/server.h"
#include "disp/request_handler.h"
#include "common/config.h"
#include "common/logger_enhanced.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>

// 全局服务器对象，用于信号处理
Server* g_server = nullptr;

// 信号处理函数
void signalHandler(int signum) {
    LOG_INFO("接收到信号: " + std::to_string(signum));
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    // 设置日志
    EnhancedLogger::getInstance().setLogFile("logs/disp");
    
    // 加载配置
    if (!Config::getInstance().loadConfig("config/server.conf")) {
        LOG_ERROR("加载配置文件失败");
        return 1;
    }
    
    // 获取配置
    int port = Config::getInstance().getInt("disp.port", 8080);
    
    // 初始化请求处理器
    if (!RequestHandler::getInstance().init()) {
        LOG_ERROR("初始化请求处理器失败");
        return 1;
    }
    
    // 创建服务器
    Server server(port);
    g_server = &server;
    
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 注册路由
    server.setRoute("/api/health", [](const std::string& request) -> std::string {
        return RequestHandler::getInstance().handleRequest("/api/health", request);
    });
    
    server.setRoute("/api/version", [](const std::string& request) -> std::string {
        return RequestHandler::getInstance().handleRequest("/api/version", request);
    });
    
    server.setRoute("/api/user", [](const std::string& request) -> std::string {
        return RequestHandler::getInstance().handleRequest("/api/user", request);
    });
    
    server.setRoute("/api/order", [](const std::string& request) -> std::string {
        return RequestHandler::getInstance().handleRequest("/api/order", request);
    });
    
    server.setRoute("/api/product", [](const std::string& request) -> std::string {
        return RequestHandler::getInstance().handleRequest("/api/product", request);
    });
    
    // 启动服务器
    if (!server.start()) {
        LOG_ERROR("启动服务器失败");
        return 1;
    }
    
    LOG_INFO("Disp服务器已启动，监听端口: " + std::to_string(port));
    
    // 主线程等待，直到服务器停止
    while (server.isRunning()) {
        sleep(1);
    }
    
    LOG_INFO("Disp服务器已停止");
    return 0;
}