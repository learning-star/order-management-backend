#include "ap/db_manager.h"
#include "common/logger_enhanced.h"
#include <sstream>

DBManager::DBManager() : mysql(nullptr), connected(false) {
}

DBManager::~DBManager() {
    disconnect();
}

DBManager& DBManager::getInstance() {
    static DBManager instance;
    return instance;
}

bool DBManager::connect(const std::string& host, const std::string& user, 
                        const std::string& password, const std::string& database, 
                        unsigned int port) {
    if (connected) {
        LOG_WARNING("数据库已连接");
        return true;
    }
    
    // 初始化MySQL
    mysql = mysql_init(nullptr);
    if (!mysql) {
        LOG_ERROR("MySQL初始化失败");
        return false;
    }
    
    // 设置连接选项
    bool reconnect = 1;
    mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);
    
    // 连接数据库
    if (!mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(), 
                           database.c_str(), port, nullptr, 0)) {
        LOG_ERROR("连接数据库失败: " + std::string(mysql_error(mysql)));
        mysql_close(mysql);
        mysql = nullptr;
        return false;
    }
    
    // 设置字符集
    mysql_set_character_set(mysql, "utf8mb4");
    
    connected = true;
    LOG_INFO("数据库连接成功: " + host + ":" + std::to_string(port) + "/" + database);
    return true;
}

void DBManager::disconnect() {
    if (mysql) {
        mysql_close(mysql);
        mysql = nullptr;
    }
    connected = false;
    LOG_INFO("数据库连接已关闭");
}

bool DBManager::executeQuery(const std::string& query) {
    if (!connected || !mysql) {
        LOG_ERROR("数据库未连接");
        return false;
    }
    
    LOG_INFO("执行SQL: " + query);
    
    int result = mysql_query(mysql, query.c_str());
    if (result != 0) {
        LOG_ERROR("SQL执行失败: " + std::string(mysql_error(mysql)));
        return false;
    }
    
    return true;
}

std::vector<std::vector<std::string>> DBManager::executeSelect(const std::string& query) {
    std::vector<std::vector<std::string>> results;
    
    if (!connected || !mysql) {
        LOG_ERROR("数据库未连接");
        return results;
    }
    
    LOG_INFO("执行SQL查询: " + query);
    
    if (mysql_query(mysql, query.c_str()) != 0) {
        LOG_ERROR("SQL查询失败: " + std::string(mysql_error(mysql)));
        return results;
    }
    
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) {
        LOG_ERROR("获取查询结果失败: " + std::string(mysql_error(mysql)));
        return results;
    }
    
    int numFields = mysql_num_fields(res);
    MYSQL_ROW row;
    
    while ((row = mysql_fetch_row(res))) {
        std::vector<std::string> rowData;
        for (int i = 0; i < numFields; i++) {
            rowData.push_back(row[i] ? row[i] : "NULL");
        }
        results.push_back(rowData);
    }
    
    mysql_free_result(res);
    return results;
}

unsigned long long DBManager::getLastInsertId() {
    if (!connected || !mysql) {
        LOG_ERROR("数据库未连接");
        return 0;
    }
    
    return mysql_insert_id(mysql);
}

unsigned long long DBManager::getAffectedRows() {
    if (!connected || !mysql) {
        LOG_ERROR("数据库未连接");
        return 0;
    }
    
    return mysql_affected_rows(mysql);
}

bool DBManager::beginTransaction() {
    return executeQuery("START TRANSACTION");
}

bool DBManager::commitTransaction() {
    return executeQuery("COMMIT");
}

bool DBManager::rollbackTransaction() {
    return executeQuery("ROLLBACK");
}

std::string DBManager::escapeString(const std::string& str) {
    if (!connected || !mysql) {
        LOG_ERROR("数据库未连接");
        return str;
    }
    
    char* buffer = new char[str.length() * 2 + 1];
    mysql_real_escape_string(mysql, buffer, str.c_str(), str.length());
    std::string result(buffer);
    delete[] buffer;
    
    return result;
}

std::string DBManager::getLastError() {
    if (!mysql) {
        return "MySQL未初始化";
    }
    
    return mysql_error(mysql);
}