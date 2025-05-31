#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <string>
#include <vector>
#include <memory>
// 根据系统配置可能需要调整MySQL头文件路径
#ifdef _WIN32
#include <mysql.h>
#else
#include <mysql/mysql.h>
#endif

class DBManager {
public:
    static DBManager& getInstance();
    
    // 连接数据库
    bool connect(const std::string& host, const std::string& user, 
                 const std::string& password, const std::string& database, 
                 unsigned int port = 3306);
    
    // 断开连接
    void disconnect();
    
    // 执行查询
    bool executeQuery(const std::string& query);
    
    // 执行查询并获取结果
    std::vector<std::vector<std::string>> executeSelect(const std::string& query);
    
    // 获取最后插入的ID
    unsigned long long getLastInsertId();
    
    // 获取受影响的行数
    unsigned long long getAffectedRows();
    
    // 事务操作
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    
    // 转义字符串
    std::string escapeString(const std::string& str);
    
    // 获取错误信息
    std::string getLastError();
    
private:
    DBManager();
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;
    
    MYSQL* mysql;
    bool connected;
};

#endif // DB_MANAGER_H