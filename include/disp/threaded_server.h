#ifndef THREADED_SERVER_H
#define THREADED_SERVER_H

#include "disp/iserver.h"
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <unordered_map>

/**
 * 传统多线程服务器实现
 * 采用"一连接一线程"模型
 */
class ThreadedServer : public IServer {
public:
    ThreadedServer(int port);
    virtual ~ThreadedServer();
    
    // 实现IServer接口
    bool start() override;
    void stop() override;
    bool isRunning() const override;
    void setRoute(const std::string& path, RequestHandler handler) override;
    void setMaxConnections(int maxConn) override;
    void setTimeout(int timeoutSec) override;
    
    // 服务器信息
    std::string getServerType() const override { return "ThreadedServer"; }
    int getPort() const override { return port; }
    int getCurrentConnections() const override { return currentConnections; }
    
private:
    // 线程处理函数
    void acceptLoop();
    void handleClient(int clientSocket);
    
    // HTTP处理
    std::string processRequest(const std::string& request);
    void parseRequest(const std::string& request, std::string& method, std::string& path);
    std::string createOptionsResponse();
    std::string createResponse(const std::string& content, int statusCode = 200, 
                              const std::string& contentType = "application/json");
    
    // 配置参数
    int port;
    int maxConnections;
    int connectionTimeout;
    
    // 网络相关
    int serverSocket;
    std::atomic<bool> running;
    std::atomic<int> currentConnections;
    
    // 线程管理
    std::thread acceptThread;
    std::vector<std::thread> clientThreads;
    
    // 路由管理
    std::unordered_map<std::string, RequestHandler> routes;
    
    // 常量
    static const int LISTEN_BACKLOG = 10;
    static const int BUFFER_SIZE = 4096;
};

#endif // THREADED_SERVER_H