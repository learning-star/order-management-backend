#include "disp/server.h"
#include "common/logger_enhanced.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

Server::Server(int port) : port(port), running(false), serverSocket(-1) {
}

Server::~Server() {
    stop();
}

bool Server::start() {
    if (running) {
        LOG_WARNING("服务器已经在运行中");
        return true;
    }
    
    // 创建套接字
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        LOG_ERROR("创建套接字失败");
        return false;
    }
    
    // 设置套接字选项
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR("设置套接字选项失败");
        close(serverSocket);
        return false;
    }
    
    // 绑定地址
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG_ERROR("绑定地址失败");
        close(serverSocket);
        return false;
    }
    
    // 监听连接
    if (listen(serverSocket, 10) < 0) {
        LOG_ERROR("监听连接失败");
        close(serverSocket);
        return false;
    }
    
    // 启动接受连接的线程
    running = true;
    acceptThread = std::thread(&Server::acceptLoop, this);
    
    LOG_INFO("服务器已启动，监听端口: " + std::to_string(port));
    return true;
}

void Server::stop() {
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
    
    LOG_INFO("服务器已停止");
}

bool Server::isRunning() const {
    return running;
}

void Server::setRoute(const std::string& path, RequestHandler handler) {
    routes[path] = handler;
    LOG_INFO("注册路由: " + path);
}

void Server::acceptLoop() {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    while (running) {
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (running) {
                LOG_ERROR("接受连接失败");
            }
            continue;
        }
        
        // 创建新线程处理客户端连接
        clientThreads.emplace_back(&Server::handleClient, this, clientSocket);
    }
}

void Server::handleClient(int clientSocket) {
    const int bufferSize = 4096;
    char buffer[bufferSize];
    
    // 接收请求
    ssize_t bytesRead = recv(clientSocket, buffer, bufferSize - 1, 0);
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

std::string Server::processRequest(const std::string& request) {
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

void Server::parseRequest(const std::string& request, std::string& method, std::string& path) {
    std::istringstream iss(request);
    std::string version;
    iss >> method >> path >> version;
    
    // 移除查询参数
    size_t pos = path.find('?');
    if (pos != std::string::npos) {
        path = path.substr(0, pos);
    }
}

std::string Server::parseRequestPath(const std::string& request) {
    std::string method, path;
    this->parseRequest(request, method, path);
    return path;
}

std::string Server::createOptionsResponse() {
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

std::string Server::createResponse(const std::string& content, int statusCode, const std::string& contentType) {
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