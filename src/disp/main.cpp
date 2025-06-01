#include "disp/server_factory.h"
#include "disp/request_handler.h"
#include "common/config.h"
#include "common/logger_enhanced.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <memory>

// 全局服务器对象，用于信号处理
std::unique_ptr<IServer> g_server = nullptr;

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
    int maxConnections = Config::getInstance().getInt("disp.max_connections", 1000);
    int timeout = Config::getInstance().getInt("disp.timeout", 60);
    bool useEpoll = Config::getInstance().getBool("disp.use_epoll", true);
    
    // 初始化请求处理器
    if (!RequestHandler::getInstance().init()) {
        LOG_ERROR("初始化请求处理器失败");
        return 1;
    }
    
    // 使用工厂创建服务器
    g_server = ServerFactory::createServer(useEpoll, port);
    if (!g_server) {
        LOG_ERROR("创建服务器失败");
        return 1;
    }
    
    // 配置服务器参数
    g_server->setMaxConnections(maxConnections);
    g_server->setTimeout(timeout);
    
    LOG_INFO("服务器配置: 类型=" + g_server->getServerType() +
             ", 端口=" + std::to_string(port) +
             ", 最大连接数=" + std::to_string(maxConnections) +
             ", 超时时间=" + std::to_string(timeout) + "秒");
    
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 注册路由
    g_server->setRoute("/api/health", [](const std::string& request) -> std::string {
        return RequestHandler::getInstance().handleRequest("/api/health", request);
    });
    
    g_server->setRoute("/api/version", [](const std::string& request) -> std::string {
        return RequestHandler::getInstance().handleRequest("/api/version", request);
    });
    
    g_server->setRoute("/api/user", [](const std::string& request) -> std::string {
        return RequestHandler::getInstance().handleRequest("/api/user", request);
    });
    
    g_server->setRoute("/api/order", [](const std::string& request) -> std::string {
        return RequestHandler::getInstance().handleRequest("/api/order", request);
    });
    
    g_server->setRoute("/api/product", [](const std::string& request) -> std::string {
        return RequestHandler::getInstance().handleRequest("/api/product", request);
    });
    
    // 启动服务器
    if (!g_server->start()) {
        LOG_ERROR("启动服务器失败");
        return 1;
    }
    
    LOG_INFO("Disp服务器已启动: " + g_server->getServerType() +
             ", 端口: " + std::to_string(g_server->getPort()));
    
    // 主线程等待，直到服务器停止
    while (g_server->isRunning()) {
        sleep(1);
    }
    
    LOG_INFO("Disp服务器已停止");
    return 0;
}