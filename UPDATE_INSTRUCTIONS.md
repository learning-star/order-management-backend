# 数据库和服务端更新说明

本文档指导您如何更新数据库结构和服务端代码，以支持前端页面显示所需的数据项。

## 🗄️ 数据库更新

### 1. 运行数据库迁移脚本

```bash
# 进入服务器目录
cd Server/bin

# 给迁移脚本执行权限
chmod +x migrate_database.sh

# 运行数据库迁移
./migrate_database.sh
```

### 2. 迁移内容说明

迁移脚本将为现有数据库添加以下字段：

**用户表 (users)**
- `phone` - 电话号码字段
- `role` - 用户角色 (admin, manager, employee)
- `status` - 用户状态 (active, inactive)

**产品表 (products)**
- `category` - 产品分类 (electronics, clothing, books, home)
- `status` - 产品状态 (active, inactive)

**订单表 (orders)**
- `customer_name` - 客户名称
- `product_name` - 产品名称
- `quantity` - 购买数量
- 状态枚举值更新为与前端一致

## 🔧 服务端代码更新

### 1. 备份原始文件

```bash
# 备份原始processor.cpp
cp src/ap/processor.cpp src/ap/processor_backup.cpp
```

### 2. 应用新的处理器代码

```bash
# 使用更新的处理器代码
cp src/ap/processor_updated.cpp src/ap/processor.cpp
```

### 3. 重新编译项目

```bash
# 进入构建目录
cd build

# 重新编译
make clean
make

# 或者完整重新构建
cd ..
rm -rf build
mkdir build
cd build
cmake ..
make
```

## 🚀 启动服务

### 1. 启动AP服务

```bash
cd Server/bin
./ap
```

### 2. 启动DISP服务

```bash
cd Server/bin
./disp
```

### 3. 启动前端服务

```bash
cd Web
python3 -m http.server 3000
```

## 📊 数据验证

运行以下SQL查询验证数据库更新是否成功：

```sql
-- 检查用户表结构
DESCRIBE users;

-- 检查产品表结构
DESCRIBE products;

-- 检查订单表结构
DESCRIBE orders;

-- 查看示例数据
SELECT * FROM users LIMIT 5;
SELECT * FROM products LIMIT 5;
SELECT * FROM orders LIMIT 5;
```

## 🔄 API接口更新

更新后的API将返回以下格式的数据：

### 用户API响应示例
```json
{
  "id": 1,
  "name": "张三",
  "email": "zhang@example.com",
  "phone": "13800138001",
  "role": "admin",
  "status": "active",
  "created_at": "2024-01-01 00:00:00"
}
```

### 产品API响应示例
```json
{
  "id": 1,
  "name": "苹果手机",
  "category": "electronics",
  "description": "最新款苹果手机",
  "price": 5999.00,
  "stock": 50,
  "status": "active",
  "created_at": "2024-01-01 00:00:00"
}
```

### 订单API响应示例
```json
{
  "id": 1,
  "user_id": 1,
  "customer_name": "张三",
  "product_name": "苹果手机",
  "quantity": 1,
  "total_amount": 5999.00,
  "status": "completed",
  "created_at": "2024-01-01 00:00:00"
}
```

## ⚠️ 注意事项

1. **备份数据**：在运行迁移脚本前，建议备份现有数据库
2. **服务重启**：更新代码后需要重启AP和DISP服务
3. **权限检查**：确保数据库用户有足够权限执行ALTER TABLE操作
4. **依赖检查**：确保nlohmann/json库已正确安装

## 🐛 故障排除

### 数据库迁移失败
- 检查数据库连接配置
- 确认用户权限
- 查看错误日志

### 编译失败
- 检查依赖库是否安装
- 确认CMake版本兼容性
- 查看编译错误信息

### 服务启动失败
- 检查端口是否被占用
- 查看服务日志文件
- 确认数据库连接正常

## 📝 更新日志

- 添加用户角色和状态管理
- 增加产品分类和状态
- 简化订单数据结构
- 优化前端数据显示
- 完善API响应格式