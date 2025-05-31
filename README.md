# 订单管理系统 - 后端服务

基于C++的分布式订单管理系统后端，采用DISP（分发器）+ AP（应用处理器）架构。

## 🏗️ 系统架构

```
客户端请求 → DISP (8080) → AP (8081) → MySQL数据库
```

### 核心组件

- **DISP (Dispatcher)**: HTTP请求分发器，负责路由和负载均衡
- **AP (Application Processor)**: 业务逻辑处理器，处理具体的CRUD操作
- **数据库**: MySQL存储用户、订单、产品数据

## 📁 项目结构

```
Server
├── CMakeLists.txt
├── README.md
├── UPDATE_INSTRUCTIONS.md
├── bin
│   ├── compile_enhanced.sh
│   └── config
│       └── server.conf
├── docs
│   └── enhanced_logging_guide.md
├── include
│   ├── ap
│   │   ├── db_manager.h
│   │   └── processor.h
│   ├── common
│   │   ├── config.h
│   │   ├── logger_enhanced.h
│   │   └── utils.h
│   └── disp
│       ├── request_handler.h
│       └── server.h
├── install_dependencies.sh
├── src
│   ├── CMakeLists.txt
│   ├── ap
│   │   ├── db_manager.cpp
│   │   ├── main.cpp
│   │   └── processor.cpp
│   ├── common
│   │   ├── config.cpp
│   │   ├── logger_enhanced.cpp
│   │   └── utils.cpp
│   └── disp
│       ├── main.cpp
│       ├── request_handler.cpp
│       └── server.cpp
└── tmp
```

## 🚀 快速开始

### 1. 编译系统

```bash
cd bin
chmod +x compile_enhanced.sh
./compile_enhanced.sh
```


### 2. 启动服务

```bash
# 启动AP服务
./ap

# 启动DISP服务（新终端）
./disp
```

## 🔧 功能特性

### 增强日志系统
- 🎨 彩色控制台输出
- 📝 结构化日志格式
- 🔍 请求追踪和性能监控
- 📊 详细的调试信息

### API接口
- **用户管理**: CRUD操作，支持角色和状态管理
- **订单管理**: 订单创建、更新、状态跟踪
- **产品管理**: 产品信息维护，库存管理

### 数据类型
- 所有数据交互统一使用字符串类型
- 支持价格、数量等数值的字符串处理
- 兼容前端表单验证

## 📋 API端点

### 用户管理
- `GET /api/user` - 获取用户列表
- `GET /api/user/{id}` - 获取单个用户
- `POST /api/user` - 创建用户
- `PUT /api/user` - 更新用户
- `DELETE /api/user` - 删除用户

### 订单管理
- `GET /api/order` - 获取订单列表
- `GET /api/order/{id}` - 获取单个订单
- `POST /api/order` - 创建订单
- `PUT /api/order` - 更新订单
- `PATCH /api/order/{id}/status` - 更新订单状态
- `DELETE /api/order` - 删除订单

### 产品管理
- `GET /api/product` - 获取产品列表
- `GET /api/product/{id}` - 获取单个产品
- `POST /api/product` - 创建产品
- `PUT /api/product` - 更新产品
- `DELETE /api/product` - 删除产品

## 🔍 日志系统

### 日志级别
- **DEBUG**: 详细调试信息
- **INFO**: 一般操作信息
- **WARNING**: 警告信息
- **ERROR**: 错误信息

### 日志类型
- 📥 HTTP请求/响应日志
- 🔧 API调用日志
- 💾 数据库操作日志
- ⚡ 性能监控日志
- 🚀 系统事件日志

## ⚙️ 配置要求

### 系统依赖
- C++17 或更高版本
- MySQL 5.7+
- libmysqlclient-dev
- nlohmann/json

### 编译要求
```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libmysqlclient-dev

# 安装nlohmann/json
sudo apt-get install nlohmann-json3-dev
```

## 🐛 故障排除

### 常见问题
1. **编译错误**: 检查依赖库是否正确安装
2. **数据库连接失败**: 验证MySQL服务状态和连接参数
3. **端口占用**: 确保8080和8081端口可用

### 调试模式
启动时可查看详细的彩色日志输出，包含：
- 请求追踪ID
- 性能计时信息
- 错误详情和堆栈

## 📊 性能特性

- 多线程处理支持
- 连接池管理
- 请求响应时间监控
- 内存使用优化

## 🔐 安全特性

- SQL注入防护
- 输入数据验证和转义
- 错误信息脱敏
- 请求来源IP记录

## 📈 版本历史

- **v1.0.0**: 基础功能实现
- **v1.1.0**: 增强日志系统
- **v1.2.0**: 字符串类型统一处理

## 🤝 贡献指南

1. Fork 项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 📄 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情