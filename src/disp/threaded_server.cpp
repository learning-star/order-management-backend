#include "disp/threaded_server.h"
#include "common/logger_enhanced.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

ThreadedServer::ThreadedServer(int port) 
    : port(port), maxConnections(100), connectionTimeout(60),
      serverSocket(-1), running(false), currentConnections(0) {
}

ThreadedServer::~ThreadedServer() {
    stop();
}

bool ThreadedServer::start() {
    if (running) {
        LOG_WARNING("ThreadedServer已经在运行中");
        return true;
    }
    
    // 创建套接字
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        LOG_ERROR("创建套接字失败: " + std::string(strerror(errno)));
        return false;
    }
    
    // 设置套接字选项
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR("设置套接字选项失败: " + std::string(strerror(errno)));
        close(serverSocket);
        return false;
    }
    
    // 绑定地址
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG_ERROR("绑定地址失败: " + std::string(strerror(errno)));
        close(serverSocket);
        return false;
    }
    
    // 监听连接
    if (listen(serverSocket, LISTEN_BACKLOG) < 0) {
        LOG_ERROR("监听连接失败: " + std::string(strerror(errno)));
        close(serverSocket);
        return false;
    }
    
    running = true;
    
    // 启动接受连接的线程
    acceptThread = std::thread(&ThreadedServer::acceptLoop, this);
    
    LOG_INFO("ThreadedServer已启动，监听端口: " + std::to_string(port) + 
             "，最大连接数: " + std::to_string(maxConnections));
    
    return true;
}

void ThreadedServer::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    
    // 关闭服务器套接字
    if (serverSocket >= 0) {
        close(serverSocket);
        serverSocket = -1;
    }
    
    // 等待接受线程结束
    if (acceptThread.joinable()) {
        acceptThread.join();
    }
    
    // 等待所有客户端线程结束
    for (auto& thread : clientThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    clientThreads.clear();
    
    LOG_INFO("ThreadedServer已停止");
}

bool ThreadedServer::isRunning() const {
    return running;
}

void ThreadedServer::setRoute(const std::string& path, RequestHandler handler) {
    routes[path] = handler;
    LOG_INFO("ThreadedServer注册路由: " + path);
}

void ThreadedServer::setMaxConnections(int maxConn) {
    maxConnections = maxConn;
    LOG_INFO("ThreadedServer设置最大连接数: " + std::to_string(maxConn));
}

void ThreadedServer::setTimeout(int timeoutSec) {
    connectionTimeout = timeoutSec;
    LOG_INFO("ThreadedServer设置超时时间: " + std::to_string(timeoutSec) + "秒");
}

void ThreadedServer::acceptLoop() {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    while (running) {
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (running) {
                LOG_ERROR("接受连接失败: " + std::string(strerror(errno)));
            }
            continue;
        }
        
        // 检查连接数限制
        if (currentConnections >= maxConnections) {
            LOG_WARNING("达到最大连接数限制，拒绝新连接");
            close(clientSocket);
            continue;
        }
        
        // 创建新线程处理客户端连接
        currentConnections++;
        clientThreads.emplace_back([this, clientSocket]() {
            this->handleClient(clientSocket);
            this->currentConnections--;
        });
    }
}

void ThreadedServer::handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    
    // 接收请求
    ssize_t bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
    if (bytesRead <= 0) {
        close(clientSocket);
        return;
    }
    
    // 确保字符串以null结尾
    buffer[bytesRead] = '\0';
    
    // 处理请求
    std::string request(buffer);
    std::string response = processRequest(request);
    
    // 发送响应
    send(clientSocket, response.c_str(), response.length(), 0);
    
    // 关闭连接
    close(clientSocket);
}

std::string ThreadedServer::processRequest(const std::string& request) {
    // 解析请求路径和方法
    std::string method, path;
    this->parseRequest(request, method, path);
    
    // 处理OPTIONS请求（预检请求）
    if (method == "OPTIONS") {
        return this->createOptionsResponse();
    }
    
    // 查找对应的处理函数
    auto it = routes.find(path);
    if (it != routes.end()) {
        try {
            // 调用处理函数
            std::string content = it->second(request);
            return createResponse(content);
        } catch (const std::exception& e) {
            LOG_ERROR("处理请求时发生异常: " + std::string(e.what()));
            return createResponse("{\"error\":\"内部服务器错误\"}", 500);
        }
    }
    
    // 未找到对应的路由
    LOG_WARNING("未找到路由: " + path);
    return createResponse("{\"error\":\"未找到\"}", 404);
}

void ThreadedServer::parseRequest(const std::string& request, std::string& method, std::string& path) {
    std::istringstream iss(request);
    std::string version;
    iss >> method >> path >> version;
    
    // 移除查询参数
    size_t pos = path.find('?');
    if (pos != std::string::npos) {
        path = path.substr(0, pos);
    }
}

std::string ThreadedServer::createOptionsResponse() {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    oss << "Access-Control-Max-Age: 86400\r\n";
    oss << "Content-Length: 0\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    
    return oss.str();
}

std::string ThreadedServer::createResponse(const std::string& content, int statusCode, const std::string& contentType) {
    std::string status;
    switch (statusCode) {
        case 200: status = "200 OK"; break;
        case 400: status = "400 Bad Request"; break;
        case 404: status = "404 Not Found"; break;
        case 500: status = "500 Internal Server Error"; break;
        default: status = "200 OK";
    }
    
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << "\r\n";
    oss << "Content-Type: " << contentType << "\r\n";
    oss << "Content-Length: " << content.length() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    oss << "\r\n";
    oss << content;
    
    return oss.str();
}