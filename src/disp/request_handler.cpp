#include "disp/request_handler.h"
#include "common/logger_enhanced.h"
#include "common/config.h"
#include "common/utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <regex>
#include <nlohmann/json.hpp>
#include <chrono>
#include <random>

RequestHandler& RequestHandler::getInstance() {
    static RequestHandler instance;
    return instance;
}

std::string RequestHandler::generateRequestId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "REQ" << timestamp << dis(gen);
    return ss.str();
}

bool RequestHandler::init() {
    // 设置进程名称
    EnhancedLogger::getInstance().setProcessName("DISP");
    
    // 从配置文件加载AP端点配置
    Config& config = Config::getInstance();
    
    // 从配置文件读取AP端点
    apEndpoints["user"] = config.getString("ap.endpoints.user", "http://localhost:8081");
    apEndpoints["order"] = config.getString("ap.endpoints.order", "http://localhost:8081");
    apEndpoints["product"] = config.getString("ap.endpoints.product", "http://localhost:8081");
    
    LOG_SYSTEM("RequestHandler", "初始化开始", "");
    
    for (const auto& endpoint : apEndpoints) {
        LOG_SYSTEM("RequestHandler", "配置AP端点", endpoint.first + " -> " + endpoint.second);
    }
    
    // 注册一些默认处理函数
    registerHandler("/api/health", [](const std::string&) -> std::string {
        return "{\"status\":\"ok\",\"timestamp\":\"" + std::to_string(std::time(nullptr)) + "\"}";
    });
    
    registerHandler("/api/version", [](const std::string&) -> std::string {
        return "{\"version\":\"1.0.0\",\"service\":\"DISP\"}";
    });
    
    LOG_SYSTEM("RequestHandler", "初始化完成", "已注册 " + std::to_string(handlers.size()) + " 个处理函数");
    return true;
}

std::string RequestHandler::handleRequest(const std::string& path, const std::string& requestData) {
    // 生成请求ID用于追踪
    std::string requestId = generateRequestId();
    std::string clientIp = extractClientIp(requestData);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 解析HTTP请求获取方法
    HttpRequest httpRequest = parseHttpRequest(requestData);
    
    // 记录请求日志
    LOG_REQUEST(requestId, httpRequest.method, path, clientIp);
    
    std::string response;
    int statusCode = 200;
    
    try {
        // 检查是否有直接处理函数
        auto it = handlers.find(path);
        if (it != handlers.end()) {
            LOG_INFO_CTX("使用本地处理函数", LogContext(requestId, clientIp, "", path));
            response = it->second(requestData);
        } else {
            // 需要转发到AP处理
            response = handleApiRequest(requestId, path, requestData, clientIp);
        }
    } catch (const std::exception& e) {
        statusCode = 500;
        response = "{\"error\":\"处理请求时发生异常\",\"message\":\"" + std::string(e.what()) + "\"}";
        LOG_ERROR_DETAIL(requestId, "RequestProcessing", "处理请求异常", e.what());
    }
    
    // 计算响应时间
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // 记录响应日志
    LOG_RESPONSE(requestId, statusCode, "", duration.count());
    
    return response;
}

std::string RequestHandler::handleApiRequest(const std::string& requestId, const std::string& path, 
                                           const std::string& requestData, const std::string& clientIp) {
    // 解析HTTP请求
    HttpRequest httpRequest = parseHttpRequest(requestData);
    
    // 根据路径确定AP端点和请求类型
    std::string apType;
    std::string requestType;
    
    if (path.find("/api/user") == 0) {
        apType = "user";
        requestType = determineUserRequestType(httpRequest.method, path);
    } else if (path.find("/api/order") == 0) {
        apType = "order";
        requestType = determineOrderRequestType(httpRequest.method, path);
    } else if (path.find("/api/product") == 0) {
        apType = "product";
        requestType = determineProductRequestType(httpRequest.method, path);
    } else {
        LOG_WARNING_CTX("未知的API路径", LogContext(requestId, clientIp, "", path));
        return "{\"error\":\"未知的API路径\",\"path\":\"" + path + "\"}";
    }
    
    LOG_API_CALL(requestId, apType, requestType, "路径: " + path);
    
    // 转发到对应的AP
    auto endpointIt = apEndpoints.find(apType);
    if (endpointIt != apEndpoints.end()) {
        return forwardToAp(requestId, endpointIt->second, requestType, httpRequest, clientIp);
    }
    
    LOG_WARNING_CTX("未找到对应的AP端点", LogContext(requestId, clientIp, "", apType));
    return "{\"error\":\"未找到对应的处理服务\",\"service\":\"" + apType + "\"}";
}

void RequestHandler::registerHandler(const std::string& path, HandlerFunc handler) {
    handlers[path] = handler;
    LOG_SYSTEM("RequestHandler", "注册处理函数", path);
}

std::string RequestHandler::forwardToAp(const std::string& requestId, const std::string& apEndpoint, 
                                       const std::string& requestType, const HttpRequest& httpRequest, 
                                       const std::string& clientIp) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    LOG_INFO_CTX("开始转发请求到AP", LogContext(requestId, clientIp, "", requestType + "@" + apEndpoint));
    
    // 解析AP端点（格式：http://localhost:8081）
    std::string host = "127.0.0.1";  // 直接使用IP地址
    int port = 8081;
    
    size_t hostStart = apEndpoint.find("://");
    if (hostStart != std::string::npos) {
        hostStart += 3;
        size_t portStart = apEndpoint.find(":", hostStart);
        if (portStart != std::string::npos) {
            std::string hostStr = apEndpoint.substr(hostStart, portStart - hostStart);
            // 将localhost转换为127.0.0.1
            if (hostStr == "localhost") {
                host = "127.0.0.1";
            } else {
                host = hostStr;
            }
            port = std::stoi(apEndpoint.substr(portStart + 1));
        } else {
            std::string hostStr = apEndpoint.substr(hostStart);
            if (hostStr == "localhost") {
                host = "127.0.0.1";
            } else {
                host = hostStr;
            }
        }
    }
    
    // 构造符合AP期望格式的JSON请求
    nlohmann::json messageJson;
    messageJson["type"] = requestType;
    messageJson["request_id"] = requestId;  // 添加请求ID
    
    // 解析请求体中的JSON数据并合并到请求中
    if (!httpRequest.body.empty()) {
        try {
            nlohmann::json bodyJson = nlohmann::json::parse(httpRequest.body);
            for (auto& item : bodyJson.items()) {
                messageJson[item.key()] = item.value();
            }
            LOG_DEBUG_CTX("解析请求体JSON成功", LogContext(requestId, clientIp));
        } catch (const std::exception& e) {
            LOG_WARNING_CTX("解析请求体JSON失败: " + std::string(e.what()), LogContext(requestId, clientIp));
        }
    }
    
    // 提取路径中的ID参数
    std::regex idPattern(R"(/api/\w+/(\d+))");
    std::smatch matches;
    if (std::regex_search(httpRequest.path, matches, idPattern)) {
        messageJson["id"] = std::stoi(matches[1].str());
        LOG_DEBUG_CTX("提取路径参数ID: " + matches[1].str(), LogContext(requestId, clientIp));
    }
    
    std::string jsonRequest = messageJson.dump();
    LOG_DEBUG_CTX("AP请求JSON: " + jsonRequest, LogContext(requestId, clientIp));
    
    // 使用socket连接AP
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_ERROR_DETAIL(requestId, "SocketError", "创建socket失败", "errno: " + std::to_string(errno));
        return "{\"error\":\"创建连接失败\",\"details\":\"socket creation failed\"}";
    }
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        LOG_ERROR_DETAIL(requestId, "AddressError", "IP地址转换失败", "host: " + host);
        close(sockfd);
        return "{\"error\":\"无效的服务器地址\",\"host\":\"" + host + "\"}";
    }
    
    // 设置连接超时
    struct timeval timeout;
    timeout.tv_sec = 5;  // 5秒超时
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG_ERROR_DETAIL(requestId, "ConnectionError", "连接AP服务失败", 
                        "host: " + host + ":" + std::to_string(port) + ", errno: " + std::to_string(errno));
        close(sockfd);
        return "{\"error\":\"连接处理服务失败\",\"endpoint\":\"" + host + ":" + std::to_string(port) + "\"}";
    }
    
    LOG_DEBUG_CTX("成功连接到AP服务", LogContext(requestId, clientIp, "", host + ":" + std::to_string(port)));
    
    // 发送请求
    ssize_t bytesSent = send(sockfd, jsonRequest.c_str(), jsonRequest.length(), 0);
    if (bytesSent < 0) {
        LOG_ERROR_DETAIL(requestId, "SendError", "发送请求失败", "errno: " + std::to_string(errno));
        close(sockfd);
        return "{\"error\":\"发送请求失败\"}";
    }
    
    LOG_DEBUG_CTX("请求发送成功, 字节数: " + std::to_string(bytesSent), LogContext(requestId, clientIp));
    
    // 接收响应
    char buffer[4096];
    ssize_t bytesReceived = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    std::string response;
    
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        response = std::string(buffer);
        LOG_DEBUG_CTX("收到AP响应, 字节数: " + std::to_string(bytesReceived), LogContext(requestId, clientIp));
        LOG_DEBUG_CTX("AP响应内容: " + response, LogContext(requestId, clientIp));
    } else {
        LOG_ERROR_DETAIL(requestId, "ReceiveError", "接收响应失败", "errno: " + std::to_string(errno));
        response = "{\"error\":\"接收响应失败\"}";
    }
    
    close(sockfd);
    
    // 计算AP调用时间
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    LOG_PERFORMANCE("AP调用", duration.count(), requestType + " -> " + host + ":" + std::to_string(port));
    
    return response;
}

std::string RequestHandler::extractClientIp(const std::string& requestData) {
    // 简单的IP提取逻辑，从HTTP头部提取
    std::istringstream stream(requestData);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (line.find("X-Forwarded-For:") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            std::string ip = line.substr(pos);
            ip.erase(0, ip.find_first_not_of(" \t"));
            ip.erase(ip.find_last_not_of(" \t\r\n") + 1);
            return ip;
        }
        if (line.find("X-Real-IP:") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            std::string ip = line.substr(pos);
            ip.erase(0, ip.find_first_not_of(" \t"));
            ip.erase(ip.find_last_not_of(" \t\r\n") + 1);
            return ip;
        }
    }
    
    return "127.0.0.1";  // 默认本地IP
}

// 解析HTTP请求
RequestHandler::HttpRequest RequestHandler::parseHttpRequest(const std::string& request) {
    HttpRequest httpRequest;
    
    std::istringstream stream(request);
    std::string line;
    
    // 解析请求行
    if (std::getline(stream, line)) {
        std::istringstream requestLine(line);
        std::string version;
        requestLine >> httpRequest.method >> httpRequest.path >> version;
        
        // 解析查询参数
        size_t queryPos = httpRequest.path.find('?');
        if (queryPos != std::string::npos) {
            httpRequest.query = httpRequest.path.substr(queryPos + 1);
            httpRequest.path = httpRequest.path.substr(0, queryPos);
        }
    }
    
    // 解析头部
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            
            // 去除前后空白
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            
            httpRequest.headers[key] = value;
        }
    }
    
    // 解析请求体
    std::ostringstream bodyStream;
    while (std::getline(stream, line)) {
        bodyStream << line << "\n";
    }
    httpRequest.body = bodyStream.str();
    
    // 去除末尾的换行符
    if (!httpRequest.body.empty() && httpRequest.body.back() == '\n') {
        httpRequest.body.pop_back();
    }
    
    return httpRequest;
}

// 确定用户请求类型
std::string RequestHandler::determineUserRequestType(const std::string& method, const std::string& path) {
    if (method == "GET") {
        std::regex idPattern(R"(/api/user/(\d+))");
        if (std::regex_match(path, idPattern)) {
            return "user.get";
        }
        return "user.list";
    } else if (method == "POST") {
        return "user.create";
    } else if (method == "PUT") {
        return "user.update";
    } else if (method == "DELETE") {
        return "user.delete";
    }
    return "user.unknown";
}

// 确定订单请求类型
std::string RequestHandler::determineOrderRequestType(const std::string& method, const std::string& path) {
    if (method == "GET") {
        std::regex idPattern(R"(/api/order/(\d+))");
        if (std::regex_match(path, idPattern)) {
            return "order.get";
        }
        return "order.list";
    } else if (method == "POST") {
        return "order.create";
    } else if (method == "PUT") {
        return "order.update";
    } else if (method == "PATCH") {
        if (path.find("/status") != std::string::npos) {
            return "order.updateStatus";
        }
        return "order.update";
    } else if (method == "DELETE") {
        return "order.delete";
    }
    return "order.unknown";
}

// 确定产品请求类型
std::string RequestHandler::determineProductRequestType(const std::string& method, const std::string& path) {
    if (method == "GET") {
        std::regex idPattern(R"(/api/product/(\d+))");
        if (std::regex_match(path, idPattern)) {
            return "product.get";
        }
        return "product.list";
    } else if (method == "POST") {
        return "product.create";
    } else if (method == "PUT") {
        return "product.update";
    } else if (method == "DELETE") {
        return "product.delete";
    }
    return "product.unknown";
}