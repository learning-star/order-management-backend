# 增强日志系统使用指南

## 概述

本增强日志系统为DISP和AP进程提供了清晰、结构化的日志记录功能，便于调试和监控分布式系统的运行状态。

## 主要特性

### 🎨 彩色输出
- **绿色**: 成功操作和信息日志
- **黄色**: 警告信息
- **红色**: 错误信息
- **蓝色**: 调试信息
- **青色**: 系统事件

### 📝 结构化日志
每条日志包含以下信息：
- **时间戳**: 精确到毫秒
- **进程名称**: DISP 或 AP
- **线程ID**: 便于多线程调试
- **日志级别**: INFO, WARNING, ERROR, DEBUG
- **Emoji指示器**: 快速识别日志类型
- **上下文信息**: 请求ID、客户端IP、用户ID等

### 🔍 请求追踪
- **请求ID**: 唯一标识每个请求，格式：REQ{timestamp}{random}
- **客户端IP**: 记录请求来源
- **操作类型**: 标识具体的业务操作
- **性能监控**: 记录操作耗时

## 日志类型

### 1. HTTP请求/响应日志
```cpp
LOG_REQUEST(requestId, method, path, clientIp);
LOG_RESPONSE(requestId, statusCode, responseSize, duration);
```

### 2. API调用日志
```cpp
LOG_API_CALL(requestId, service, operation, details);
```

### 3. 数据库操作日志
```cpp
LOG_DB_QUERY(requestId, operation, table, query);
LOG_DB_RESULT(requestId, operation, affectedRows, duration);
```

### 4. 性能监控日志
```cpp
LOG_PERFORMANCE(operation, duration, details);
```

### 5. 系统事件日志
```cpp
LOG_SYSTEM(component, event, details);
```

### 6. 错误详情日志
```cpp
LOG_ERROR_DETAIL(requestId, errorType, description, details);
```

## 日志级别

- **DEBUG**: 详细的调试信息
- **INFO**: 一般信息
- **WARNING**: 警告信息
- **ERROR**: 错误信息

## 日志输出示例

```
[2024-12-23 14:30:15.123] [DISP] [Thread-1234] [INFO] 🚀 RequestHandler: 初始化开始 - 
[2024-12-23 14:30:15.125] [DISP] [Thread-1234] [INFO] ⚙️  RequestHandler: 配置AP端点 - user -> http://localhost:8081
[2024-12-23 14:30:15.127] [DISP] [Thread-1234] [INFO] 🚀 RequestHandler: 初始化完成 - 已注册 2 个处理函数

[2024-12-23 14:30:20.001] [DISP] [Thread-5678] [INFO] 📥 [REQ1703311820001234] GET /api/user/list from 192.168.1.100
[2024-12-23 14:30:20.002] [DISP] [Thread-5678] [INFO] 🔧 [REQ1703311820001234] AP调用: user.list - 路径: /api/user/list
[2024-12-23 14:30:20.055] [DISP] [Thread-5678] [INFO] ⚡ AP调用 耗时 53ms - user.list -> 127.0.0.1:8081
[2024-12-23 14:30:20.056] [DISP] [Thread-5678] [INFO] 📤 [REQ1703311820001234] 200 耗时 55ms

[2024-12-23 14:30:20.003] [AP] [Thread-9012] [INFO] 📥 [REQ1703311820001234] JSON user.list from 192.168.1.100
[2024-12-23 14:30:20.004] [AP] [Thread-9012] [INFO] 🔧 [REQ1703311820001234] AP调用: AP.user.list - 处理请求
[2024-12-23 14:30:20.054] [AP] [Thread-9012] [INFO] ⚡ 请求处理 耗时 50ms - user.list
```

## 配置和使用

### 1. 设置进程名称
```cpp
EnhancedLogger::getInstance().setProcessName("DISP");  // 或 "AP"
```

### 2. 基本日志记录
```cpp
LOG_INFO_CTX("操作成功", LogContext(requestId, clientIp, userId, operation));
LOG_WARNING_CTX("资源不足", LogContext(requestId, clientIp));
LOG_ERROR_DETAIL(requestId, "DatabaseError", "连接失败", "无法连接到MySQL服务器");
```

### 3. 上下文对象
```cpp
LogContext context(requestId, clientIp, userId, operation);
// requestId: 请求唯一标识
// clientIp: 客户端IP地址
// userId: 用户ID（可选）
// operation: 操作类型（可选）
```

## 编译和部署

### 1. 编译增强版服务器
```bash
cd Server/bin
./compile_enhanced.sh
```

### 2. 启动服务
```bash
# 启动AP服务
./build/ap_enhanced

# 启动DISP服务
./build/disp_enhanced
```

## 调试建议

### 1. 请求追踪
- 使用请求ID在日志中搜索完整的请求处理流程
- 关注DISP和AP之间的数据传递

### 2. 性能监控
- 关注⚡符号的性能日志
- 识别处理时间较长的操作

### 3. 错误排查
- 查看🚨符号的错误日志
- 结合错误类型和详细信息定位问题

### 4. 日志过滤
```bash
# 只查看错误日志
grep "ERROR" server.log

# 查看特定请求的所有日志
grep "REQ1703311820001234" server.log

# 查看性能相关日志
grep "⚡" server.log

# 查看特定客户端的日志
grep "192.168.1.100" server.log
```

## 日志符号说明

| 符号 | 含义 | 用途 |
|------|------|------|
| 🚀 | 系统启动/初始化 | 服务启动、组件初始化 |
| ⚙️ | 配置/设置 | 配置加载、参数设置 |
| 📥 | 请求接收 | HTTP请求、API调用接收 |
| 📤 | 响应发送 | HTTP响应、API调用响应 |
| 🔧 | API调用 | 内部服务调用 |
| 💾 | 数据库操作 | SQL查询、数据库连接 |
| ⚡ | 性能监控 | 操作耗时、性能指标 |
| 🚨 | 错误事件 | 异常、错误情况 |
| ⚠️ | 警告信息 | 警告、注意事项 |
| 🔍 | 调试信息 | 详细调试数据 |

## 故障排除

### 常见问题

1. **日志无彩色输出**
   - 确保终端支持ANSI颜色码
   - 检查是否在支持颜色的终端中运行

2. **日志级别过多**
   - 调整日志级别配置
   - 使用grep过滤特定级别的日志

3. **性能影响**
   - 日志系统使用了高效的格式化
   - 在生产环境可考虑调整日志级别

### 最佳实践

1. **开发环境**: 使用DEBUG级别查看详细信息
2. **测试环境**: 使用INFO级别监控关键操作
3. **生产环境**: 使用WARNING级别减少日志量
4. **故障排除**: 临时调整为DEBUG级别获取详细信息

## 总结

增强日志系统提供了：
- ✅ 清晰的彩色输出
- ✅ 结构化的日志格式
- ✅ 完整的请求追踪
- ✅ 详细的性能监控
- ✅ 便于调试的上下文信息

通过这个增强的日志系统，您可以更容易地监控和调试DISP和AP进程的运行状态，快速定位和解决问题。