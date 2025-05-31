#include "ap/processor.h"
#include "ap/db_manager.h"
#include "common/logger_enhanced.h"
#include "common/config.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <cstring>
#include <sstream>
#include <chrono>
#include <random>
#include <nlohmann/json.hpp>

Processor& Processor::getInstance() {
    static Processor instance;
    return instance;
}

std::string Processor::generateRequestId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "AP" << timestamp << dis(gen);
    return ss.str();
}

bool Processor::init() {
    // 设置进程名称
    EnhancedLogger::getInstance().setProcessName("AP");
    
    LOG_SYSTEM("Processor", "初始化开始", "");
    // 注册用户相关的处理函数（更新为包含新字段）
    registerProcessor("user.get", [](const nlohmann::json& request) -> std::string {
        try {
            std::string id = request["id"];
            if (id.empty()) {
                return "{\"error\":\"用户ID不能为空\"}";
            }
            
            std::string query = "SELECT id, name, email, phone, role, status, created_at FROM users WHERE id = " + id;
            auto result = DBManager::getInstance().executeSelect(query);
            
            if (result.empty()) {
                return "{\"error\":\"用户不存在\"}";
            }
            
            nlohmann::json user;
            user["id"] = result[0][0];
            user["name"] = result[0][1];
            user["email"] = result[0][2];
            user["phone"] = result[0][3];
            user["role"] = result[0][4];
            user["status"] = result[0][5];
            user["created_at"] = result[0][6];
            
            return user.dump();
        } catch (const std::exception& e) {
            return "{\"error\":\"获取用户信息失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("user.list", [](const nlohmann::json& request) -> std::string {
        try {
            std::string query = "SELECT id, name, email, phone, role, status, created_at FROM users ORDER BY id DESC";
            auto result = DBManager::getInstance().executeSelect(query);
            
            nlohmann::json users = nlohmann::json::array();
            for (const auto& row : result) {
                nlohmann::json user;
                user["id"] = row[0];
                user["name"] = row[1];
                user["email"] = row[2];
                user["phone"] = row[3];
                user["role"] = row[4];
                user["status"] = row[5];
                user["created_at"] = row[6];
                users.push_back(user);
            }
            
            return users.dump();
        } catch (const std::exception& e) {
            return "{\"error\":\"获取用户列表失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("user.create", [](const nlohmann::json& request) -> std::string {
        try {
            std::string name = request.value("name", "");
            std::string email = request.value("email", "");
            std::string phone = request.value("phone", "");
            std::string role = request.value("role", "employee");
            std::string status = request.value("status", "active");
            std::string password = request.value("password", "default123");  // 默认密码
            
            if (name.empty() || email.empty()) {
                return "{\"error\":\"用户名和邮箱不能为空\"}";
            }
            
            // 转义字符串防止SQL注入
            name = DBManager::getInstance().escapeString(name);
            email = DBManager::getInstance().escapeString(email);
            phone = DBManager::getInstance().escapeString(phone);
            role = DBManager::getInstance().escapeString(role);
            status = DBManager::getInstance().escapeString(status);
            password = DBManager::getInstance().escapeString(password);
            
            std::string query = "INSERT INTO users (name, email, phone, password, role, status, created_at) VALUES ('" +
                               name + "', '" + email + "', '" + phone + "', '" + password + "', '" + 
                               role + "', '" + status + "', NOW())";
            
            if (DBManager::getInstance().executeQuery(query)) {
                unsigned long long id = DBManager::getInstance().getLastInsertId();
                nlohmann::json user;
                user["id"] = id;
                user["name"] = request["name"];
                user["email"] = request["email"];
                user["phone"] = request["phone"];
                user["role"] = request["role"];
                user["status"] = request["status"];
                user["success"] = true;
                return user.dump();
            } else {
                return "{\"error\":\"创建用户失败\"}";
            }
        } catch (const std::exception& e) {
            return "{\"error\":\"创建用户失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("user.update", [](const nlohmann::json& request) -> std::string {
        try {
            std::string id = request["id"];
            if (id.empty()) {
                return "{\"error\":\"用户ID不能为空\"}";
            }
            
            // 构建更新字段
            std::vector<std::string> updates;
            if (request.contains("name") && !request["name"].get<std::string>().empty()) {
                updates.push_back("name = '" + DBManager::getInstance().escapeString(request["name"]) + "'");
            }
            if (request.contains("email") && !request["email"].get<std::string>().empty()) {
                updates.push_back("email = '" + DBManager::getInstance().escapeString(request["email"]) + "'");
            }
            if (request.contains("phone")) {
                updates.push_back("phone = '" + DBManager::getInstance().escapeString(request["phone"]) + "'");
            }
            if (request.contains("role") && !request["role"].get<std::string>().empty()) {
                updates.push_back("role = '" + DBManager::getInstance().escapeString(request["role"]) + "'");
            }
            if (request.contains("status") && !request["status"].get<std::string>().empty()) {
                updates.push_back("status = '" + DBManager::getInstance().escapeString(request["status"]) + "'");
            }
            
            if (updates.empty()) {
                return "{\"error\":\"没有要更新的字段\"}";
            }
            
            std::string updateFields;
            for (size_t i = 0; i < updates.size(); ++i) {
                if (i > 0) updateFields += ", ";
                updateFields += updates[i];
            }
            
            std::string query = "UPDATE users SET " + updateFields +
                               ", updated_at = NOW() WHERE id = " + id;
            
            if (DBManager::getInstance().executeQuery(query)) {
                if (DBManager::getInstance().getAffectedRows() > 0) {
                    return "{\"success\":true,\"message\":\"用户更新成功\"}";
                } else {
                    return "{\"error\":\"用户不存在\"}";
                }
            } else {
                return "{\"error\":\"更新用户失败\"}";
            }
        } catch (const std::exception& e) {
            return "{\"error\":\"更新用户失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("user.delete", [](const nlohmann::json& request) -> std::string {
        try {
            std::string id = request["id"];
            if (id.empty()) {
                return "{\"error\":\"用户ID不能为空\"}";
            }
            
            std::string query = "DELETE FROM users WHERE id = " + id;
            
            if (DBManager::getInstance().executeQuery(query)) {
                if (DBManager::getInstance().getAffectedRows() > 0) {
                    return "{\"success\":true,\"message\":\"用户删除成功\"}";
                } else {
                    return "{\"error\":\"用户不存在\"}";
                }
            } else {
                return "{\"error\":\"删除用户失败\"}";
            }
        } catch (const std::exception& e) {
            return "{\"error\":\"删除用户失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    // 注册产品相关的处理函数（更新为包含新字段）
    registerProcessor("product.list", [](const nlohmann::json& request) -> std::string {
        try {
            std::string query = "SELECT id, name, category, description, price, stock, status, created_at FROM products ORDER BY id DESC";
            auto result = DBManager::getInstance().executeSelect(query);
            
            nlohmann::json products = nlohmann::json::array();
            for (const auto& row : result) {
                nlohmann::json product;
                product["id"] = row[0];
                product["name"] = row[1];
                product["category"] = row[2];
                product["description"] = row[3];
                product["price"] = row[4];
                product["stock"] = row[5];
                product["status"] = row[6];
                product["created_at"] = row[7];
                products.push_back(product);
            }
            
            return products.dump();
        } catch (const std::exception& e) {
            return "{\"error\":\"获取产品列表失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("product.get", [](const nlohmann::json& request) -> std::string {
        try {
            std::string id = request["id"];
            if (id.empty()) {
                return "{\"error\":\"产品ID不能为空\"}";
            }
            
            std::string query = "SELECT id, name, category, description, price, stock, status, created_at FROM products WHERE id = " + id;
            auto result = DBManager::getInstance().executeSelect(query);
            
            if (result.empty()) {
                return "{\"error\":\"产品不存在\"}";
            }
            
            nlohmann::json product;
            product["id"] = result[0][0];
            product["name"] = result[0][1];
            product["category"] = result[0][2];
            product["description"] = result[0][3];
            product["price"] = result[0][4];
            product["stock"] = result[0][5];
            product["status"] = result[0][6];
            product["created_at"] = result[0][7];
            
            return product.dump();
        } catch (const std::exception& e) {
            return "{\"error\":\"获取产品信息失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("product.create", [](const nlohmann::json& request) -> std::string {
        try {
            std::string name = request.value("name", "");
            std::string category = request.value("category", "electronics");
            std::string description = request.value("description", "");
            std::string price = request.value("price", "0.0");
            std::string stock = request.value("stock", "0");
            std::string status = request.value("status", "active");
            
            if (name.empty() || price.empty()) {
                return "{\"error\":\"产品名称和价格不能为空\"}";
            }
            
            // 验证价格格式和范围
            try {
                double priceValue = std::stod(price);
                if (priceValue < 0.0) {
                    return "{\"error\":\"价格不能小于0\"}";
                }
            } catch (const std::exception& e) {
                return "{\"error\":\"价格格式无效\"}";
            }
            
            name = DBManager::getInstance().escapeString(name);
            category = DBManager::getInstance().escapeString(category);
            description = DBManager::getInstance().escapeString(description);
            status = DBManager::getInstance().escapeString(status);
            
            std::string query = "INSERT INTO products (name, category, description, price, stock, status, created_at) VALUES ('" +
                               name + "', '" + category + "', '" + description + "', '" + price +
                               "', '" + stock + "', '" + status + "', NOW())";
            
            if (DBManager::getInstance().executeQuery(query)) {
                unsigned long long id = DBManager::getInstance().getLastInsertId();
                nlohmann::json product;
                product["id"] = id;
                product["name"] = request["name"];
                product["category"] = request["category"];
                product["description"] = request["description"];
                product["price"] = price;
                product["stock"] = stock;
                product["status"] = request["status"];
                product["success"] = true;
                return product.dump();
            } else {
                return "{\"error\":\"创建产品失败\"}";
            }
        } catch (const std::exception& e) {
            return "{\"error\":\"创建产品失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("product.update", [](const nlohmann::json& request) -> std::string {
        try {
            std::string id = request["id"];
            if (id.empty()) {
                return "{\"error\":\"产品ID不能为空\"}";
            }
            
            std::vector<std::string> updates;
            
            if (request.contains("name") && !request["name"].get<std::string>().empty()) {
                updates.push_back("name = '" + DBManager::getInstance().escapeString(request["name"]) + "'");
            }
            if (request.contains("category") && !request["category"].get<std::string>().empty()) {
                updates.push_back("category = '" + DBManager::getInstance().escapeString(request["category"]) + "'");
            }
            if (request.contains("description")) {
                updates.push_back("description = '" + DBManager::getInstance().escapeString(request["description"]) + "'");
            }
            if (request.contains("price") && !request["price"].get<std::string>().empty()) {
                std::string price = request["price"].get<std::string>();
                updates.push_back("price = '" + DBManager::getInstance().escapeString(price) + "'");
            }
            if (request.contains("stock") && !request["stock"].get<std::string>().empty()) {
                std::string stock = request["stock"].get<std::string>();
                updates.push_back("stock = '" + DBManager::getInstance().escapeString(stock) + "'");
            }
            if (request.contains("status") && !request["status"].get<std::string>().empty()) {
                updates.push_back("status = '" + DBManager::getInstance().escapeString(request["status"]) + "'");
            }
            
            if (updates.empty()) {
                return "{\"error\":\"没有要更新的字段\"}";
            }
            
            std::string updateFields;
            for (size_t i = 0; i < updates.size(); ++i) {
                if (i > 0) updateFields += ", ";
                updateFields += updates[i];
            }
            
            std::string query = "UPDATE products SET " + updateFields +
                               ", updated_at = NOW() WHERE id = " + id;
            
            if (DBManager::getInstance().executeQuery(query)) {
                if (DBManager::getInstance().getAffectedRows() > 0) {
                    return "{\"success\":true,\"message\":\"产品更新成功\"}";
                } else {
                    return "{\"error\":\"产品不存在\"}";
                }
            } else {
                return "{\"error\":\"更新产品失败\"}";
            }
        } catch (const std::exception& e) {
            return "{\"error\":\"更新产品失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("product.delete", [](const nlohmann::json& request) -> std::string {
        try {
            std::string id = request["id"];
            if (id.empty()) {
                return "{\"error\":\"产品ID不能为空\"}";
            }
            
            std::string query = "DELETE FROM products WHERE id = " + id;
            
            if (DBManager::getInstance().executeQuery(query)) {
                if (DBManager::getInstance().getAffectedRows() > 0) {
                    return "{\"success\":true,\"message\":\"产品删除成功\"}";
                } else {
                    return "{\"error\":\"产品不存在\"}";
                }
            } else {
                return "{\"error\":\"删除产品失败\"}";
            }
        } catch (const std::exception& e) {
            return "{\"error\":\"删除产品失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    // 注册订单相关的处理函数（更新为简化的结构）
    registerProcessor("order.list", [](const nlohmann::json& request) -> std::string {
        try {
            std::string query = "SELECT id, user_id, customer_name, product_name, quantity, total_amount, status, created_at FROM orders ORDER BY id DESC";
            auto result = DBManager::getInstance().executeSelect(query);
            
            nlohmann::json orders = nlohmann::json::array();
            for (const auto& row : result) {
                nlohmann::json order;
                order["id"] = row[0];
                order["user_id"] = row[1];
                order["customer_name"] = row[2];
                order["product_name"] = row[3];
                order["quantity"] = row[4];
                order["total_amount"] = row[5];
                order["status"] = row[6];
                order["created_at"] = row[7];
                orders.push_back(order);
            }
            
            return orders.dump();
        } catch (const std::exception& e) {
            return "{\"error\":\"获取订单列表失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("order.get", [](const nlohmann::json& request) -> std::string {
        try {
            std::string id = request["id"];
            if (id.empty()) {
                return "{\"error\":\"订单ID不能为空\"}";
            }
            
            std::string query = "SELECT id, user_id, customer_name, product_name, quantity, total_amount, status, created_at FROM orders WHERE id = " + id;
            auto result = DBManager::getInstance().executeSelect(query);
            
            if (result.empty()) {
                return "{\"error\":\"订单不存在\"}";
            }
            
            nlohmann::json order;
            order["id"] = result[0][0];
            order["user_id"] = result[0][1];
            order["customer_name"] = result[0][2];
            order["product_name"] = result[0][3];
            order["quantity"] = result[0][4];
            order["total_amount"] = result[0][5];
            order["status"] = result[0][6];
            order["created_at"] = result[0][7];
            
            return order.dump();
        } catch (const std::exception& e) {
            return "{\"error\":\"获取订单信息失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("order.create", [](const nlohmann::json& request) -> std::string {
        try {
            std::string customerName = request.value("customer_name", "");
            std::string productName = request.value("product_name", "");
            std::string quantity = request.value("quantity", "1");
            std::string totalAmount = request.value("total_amount", "0.0");
            std::string status = request.value("status", "pending");
            std::string userId = request.value("user_id", "1"); // 默认用户ID
            
            if (customerName.empty() || productName.empty() || quantity.empty() || totalAmount.empty()) {
                return "{\"error\":\"客户名称、产品名称、数量和总金额不能为空\"}";
            }
            
            customerName = DBManager::getInstance().escapeString(customerName);
            productName = DBManager::getInstance().escapeString(productName);
            status = DBManager::getInstance().escapeString(status);
            
            std::string query = "INSERT INTO orders (user_id, customer_name, product_name, quantity, total_amount, status, created_at) VALUES ('" +
                               userId + "', '" + customerName + "', '" + productName + "', '" +
                               quantity + "', '" + totalAmount + "', '" + status + "', NOW())";
            
            if (DBManager::getInstance().executeQuery(query)) {
                unsigned long long id = DBManager::getInstance().getLastInsertId();
                nlohmann::json order;
                order["id"] = id;
                order["user_id"] = userId;
                order["customer_name"] = request["customer_name"];
                order["product_name"] = request["product_name"];
                order["quantity"] = quantity;
                order["total_amount"] = totalAmount;
                order["status"] = status;
                order["success"] = true;
                return order.dump();
            } else {
                return "{\"error\":\"创建订单失败\"}";
            }
        } catch (const std::exception& e) {
            return "{\"error\":\"创建订单失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("order.update", [](const nlohmann::json& request) -> std::string {
        try {
            std::string id = request["id"];
            if (id.empty()) {
                return "{\"error\":\"订单ID不能为空\"}";
            }
            
            std::vector<std::string> updates;
            
            if (request.contains("customer_name") && !request["customer_name"].get<std::string>().empty()) {
                updates.push_back("customer_name = '" + DBManager::getInstance().escapeString(request["customer_name"]) + "'");
            }
            if (request.contains("product_name") && !request["product_name"].get<std::string>().empty()) {
                updates.push_back("product_name = '" + DBManager::getInstance().escapeString(request["product_name"]) + "'");
            }
            if (request.contains("quantity") && !request["quantity"].get<std::string>().empty()) {
                std::string quantity = request["quantity"].get<std::string>();
                updates.push_back("quantity = '" + DBManager::getInstance().escapeString(quantity) + "'");
            }
            if (request.contains("total_amount") && !request["total_amount"].get<std::string>().empty()) {
                std::string totalAmount = request["total_amount"].get<std::string>();
                updates.push_back("total_amount = '" + DBManager::getInstance().escapeString(totalAmount) + "'");
            }
            if (request.contains("status") && !request["status"].get<std::string>().empty()) {
                updates.push_back("status = '" + DBManager::getInstance().escapeString(request["status"]) + "'");
            }
            
            if (updates.empty()) {
                return "{\"error\":\"没有要更新的字段\"}";
            }
            
            std::string updateFields;
            for (size_t i = 0; i < updates.size(); ++i) {
                if (i > 0) updateFields += ", ";
                updateFields += updates[i];
            }
            
            std::string query = "UPDATE orders SET " + updateFields +
                               ", updated_at = NOW() WHERE id = " + id;
            
            if (DBManager::getInstance().executeQuery(query)) {
                if (DBManager::getInstance().getAffectedRows() > 0) {
                    return "{\"success\":true,\"message\":\"订单更新成功\"}";
                } else {
                    return "{\"error\":\"订单不存在\"}";
                }
            } else {
                return "{\"error\":\"更新订单失败\"}";
            }
        } catch (const std::exception& e) {
            return "{\"error\":\"更新订单失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("order.updateStatus", [](const nlohmann::json& request) -> std::string {
        try {
            std::string id = request["id"];
            std::string status = request.value("status", "");
            
            if (id.empty() || status.empty()) {
                return "{\"error\":\"订单ID和状态不能为空\"}";
            }
            
            std::string query = "UPDATE orders SET status = '" + DBManager::getInstance().escapeString(status) +
                               "', updated_at = NOW() WHERE id = " + id;
            
            if (DBManager::getInstance().executeQuery(query)) {
                if (DBManager::getInstance().getAffectedRows() > 0) {
                    return "{\"success\":true,\"message\":\"订单状态更新成功\"}";
                } else {
                    return "{\"error\":\"订单不存在\"}";
                }
            } else {
                return "{\"error\":\"更新订单状态失败\"}";
            }
        } catch (const std::exception& e) {
            return "{\"error\":\"更新订单状态失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    registerProcessor("order.delete", [](const nlohmann::json& request) -> std::string {
        try {
            std::string id = request["id"];
            if (id.empty()) {
                return "{\"error\":\"订单ID不能为空\"}";
            }
            
            std::string query = "DELETE FROM orders WHERE id = " + id;
            
            if (DBManager::getInstance().executeQuery(query)) {
                if (DBManager::getInstance().getAffectedRows() > 0) {
                    return "{\"success\":true,\"message\":\"订单删除成功\"}";
                } else {
                    return "{\"error\":\"订单不存在\"}";
                }
            } else {
                return "{\"error\":\"删除订单失败\"}";
            }
        } catch (const std::exception& e) {
            return "{\"error\":\"删除订单失败: " + std::string(e.what()) + "\"}";
        }
    });
    
    LOG_SYSTEM("Processor", "初始化完成", "已注册 " + std::to_string(processors.size()) + " 个处理函数");
    return true;
}

std::string Processor::processRequest(const std::string& requestType, const nlohmann::json& requestData) {
    // 生成或提取请求ID
    std::string requestId = requestData.value("request_id", generateRequestId());
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    LOG_API_CALL(requestId, "AP", requestType, "处理请求");
    
    // 查找对应的处理函数
    auto it = processors.find(requestType);
    if (it != processors.end()) {
        try {
            std::string response = it->second(requestData);
            
            // 计算处理时间
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            LOG_PERFORMANCE("请求处理", duration.count(), requestType);
            LOG_DEBUG_CTX("处理成功: " + requestType, LogContext(requestId));
            
            return response;
        } catch (const std::exception& e) {
            LOG_ERROR_DETAIL(requestId, "RequestProcessing", "处理请求异常",
                           requestType + ": " + std::string(e.what()));
            return "{\"error\":\"处理请求时发生异常\",\"details\":\"" + std::string(e.what()) + "\"}";
        }
    }
    
    LOG_WARNING_CTX("未知的请求类型: " + requestType, LogContext(requestId));
    return "{\"error\":\"未知的请求类型\",\"type\":\"" + requestType + "\"}";
}

void Processor::registerProcessor(const std::string& requestType, ProcessFunc processor) {
    processors[requestType] = processor;
    LOG_SYSTEM("Processor", "注册处理函数", requestType);
}

bool Processor::startService(int port) {
    if (running) {
        LOG_WARNING_CTX("服务已经在运行中", LogContext("", "", "", "port:" + std::to_string(servicePort)));
        return true;
    }
    
    LOG_SYSTEM("Processor", "启动服务", "端口: " + std::to_string(port));
    
    // 创建套接字
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        LOG_ERROR_DETAIL("", "SocketError", "创建套接字失败", "errno: " + std::to_string(errno));
        return false;
    }
    
    // 设置套接字选项
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR_DETAIL("", "SocketError", "设置套接字选项失败", "errno: " + std::to_string(errno));
        close(serverSocket);
        return false;
    }
    
    // 绑定地址
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG_ERROR_DETAIL("", "BindError", "绑定地址失败",
                        "端口: " + std::to_string(port) + ", errno: " + std::to_string(errno));
        close(serverSocket);
        return false;
    }
    
    // 监听连接
    if (listen(serverSocket, 10) < 0) {
        LOG_ERROR_DETAIL("", "ListenError", "监听连接失败", "errno: " + std::to_string(errno));
        close(serverSocket);
        return false;
    }
    
    // 保存端口
    servicePort = port;
    running = true;
    
    // 启动服务线程
    std::thread serviceThread(&Processor::serviceLoop, this, serverSocket);
    serviceThread.detach();
    
    LOG_SYSTEM("Processor", "服务启动成功", "监听端口: " + std::to_string(port));
    return true;
}

void Processor::stopService() {
    running = false;
    LOG_SYSTEM("Processor", "服务停止", "端口: " + std::to_string(servicePort));
}

bool Processor::isRunning()
{
    return this->running;
}

void Processor::serviceLoop(int serverSocket)
{
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    LOG_SYSTEM("ServiceLoop", "服务循环启动", "等待客户端连接");
    
    while (running) {
        // 接受连接
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (running) {
                LOG_ERROR_DETAIL("", "AcceptError", "接受连接失败", "errno: " + std::to_string(errno));
            }
            continue;
        }
        
        // 获取客户端IP地址
        char clientIpStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIpStr, INET_ADDRSTRLEN);
        std::string clientIp(clientIpStr);
        
        LOG_DEBUG_CTX("接受新连接", LogContext("", clientIp, "", ""));
        
        // 处理客户端请求
        const int bufferSize = 4096;
        char buffer[bufferSize];
        
        // 接收请求
        ssize_t bytesRead = recv(clientSocket, buffer, bufferSize - 1, 0);
        if (bytesRead <= 0) {
            LOG_DEBUG_CTX("客户端连接关闭或接收失败", LogContext("", clientIp));
            close(clientSocket);
            continue;
        }
        
        // 确保字符串以null结尾
        buffer[bytesRead] = '\0';
        
        // 解析JSON请求
        std::string request(buffer);
        std::string response;
        std::string requestId;
        
        LOG_DEBUG_CTX("收到请求数据, 字节数: " + std::to_string(bytesRead), LogContext("", clientIp));
        
        try {
            nlohmann::json jsonRequest = nlohmann::json::parse(request);
            std::string requestType = jsonRequest.value("type", "unknown");
            requestId = jsonRequest.value("request_id", generateRequestId());
            
            LOG_REQUEST(requestId, "JSON", requestType, clientIp);
            LOG_DEBUG_CTX("请求内容: " + request, LogContext(requestId, clientIp));
            
            // 处理请求
            response = processRequest(requestType, jsonRequest);
            
            LOG_DEBUG_CTX("响应内容: " + response, LogContext(requestId, clientIp));
            
        } catch (const nlohmann::json::parse_error& e) {
            requestId = generateRequestId();
            LOG_ERROR_DETAIL(requestId, "JSONParseError", "JSON解析错误",
                           "客户端: " + clientIp + ", 错误: " + std::string(e.what()));
            LOG_DEBUG_CTX("原始请求数据: " + request, LogContext(requestId, clientIp));
            response = "{\"error\":\"JSON解析错误\",\"details\":\"" + std::string(e.what()) + "\"}";
        }
        
        // 发送响应
        ssize_t bytesSent = send(clientSocket, response.c_str(), response.length(), 0);
        if (bytesSent < 0) {
            LOG_ERROR_DETAIL(requestId, "SendError", "发送响应失败",
                           "客户端: " + clientIp + ", errno: " + std::to_string(errno));
        } else {
            LOG_DEBUG_CTX("响应发送成功, 字节数: " + std::to_string(bytesSent), LogContext(requestId, clientIp));
        }
        
        // 关闭连接
        close(clientSocket);
        LOG_DEBUG_CTX("连接关闭", LogContext(requestId, clientIp));
    }
    
    // 关闭服务器套接字
    close(serverSocket);
    LOG_SYSTEM("ServiceLoop", "服务循环结束", "服务器套接字已关闭");
}