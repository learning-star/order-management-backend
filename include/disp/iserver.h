#ifndef ISERVER_H
#define ISERVER_H

#include <string>
#include <functional>

/**
 * 服务器接口抽象类
 * 定义所有服务器实现必须提供的基本功能
 */
class IServer {
public:
    virtual ~IServer() = default;
    
    // 基本生命周期管理
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    
    // 路由管理
    using RequestHandler = std::function<std::string(const std::string&)>;
    virtual void setRoute(const std::string& path, RequestHandler handler) = 0;
    
    // 服务器配置
    virtual void setMaxConnections(int maxConn) = 0;
    virtual void setTimeout(int timeoutSec) = 0;
    
    // 服务器信息
    virtual std::string getServerType() const = 0;
    virtual int getPort() const = 0;
    virtual int getCurrentConnections() const = 0;
};

#endif // ISERVER_H