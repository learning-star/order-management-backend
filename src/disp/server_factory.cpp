#include "disp/server_factory.h"
#include "disp/threaded_server.h"
#include "disp/server_epoll.h"
#include "common/logger_enhanced.h"
#include <algorithm>

std::unique_ptr<IServer> ServerFactory::createServer(ServerType type, int port) {
    switch (type) {
        case ServerType::THREADED:
            LOG_INFO("创建ThreadedServer实例 (端口: " + std::to_string(port) + ")");
            return std::make_unique<ThreadedServer>(port);
            
        case ServerType::EPOLL:
            LOG_INFO("创建EpollServer实例 (端口: " + std::to_string(port) + ")");
            return std::make_unique<EpollServer>(port);
            
        default:
            LOG_ERROR("未知的服务器类型");
            return nullptr;
    }
}

std::unique_ptr<IServer> ServerFactory::createServer(const std::string& typeStr, int port) {
    std::string lowerTypeStr = typeStr;
    std::transform(lowerTypeStr.begin(), lowerTypeStr.end(), lowerTypeStr.begin(), ::tolower);
    
    if (lowerTypeStr == "threaded" || lowerTypeStr == "thread") {
        return createServer(ServerType::THREADED, port);
    } else if (lowerTypeStr == "epoll") {
        return createServer(ServerType::EPOLL, port);
    } else {
        LOG_ERROR("无效的服务器类型字符串: " + typeStr + "，支持的类型: threaded, epoll");
        LOG_INFO("回退到默认的ThreadedServer");
        return createServer(ServerType::THREADED, port);
    }
}

std::unique_ptr<IServer> ServerFactory::createServer(bool useEpoll, int port) {
    if (useEpoll) {
        return createServer(ServerType::EPOLL, port);
    } else {
        return createServer(ServerType::THREADED, port);
    }
}