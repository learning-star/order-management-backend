#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

namespace Utils {
    // 字符串处理函数
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    std::string join(const std::vector<std::string>& elements, const std::string& delimiter);
    
    // 文件操作函数
    bool fileExists(const std::string& filename);
    std::string readFile(const std::string& filename);
    bool writeFile(const std::string& filename, const std::string& content);
    
    // 时间相关函数
    std::string getCurrentTimeString();
    std::string formatTime(time_t time);
    
    // 网络相关函数
    bool isValidIpAddress(const std::string& ipAddress);
    bool isPortAvailable(int port);
    
    // 加密相关函数
    std::string md5(const std::string& input);
    std::string sha256(const std::string& input);
    
    // JSON处理函数
    std::string escapeJson(const std::string& input);
}

#endif // UTILS_H