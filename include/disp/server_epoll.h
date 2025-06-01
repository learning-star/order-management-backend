#ifndef SERVER_EPOLL_H
#define SERVER_EPOLL_H

#include "disp/iserver.h"
#include <string>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <vector>
#include <sys/epoll.h>

// 客户端连接状态
enum class ClientState {
    READING_REQUEST,    // 正在读取请求
    PROCESSING,         // 正在处理请求
    WRITING_RESPONSE,   // 正在写入响应
    CLOSING             // 准备关闭
};

// 客户端连接信息
struct ClientConnection {
    int fd;                          // 客户端socket文件描述符
    ClientState state;               // 连接状态
    std::string readBuffer;          // 读缓冲区
    std::string writeBuffer;         // 写缓冲区
    size_t writePos;                 // 写入位置
    time_t lastActivity;             // 最后活动时间
    bool keepAlive;                  // 是否保持连接
    
    ClientConnection(int clientFd) 
        : fd(clientFd), state(ClientState::READING_REQUEST), 
          writePos(0), lastActivity(time(nullptr)), keepAlive(false) {}
};

class EpollServer : public IServer {
public:
    EpollServer(int port);
    virtual ~EpollServer();
    
    // 实现IServer接口
    bool start() override;
    void stop() override;
    bool isRunning() const override;
    void setRoute(const std::string& path, RequestHandler handler) override;
    void setMaxConnections(int maxConn) override;
    void setTimeout(int timeoutSec) override;
    
    // 服务器信息
    std::string getServerType() const override { return "EpollServer"; }
    int getPort() const override { return port; }
    int getCurrentConnections() const override { return static_cast<int>(clients.size()); }
    
private:
    // 核心epoll事件循环
    void eventLoop();
    
    // 连接管理
    bool acceptNewConnection();
    void closeConnection(int clientFd);
    void cleanupTimeoutConnections();
    
    // IO处理
    bool handleRead(int clientFd);
    bool handleWrite(int clientFd);
    
    // 请求处理
    bool processCompleteRequest(int clientFd);
    std::string processRequest(const std::string& request);
    bool isRequestComplete(const std::string& buffer);
    
    // HTTP协议处理
    void parseRequest(const std::string& request, std::string& method, std::string& path);
    std::string createOptionsResponse();
    std::string createResponse(const std::string& content, int statusCode = 200, 
                              const std::string& contentType = "application/json");
    
    // epoll操作
    bool addToEpoll(int fd, uint32_t events);
    bool modifyEpoll(int fd, uint32_t events);
    bool removeFromEpoll(int fd);
    
    // 设置非阻塞模式
    bool setNonBlocking(int fd);
    
    // 服务器配置
    int port;
    int maxConnections;
    int connectionTimeout;
    
    // epoll相关
    int epollFd;
    int serverSocket;
    std::atomic<bool> running;
    
    // 连接管理
    std::unordered_map<int, std::unique_ptr<ClientConnection>> clients;
    std::unordered_map<std::string, RequestHandler> routes;
    
    // 常量
    static const int MAX_EVENTS = 1024;
    static const int BUFFER_SIZE = 4096;
    static const int LISTEN_BACKLOG = 128;
};

#endif // SERVER_EPOLL_H