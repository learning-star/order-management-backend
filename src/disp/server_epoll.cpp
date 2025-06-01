#include "disp/server_epoll.h"
#include "common/logger_enhanced.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <algorithm>

EpollServer::EpollServer(int port) 
    : port(port), maxConnections(1000), connectionTimeout(60),
      epollFd(-1), serverSocket(-1), running(false) {
}

EpollServer::~EpollServer() {
    stop();
}

void EpollServer::setMaxConnections(int maxConn) {
    maxConnections = maxConn;
    LOG_INFO("EpollServer设置最大连接数: " + std::to_string(maxConn));
}

void EpollServer::setTimeout(int timeoutSec) {
    connectionTimeout = timeoutSec;
    LOG_INFO("EpollServer设置超时时间: " + std::to_string(timeoutSec) + "秒");
}

bool EpollServer::start() {
    if (running) {
        LOG_WARNING("Epoll服务器已经在运行中");
        return true;
    }
    
    // 1. 创建服务器socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        LOG_ERROR("创建服务器套接字失败: " + std::string(strerror(errno)));
        return false;
    }
    
    // 2. 设置socket选项
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR("设置SO_REUSEADDR失败: " + std::string(strerror(errno)));
        close(serverSocket);
        return false;
    }
    
    // 3. 设置非阻塞模式
    if (!setNonBlocking(serverSocket)) {
        LOG_ERROR("设置服务器socket非阻塞模式失败");
        close(serverSocket);
        return false;
    }
    
    // 4. 绑定地址
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
    
    // 5. 开始监听
    if (listen(serverSocket, LISTEN_BACKLOG) < 0) {
        LOG_ERROR("监听失败: " + std::string(strerror(errno)));
        close(serverSocket);
        return false;
    }
    
    // 6. 创建epoll实例
    epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd < 0) {
        LOG_ERROR("创建epoll实例失败: " + std::string(strerror(errno)));
        close(serverSocket);
        return false;
    }
    
    // 7. 将服务器socket添加到epoll
    if (!addToEpoll(serverSocket, EPOLLIN)) {
        LOG_ERROR("将服务器socket添加到epoll失败");
        close(epollFd);
        close(serverSocket);
        return false;
    }
    
    running = true;
    LOG_INFO("Epoll服务器已启动，监听端口: " + std::to_string(port) + 
             "，最大连接数: " + std::to_string(maxConnections));
    
    // 8. 启动事件循环
    eventLoop();
    
    return true;
}

void EpollServer::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    
    // 关闭所有客户端连接
    for (auto& pair : clients) {
        close(pair.first);
    }
    clients.clear();
    
    // 关闭epoll和服务器socket
    if (epollFd >= 0) {
        close(epollFd);
        epollFd = -1;
    }
    
    if (serverSocket >= 0) {
        close(serverSocket);
        serverSocket = -1;
    }
    
    LOG_INFO("Epoll服务器已停止");
}

bool EpollServer::isRunning() const {
    return running;
}

void EpollServer::setRoute(const std::string& path, RequestHandler handler) {
    routes[path] = handler;
    LOG_INFO("EpollServer注册路由: " + path);
}

void EpollServer::eventLoop() {
    struct epoll_event events[MAX_EVENTS];
    time_t lastCleanup = time(nullptr);
    
    while (running) {
        // 等待事件，超时时间1秒
        int nfds = epoll_wait(epollFd, events, MAX_EVENTS, 1000);
        
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;  // 被信号中断，继续
            }
            LOG_ERROR("epoll_wait失败: " + std::string(strerror(errno)));
            break;
        }
        
        // 处理所有就绪的事件
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t eventMask = events[i].events;
            
            if (fd == serverSocket) {
                // 新连接到达
                if (eventMask & EPOLLIN) {
                    acceptNewConnection();
                }
            } else {
                // 客户端事件
                bool shouldClose = false;
                
                if (eventMask & (EPOLLHUP | EPOLLERR)) {
                    // 连接异常或挂起
                    shouldClose = true;
                } else if (eventMask & EPOLLIN) {
                    // 可读事件
                    if (!handleRead(fd)) {
                        shouldClose = true;
                    }
                } else if (eventMask & EPOLLOUT) {
                    // 可写事件
                    if (!handleWrite(fd)) {
                        shouldClose = true;
                    }
                }
                
                if (shouldClose) {
                    closeConnection(fd);
                }
            }
        }
        
        // 定期清理超时连接
        time_t now = time(nullptr);
        if (now - lastCleanup >= 10) {  // 每10秒清理一次
            cleanupTimeoutConnections();
            lastCleanup = now;
        }
    }
}

bool EpollServer::acceptNewConnection() {
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        int clientFd = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有更多连接可接受
                break;
            }
            LOG_ERROR("接受连接失败: " + std::string(strerror(errno)));
            return false;
        }
        
        // 检查连接数限制
        if (clients.size() >= static_cast<size_t>(maxConnections)) {
            LOG_WARNING("达到最大连接数限制，拒绝新连接");
            close(clientFd);
            continue;
        }
        
        // 设置客户端socket为非阻塞
        if (!setNonBlocking(clientFd)) {
            LOG_ERROR("设置客户端socket非阻塞模式失败");
            close(clientFd);
            continue;
        }
        
        // 添加到epoll监听
        if (!addToEpoll(clientFd, EPOLLIN | EPOLLET)) {  // 边缘触发模式
            LOG_ERROR("将客户端socket添加到epoll失败");
            close(clientFd);
            continue;
        }
        
        // 创建客户端连接对象
        clients[clientFd] = std::make_unique<ClientConnection>(clientFd);
        
        // 记录新连接
        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
        LOG_INFO("新连接建立: " + std::string(clientIp) + ":" + 
                std::to_string(ntohs(clientAddr.sin_port)) + 
                " (fd=" + std::to_string(clientFd) + ")");
    }
    
    return true;
}

bool EpollServer::handleRead(int clientFd) {
    auto it = clients.find(clientFd);
    if (it == clients.end()) {
        return false;
    }
    
    ClientConnection* conn = it->second.get();
    conn->lastActivity = time(nullptr);
    
    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer), 0);
        
        if (bytesRead > 0) {
            // 成功读取数据
            conn->readBuffer.append(buffer, bytesRead);
            
            // 检查是否接收到完整的HTTP请求
            if (isRequestComplete(conn->readBuffer)) {
                return processCompleteRequest(clientFd);
            }
        } else if (bytesRead == 0) {
            // 客户端关闭连接
            LOG_DEBUG("客户端关闭连接 (fd=" + std::to_string(clientFd) + ")");
            return false;
        } else {
            // 读取错误
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有更多数据可读，这是正常的
                break;
            } else {
                LOG_ERROR("读取客户端数据失败: " + std::string(strerror(errno)));
                return false;
            }
        }
    }
    
    return true;
}

bool EpollServer::handleWrite(int clientFd) {
    auto it = clients.find(clientFd);
    if (it == clients.end()) {
        return false;
    }
    
    ClientConnection* conn = it->second.get();
    conn->lastActivity = time(nullptr);
    
    while (conn->writePos < conn->writeBuffer.length()) {
        ssize_t bytesWritten = send(clientFd, 
                                   conn->writeBuffer.c_str() + conn->writePos,
                                   conn->writeBuffer.length() - conn->writePos, 0);
        
        if (bytesWritten > 0) {
            conn->writePos += bytesWritten;
        } else if (bytesWritten == 0) {
            // 无法写入更多数据
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 发送缓冲区满，稍后再试
                break;
            } else {
                LOG_ERROR("发送数据失败: " + std::string(strerror(errno)));
                return false;
            }
        }
    }
    
    // 检查是否发送完毕
    if (conn->writePos >= conn->writeBuffer.length()) {
        // 发送完毕，关闭连接或切换为读模式
        if (conn->keepAlive) {
            // 保持连接，重置状态
            conn->readBuffer.clear();
            conn->writeBuffer.clear();
            conn->writePos = 0;
            conn->state = ClientState::READING_REQUEST;
            
            // 切换为只监听读事件
            return modifyEpoll(clientFd, EPOLLIN | EPOLLET);
        } else {
            // 关闭连接
            return false;
        }
    }
    
    return true;
}

bool EpollServer::processCompleteRequest(int clientFd) {
    auto it = clients.find(clientFd);
    if (it == clients.end()) {
        return false;
    }
    
    ClientConnection* conn = it->second.get();
    
    // 处理HTTP请求
    std::string response = processRequest(conn->readBuffer);
    
    // 准备响应
    conn->writeBuffer = response;
    conn->writePos = 0;
    conn->state = ClientState::WRITING_RESPONSE;
    
    // 切换为监听写事件
    if (!modifyEpoll(clientFd, EPOLLOUT | EPOLLET)) {
        return false;
    }
    
    // 立即尝试发送一些数据
    return handleWrite(clientFd);
}

void EpollServer::closeConnection(int clientFd) {
    removeFromEpoll(clientFd);
    close(clientFd);
    clients.erase(clientFd);
    
    LOG_DEBUG("关闭连接 (fd=" + std::to_string(clientFd) + ")");
}

void EpollServer::cleanupTimeoutConnections() {
    time_t now = time(nullptr);
    std::vector<int> timeoutFds;
    
    for (const auto& pair : clients) {
        if (now - pair.second->lastActivity > connectionTimeout) {
            timeoutFds.push_back(pair.first);
        }
    }
    
    for (int fd : timeoutFds) {
        LOG_INFO("清理超时连接 (fd=" + std::to_string(fd) + ")");
        closeConnection(fd);
    }
}

bool EpollServer::isRequestComplete(const std::string& buffer) {
    // 简单的HTTP请求完整性检查
    // 查找请求结束标记 "\r\n\r\n"
    size_t headerEnd = buffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return false;  // 还没有收到完整的HTTP头部
    }
    
    // 检查Content-Length头部
    std::string lowerBuffer = buffer;
    std::transform(lowerBuffer.begin(), lowerBuffer.end(), lowerBuffer.begin(), ::tolower);
    
    size_t contentLengthPos = lowerBuffer.find("content-length:");
    if (contentLengthPos != std::string::npos) {
        // 提取Content-Length值
        size_t valueStart = contentLengthPos + 15;
        size_t lineEnd = buffer.find("\r\n", valueStart);
        if (lineEnd != std::string::npos) {
            std::string lengthStr = buffer.substr(valueStart, lineEnd - valueStart);
            lengthStr.erase(0, lengthStr.find_first_not_of(" \t"));
            lengthStr.erase(lengthStr.find_last_not_of(" \t") + 1);
            
            try {
                int contentLength = std::stoi(lengthStr);
                size_t bodyStart = headerEnd + 4;
                size_t actualBodyLength = buffer.length() - bodyStart;
                
                return actualBodyLength >= static_cast<size_t>(contentLength);
            } catch (const std::exception&) {
                // 无效的Content-Length，假设请求完整
                return true;
            }
        }
    }
    
    // 没有Content-Length头部，假设请求完整
    return true;
}

std::string EpollServer::processRequest(const std::string& request) {
    // 解析请求路径和方法
    std::string method, path;
    parseRequest(request, method, path);
    
    // 处理OPTIONS请求（预检请求）
    if (method == "OPTIONS") {
        return createOptionsResponse();
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

void EpollServer::parseRequest(const std::string& request, std::string& method, std::string& path) {
    std::istringstream iss(request);
    std::string version;
    iss >> method >> path >> version;
    
    // 移除查询参数
    size_t pos = path.find('?');
    if (pos != std::string::npos) {
        path = path.substr(0, pos);
    }
}

std::string EpollServer::createOptionsResponse() {
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

std::string EpollServer::createResponse(const std::string& content, int statusCode, const std::string& contentType) {
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

bool EpollServer::addToEpoll(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        LOG_ERROR("添加fd到epoll失败: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

bool EpollServer::modifyEpoll(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        LOG_ERROR("修改epoll事件失败: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

bool EpollServer::removeFromEpoll(int fd) {
    if (epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        LOG_ERROR("从epoll删除fd失败: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

bool EpollServer::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        LOG_ERROR("获取文件描述符标志失败: " + std::string(strerror(errno)));
        return false;
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG_ERROR("设置非阻塞模式失败: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}