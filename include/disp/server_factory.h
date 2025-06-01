#ifndef SERVER_FACTORY_H
#define SERVER_FACTORY_H

#include "disp/iserver.h"
#include <memory>

/**
 * 服务器工厂类
 * 根据配置创建不同类型的服务器实例
 */
class ServerFactory {
public:
    enum class ServerType {
        THREADED,   // 传统多线程服务器
        EPOLL       // Epoll事件驱动服务器
    };
    
    /**
     * 创建服务器实例
     * @param type 服务器类型
     * @param port 监听端口
     * @return 服务器实例的智能指针
     */
    static std::unique_ptr<IServer> createServer(ServerType type, int port);
    
    /**
     * 根据配置字符串创建服务器
     * @param typeStr 服务器类型字符串 ("threaded" 或 "epoll")
     * @param port 监听端口
     * @return 服务器实例的智能指针
     */
    static std::unique_ptr<IServer> createServer(const std::string& typeStr, int port);
    
    /**
     * 根据布尔值创建服务器
     * @param useEpoll true使用epoll，false使用多线程
     * @param port 监听端口
     * @return 服务器实例的智能指针
     */
    static std::unique_ptr<IServer> createServer(bool useEpoll, int port);
    
private:
    ServerFactory() = delete;  // 静态工厂类，禁止实例化
};

#endif // SERVER_FACTORY_H