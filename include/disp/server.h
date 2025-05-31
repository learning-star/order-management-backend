#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <unordered_map>

class Server {
public:
    Server(int port);
    ~Server();
    
    bool start();
    void stop();
    bool isRunning() const;
    
    // 设置路由处理函数
    using RequestHandler = std::function<std::string(const std::string&)>;
    void setRoute(const std::string& path, RequestHandler handler);
    
private:
    void acceptLoop();
    void handleClient(int clientSocket);
    std::string processRequest(const std::string& request);
    void parseRequest(const std::string& request, std::string& method, std::string& path);
    std::string parseRequestPath(const std::string& request);
    std::string createOptionsResponse();
    std::string createResponse(const std::string& content, int statusCode = 200, const std::string& contentType = "application/json");
    
    int serverSocket;
    int port;
    std::atomic<bool> running;
    std::thread acceptThread;
    std::vector<std::thread> clientThreads;
    std::unordered_map<std::string, RequestHandler> routes;
};

#endif // SERVER_H