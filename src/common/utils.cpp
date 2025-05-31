#include "common/utils.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace Utils {

// 字符串处理函数
std::string trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](int c) {
        return std::isspace(c);
    });
    
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](int c) {
        return std::isspace(c);
    }).base();
    
    return (start < end) ? std::string(start, end) : std::string();
}

std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    
    while ((pos = str.find(delimiter, prev)) != std::string::npos) {
        tokens.push_back(str.substr(prev, pos - prev));
        prev = pos + delimiter.length();
    }
    
    tokens.push_back(str.substr(prev));
    return tokens;
}

std::string join(const std::vector<std::string>& elements, const std::string& delimiter) {
    std::ostringstream oss;
    
    for (size_t i = 0; i < elements.size(); ++i) {
        oss << elements[i];
        if (i < elements.size() - 1) {
            oss << delimiter;
        }
    }
    
    return oss.str();
}

// 文件操作函数
bool fileExists(const std::string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool writeFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    return file.good();
}

// 时间相关函数
std::string getCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    
    return ss.str();
}

std::string formatTime(time_t time) {
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    
    return ss.str();
}

// 网络相关函数
bool isValidIpAddress(const std::string& ipAddress) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) != 0;
}

bool isPortAvailable(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    
    return result == 0;
}

// 加密相关函数
std::string md5(const std::string& input) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    
    // 使用EVP接口代替直接调用MD5函数
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_md5();
    
    EVP_DigestInit_ex(context, md, nullptr);
    EVP_DigestUpdate(context, input.c_str(), input.length());
    EVP_DigestFinal_ex(context, digest, nullptr);
    EVP_MD_CTX_free(context);
    
    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    
    return ss.str();
}

std::string sha256(const std::string& input) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input.c_str(), input.length(), digest);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    
    return ss.str();
}

// JSON处理函数
std::string escapeJson(const std::string& input) {
    std::ostringstream oss;
    
    for (auto c : input) {
        switch (c) {
            case '\"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '/':  oss << "\\/"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (c < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    oss << c;
                }
        }
    }
    
    return oss.str();
}

} // namespace Utils