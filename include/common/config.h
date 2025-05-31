#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <memory>

class Config {
public:
    static Config& getInstance();
    
    bool loadConfig(const std::string& configFile);
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;

private:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    std::map<std::string, std::string> configMap;
};

#endif // CONFIG_H