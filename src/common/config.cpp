#include "common/config.h"
#include "common/logger_enhanced.h"
#include <fstream>
#include <sstream>

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

bool Config::loadConfig(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        LOG_ERROR("无法打开配置文件: " + configFile);
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // 跳过注释和空行
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // 解析键值对
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            // 去除前后空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            configMap[key] = value;
        }
    }
    
    LOG_INFO("配置文件加载成功: " + configFile);
    return true;
}

std::string Config::getString(const std::string& key, const std::string& defaultValue) const {
    auto it = configMap.find(key);
    if (it != configMap.end()) {
        return it->second;
    }
    return defaultValue;
}

int Config::getInt(const std::string& key, int defaultValue) const {
    auto it = configMap.find(key);
    if (it != configMap.end()) {
        try {
            return std::stoi(it->second);
        } catch (const std::exception& e) {
            LOG_ERROR("配置项 '" + key + "' 不是有效的整数: " + it->second);
        }
    }
    return defaultValue;
}

bool Config::getBool(const std::string& key, bool defaultValue) const {
    auto it = configMap.find(key);
    if (it != configMap.end()) {
        std::string value = it->second;
        // 转换为小写
        for (auto& c : value) {
            c = std::tolower(c);
        }
        
        if (value == "true" || value == "yes" || value == "1") {
            return true;
        } else if (value == "false" || value == "no" || value == "0") {
            return false;
        }
        
        LOG_ERROR("配置项 '" + key + "' 不是有效的布尔值: " + it->second);
    }
    return defaultValue;
}