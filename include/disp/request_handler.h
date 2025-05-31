#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

class RequestHandler {
public:
    // HTTP请求结构体
    struct HttpRequest {
        std::string method;
        std::string path;
        std::string query;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
    };
    
    static RequestHandler& getInstance();
    
    // 初始化处理器
    bool init();
    
    // 处理请求
    std::string handleRequest(const std::string& path, const std::string& requestData);
    
    // 注册处理函数
    using HandlerFunc = std::function<std::string(const std::string&)>;
    void registerHandler(const std::string& path, HandlerFunc handler);
    
    // 转发请求到AP
    std::string forwardToAp(const std::string& requestId, const std::string& apEndpoint,
                           const std::string& requestType, const HttpRequest& httpRequest,
                           const std::string& clientIp);
    
private:
    RequestHandler() = default;
    ~RequestHandler() = default;
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;
    
    // 请求ID生成
    std::string generateRequestId();
    
    // 提取客户端IP
    std::string extractClientIp(const std::string& requestData);
    
    // 处理API请求
    std::string handleApiRequest(const std::string& requestId, const std::string& path,
                                const std::string& requestData, const std::string& clientIp);
    
    // HTTP请求解析
    HttpRequest parseHttpRequest(const std::string& request);
    
    // 确定请求类型的方法
    std::string determineUserRequestType(const std::string& method, const std::string& path);
    std::string determineOrderRequestType(const std::string& method, const std::string& path);
    std::string determineProductRequestType(const std::string& method, const std::string& path);
    
    // 路径到处理函数的映射
    std::unordered_map<std::string, HandlerFunc> handlers;
    
    // AP服务地址映射
    std::unordered_map<std::string, std::string> apEndpoints;
};

#endif // REQUEST_HANDLER_H