# Linux Epoll深度解析与实践

## 📖 Epoll基础概念

### 什么是Epoll？
Epoll是Linux内核提供的一种高效的IO事件通知机制，专门用于处理大量并发连接的场景。它是select和poll的改进版本，解决了传统IO多路复用的性能瓶颈。

### 为什么需要Epoll？

#### 传统方案的问题
```cpp
// select的问题
fd_set readfds;
FD_ZERO(&readfds);
for (int i = 0; i < num_fds; i++) {
    FD_SET(fds[i], &readfds);  // O(n)操作
}
select(max_fd + 1, &readfds, NULL, NULL, NULL);  // O(n)扫描
for (int i = 0; i < num_fds; i++) {
    if (FD_ISSET(fds[i], &readfds)) {  // O(n)检查
        // 处理就绪的fd
    }
}
```

#### Epoll的优势
```cpp
// epoll的高效性
epoll_wait(epollfd, events, MAX_EVENTS, timeout);  // O(1)获取就绪事件
for (int i = 0; i < nfds; i++) {
    // 直接处理就绪的fd，无需遍历所有fd
    handle_event(events[i]);
}
```

## 🔧 Epoll核心API详解

### 1. epoll_create1() - 创建epoll实例

#### API签名
```cpp
#include <sys/epoll.h>
int epoll_create1(int flags);
```

#### 我们的实现
```cpp
// 在Server::start()中
epollFd = epoll_create1(EPOLL_CLOEXEC);
if (epollFd < 0) {
    LOG_ERROR("创建epoll实例失败: " + std::string(strerror(errno)));
    return false;
}
```

#### 参数说明
- `EPOLL_CLOEXEC`: 设置close-on-exec标志，子进程执行exec时自动关闭
- 返回值: epoll文件描述符，失败返回-1

### 2. epoll_ctl() - 控制epoll事件

#### API签名
```cpp
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

#### 我们的封装实现
```cpp
bool Server::addToEpoll(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;      // 设置监听的事件类型
    ev.data.fd = fd;         // 关联的文件描述符
    
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        LOG_ERROR("添加fd到epoll失败: " + std::string(strerror(errno)));
        return false;
    }
    return true;
}

bool Server::modifyEpoll(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        LOG_ERROR("修改epoll事件失败: " + std::string(strerror(errno)));
        return false;
    }
    return true;
}

bool Server::removeFromEpoll(int fd) {
    if (epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        LOG_ERROR("从epoll删除fd失败: " + std::string(strerror(errno)));
        return false;
    }
    return true;
}
```

#### 操作类型详解
```cpp
// op参数的三种操作
EPOLL_CTL_ADD    // 添加新的文件描述符到epoll
EPOLL_CTL_MOD    // 修改已存在的文件描述符的事件
EPOLL_CTL_DEL    // 从epoll中删除文件描述符
```

#### 事件类型详解
```cpp
// events参数的常用事件类型
EPOLLIN      // 可读事件（有数据到达）
EPOLLOUT     // 可写事件（发送缓冲区有空间）
EPOLLERR     // 错误事件
EPOLLHUP     // 挂起事件（连接断开）
EPOLLET      // 边缘触发模式
EPOLLONESHOT // 一次性事件
```

#### 实际使用示例
```cpp
// 添加服务器socket，监听新连接
addToEpoll(serverSocket, EPOLLIN);

// 添加客户端socket，使用边缘触发
addToEpoll(clientFd, EPOLLIN | EPOLLET);

// 切换到写模式
modifyEpoll(clientFd, EPOLLOUT | EPOLLET);
```

### 3. epoll_wait() - 等待IO事件

#### API签名
```cpp
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```

#### 我们的事件循环实现
```cpp
void Server::eventLoop() {
    struct epoll_event events[MAX_EVENTS];  // 事件数组
    time_t lastCleanup = time(nullptr);
    
    while (running) {
        // 等待事件，超时时间1秒
        int nfds = epoll_wait(epollFd, events, MAX_EVENTS, 1000);
        
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;  // 被信号中断，继续循环
            }
            LOG_ERROR("epoll_wait失败: " + std::string(strerror(errno)));
            break;
        }
        
        // 处理所有就绪的事件
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;           // 就绪的文件描述符
            uint32_t eventMask = events[i].events; // 就绪的事件类型
            
            if (fd == serverSocket) {
                // 新连接到达
                if (eventMask & EPOLLIN) {
                    acceptNewConnection();
                }
            } else {
                // 客户端事件处理
                handleClientEvent(fd, eventMask);
            }
        }
        
        // 定期维护任务
        performMaintenance();
    }
}
```

#### 参数详解
- `epfd`: epoll实例的文件描述符
- `events`: 用于接收就绪事件的数组
- `maxevents`: events数组的大小
- `timeout`: 超时时间（毫秒），-1表示无限等待，0表示立即返回
- 返回值: 就绪事件的数量，0表示超时，-1表示错误

## 🔄 边缘触发 vs 水平触发

### 水平触发 (Level Triggered, LT)
```cpp
// 水平触发特点：只要有数据就持续通知
addToEpoll(fd, EPOLLIN);  // 默认水平触发

// 数据处理示例
while (true) {
    epoll_wait(...);  // 如果还有数据，会立即返回
    recv(...);        // 即使只读取部分数据
    // 下次epoll_wait()仍会立即返回，因为还有数据
}
```

### 边缘触发 (Edge Triggered, ET)
```cpp
// 边缘触发特点：只在状态变化时通知一次
addToEpoll(fd, EPOLLIN | EPOLLET);  // 边缘触发

// 我们的边缘触发处理实现
bool Server::handleRead(int clientFd) {
    auto it = clients.find(clientFd);
    if (it == clients.end()) return false;
    
    ClientConnection* conn = it->second.get();
    char buffer[BUFFER_SIZE];
    
    // 必须循环读取所有可用数据
    while (true) {
        ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer), 0);
        
        if (bytesRead > 0) {
            // 成功读取数据
            conn->readBuffer.append(buffer, bytesRead);
            
            // 检查请求是否完整
            if (isRequestComplete(conn->readBuffer)) {
                return processCompleteRequest(clientFd);
            }
        } else if (bytesRead == 0) {
            // 连接关闭
            return false;
        } else {
            // 错误处理
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有更多数据可读，这是正常的
                break;
            } else {
                LOG_ERROR("读取失败: " + std::string(strerror(errno)));
                return false;
            }
        }
    }
    return true;
}
```

### 对比总结
| 特性 | 水平触发(LT) | 边缘触发(ET) |
|------|-------------|-------------|
| 通知方式 | 持续通知 | 状态变化时通知一次 |
| 编程难度 | 简单 | 复杂 |
| 性能 | 较低 | 较高 |
| 数据处理 | 可分批处理 | 必须一次性处理完 |
| 兼容性 | 兼容阻塞IO | 必须非阻塞IO |

## 🚫 非阻塞IO配合

### 设置非阻塞模式
```cpp
bool Server::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);     // 获取当前标志
    if (flags < 0) {
        LOG_ERROR("获取文件描述符标志失败: " + std::string(strerror(errno)));
        return false;
    }
    
    // 设置O_NONBLOCK标志
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG_ERROR("设置非阻塞模式失败: " + std::string(strerror(errno)));
        return false;
    }
    return true;
}
```

### 非阻塞Accept示例
```cpp
bool Server::acceptNewConnection() {
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        // 非阻塞accept
        int clientFd = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有更多连接可接受，正常退出
                break;
            }
            LOG_ERROR("接受连接失败: " + std::string(strerror(errno)));
            return false;
        }
        
        // 处理新连接
        setupNewConnection(clientFd);
    }
    return true;
}
```

### 非阻塞写入示例
```cpp
bool Server::handleWrite(int clientFd) {
    auto it = clients.find(clientFd);
    if (it == clients.end()) return false;
    
    ClientConnection* conn = it->second.get();
    
    // 循环发送直到EAGAIN
    while (conn->writePos < conn->writeBuffer.length()) {
        ssize_t bytesWritten = send(clientFd, 
                                   conn->writeBuffer.c_str() + conn->writePos,
                                   conn->writeBuffer.length() - conn->writePos, 0);
        
        if (bytesWritten > 0) {
            conn->writePos += bytesWritten;
        } else if (bytesWritten == 0) {
            break;  // 无法写入更多数据
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
        // 发送完毕，切换状态
        return switchToReadMode(clientFd);
    }
    
    return true;
}
```

## 🏗️ 状态机设计

### 连接状态定义
```cpp
enum class ClientState {
    READING_REQUEST,    // 正在读取HTTP请求
    PROCESSING,         // 正在处理业务逻辑
    WRITING_RESPONSE,   // 正在发送HTTP响应
    CLOSING             // 准备关闭连接
};

struct ClientConnection {
    int fd;                          // socket文件描述符
    ClientState state;               // 当前状态
    std::string readBuffer;          // 读缓冲区
    std::string writeBuffer;         // 写缓冲区
    size_t writePos;                 // 写入位置
    time_t lastActivity;             // 最后活动时间
    bool keepAlive;                  // 是否保持连接
    
    ClientConnection(int clientFd) 
        : fd(clientFd), state(ClientState::READING_REQUEST), 
          writePos(0), lastActivity(time(nullptr)), keepAlive(false) {}
};
```

### 状态转换逻辑
```cpp
bool Server::processCompleteRequest(int clientFd) {
    auto it = clients.find(clientFd);
    if (it == clients.end()) return false;
    
    ClientConnection* conn = it->second.get();
    
    // 状态转换: READING_REQUEST → PROCESSING
    conn->state = ClientState::PROCESSING;
    
    // 处理HTTP请求
    std::string response = processRequest(conn->readBuffer);
    
    // 状态转换: PROCESSING → WRITING_RESPONSE
    conn->writeBuffer = response;
    conn->writePos = 0;
    conn->state = ClientState::WRITING_RESPONSE;
    
    // 切换epoll事件为可写
    if (!modifyEpoll(clientFd, EPOLLOUT | EPOLLET)) {
        return false;
    }
    
    // 立即尝试发送数据
    return handleWrite(clientFd);
}
```

## 🔍 HTTP请求解析

### 请求完整性检查
```cpp
bool Server::isRequestComplete(const std::string& buffer) {
    // 1. 查找HTTP头结束标记
    size_t headerEnd = buffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return false;  // HTTP头还未完整接收
    }
    
    // 2. 检查Content-Length头部
    std::string lowerBuffer = buffer;
    std::transform(lowerBuffer.begin(), lowerBuffer.end(), 
                   lowerBuffer.begin(), ::tolower);
    
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
                
                // 检查body是否完整
                return actualBodyLength >= static_cast<size_t>(contentLength);
            } catch (const std::exception&) {
                return true;  // 解析失败，假设完整
            }
        }
    }
    
    // 没有Content-Length，假设是GET请求等无body的请求
    return true;
}
```

## ⚡ 性能优化技巧

### 1. 批量事件处理
```cpp
// 一次性处理多个事件，减少系统调用
struct epoll_event events[MAX_EVENTS];  // 1024个事件
int nfds = epoll_wait(epollFd, events, MAX_EVENTS, timeout);
```

### 2. 内存池优化
```cpp
// 使用智能指针管理连接对象
std::unordered_map<int, std::unique_ptr<ClientConnection>> clients;

// 连接对象复用
void resetConnection(ClientConnection* conn) {
    conn->readBuffer.clear();
    conn->writeBuffer.clear();
    conn->writePos = 0;
    conn->state = ClientState::READING_REQUEST;
    conn->lastActivity = time(nullptr);
}
```

### 3. 超时清理机制
```cpp
void Server::cleanupTimeoutConnections() {
    time_t now = time(nullptr);
    std::vector<int> timeoutFds;
    
    // 收集超时的连接
    for (const auto& pair : clients) {
        if (now - pair.second->lastActivity > connectionTimeout) {
            timeoutFds.push_back(pair.first);
        }
    }
    
    // 批量清理
    for (int fd : timeoutFds) {
        LOG_INFO("清理超时连接 (fd=" + std::to_string(fd) + ")");
        closeConnection(fd);
    }
}
```

## 🚨 错误处理最佳实践

### 1. EAGAIN/EWOULDBLOCK处理
```cpp
// 正确的非阻塞IO错误处理
if (bytesRead < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // 这不是错误，只是暂时没有数据
        return true;  // 继续处理其他事件
    } else {
        // 真正的错误
        LOG_ERROR("读取错误: " + std::string(strerror(errno)));
        return false;  // 关闭连接
    }
}
```

### 2. 信号中断处理
```cpp
int nfds = epoll_wait(epollFd, events, MAX_EVENTS, timeout);
if (nfds < 0) {
    if (errno == EINTR) {
        continue;  // 被信号中断，继续循环
    }
    // 其他错误
    LOG_ERROR("epoll_wait失败: " + std::string(strerror(errno)));
    break;
}
```

### 3. 连接异常处理
```cpp
if (eventMask & (EPOLLHUP | EPOLLERR)) {
    // 连接挂起或错误，需要清理
    LOG_DEBUG("连接异常 (fd=" + std::to_string(fd) + ")");
    closeConnection(fd);
    continue;
}
```

## 📊 监控和调试

### 1. 连接统计
```cpp
void Server::printStats() {
    LOG_INFO("当前连接数: " + std::to_string(clients.size()));
    LOG_INFO("最大连接数: " + std::to_string(maxConnections));
    
    // 统计各状态的连接数
    std::map<ClientState, int> stateCount;
    for (const auto& pair : clients) {
        stateCount[pair.second->state]++;
    }
    
    for (const auto& state : stateCount) {
        LOG_INFO("状态 " + std::to_string(static_cast<int>(state.first)) + 
                ": " + std::to_string(state.second) + " 个连接");
    }
}
```

### 2. 性能监控
```cpp
// 监控事件循环性能
auto startTime = std::chrono::high_resolution_clock::now();
int nfds = epoll_wait(epollFd, events, MAX_EVENTS, timeout);
auto endTime = std::chrono::high_resolution_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
if (duration.count() > 1000) {  // 超过1ms
    LOG_WARNING("epoll_wait耗时过长: " + std::to_string(duration.count()) + "微秒");
}
```

## 🎯 总结

通过这次深入的epoll学习，你掌握了：

1. **Epoll三大API**: `epoll_create1()`, `epoll_ctl()`, `epoll_wait()`
2. **边缘触发技术**: 提高性能的关键技术
3. **非阻塞IO编程**: 避免线程阻塞的核心技术
4. **状态机设计**: 管理复杂连接状态的设计模式
5. **错误处理**: 处理各种异常情况的最佳实践
6. **性能优化**: 内存管理、批量处理等优化技巧

Epoll是Linux下高性能服务器开发的核心技术，掌握它将为你的系统架构能力带来质的提升！