#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

class Processor {
public:
    static Processor& getInstance();
    
    // 初始化处理器
    bool init();
    
    // 处理请求
    std::string processRequest(const std::string& requestType, const nlohmann::json& requestData);
    
    // 注册处理函数
    using ProcessFunc = std::function<std::string(const nlohmann::json&)>;
    void registerProcessor(const std::string& requestType, ProcessFunc processor);
    
    // 启动处理服务
    bool startService(int port);
    
    // 停止处理服务
    void stopService();

    // 获取ap运行状态
    bool isRunning();
    
private:
    Processor() = default;
    ~Processor() = default;
    Processor(const Processor&) = delete;
    Processor& operator=(const Processor&) = delete;
    
    // 请求类型到处理函数的映射
    std::unordered_map<std::string, ProcessFunc> processors;
    
    // 请求ID生成
    std::string generateRequestId();
    
    // 服务相关
    int servicePort;
    bool running;
    void serviceLoop(int serverSocket);
};

#endif // PROCESSOR_H