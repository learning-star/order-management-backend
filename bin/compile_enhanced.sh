#!/bin/bash

# 编译增强日志的服务器组件
# 用法: ./compile_enhanced.sh

set -e

echo "🔨 开始编译增强版服务器组件..."

# 设置编译参数
CXX=g++
CXXFLAGS="-std=c++17 -Wall -Wextra -O2 -I../include"
LIBS="-lpthread -lmysqlclient"

# 编译目录
SRC_DIR="../src"
BUILD_DIR="../build"
INCLUDE_DIR="../include"

# 创建构建目录
mkdir -p $BUILD_DIR

echo "📁 创建构建目录: $BUILD_DIR"

# 编译增强日志库
echo "🔧 编译增强日志库..."
$CXX $CXXFLAGS -c $SRC_DIR/common/logger_enhanced.cpp -o $BUILD_DIR/logger_enhanced.o

# 编译数据库管理器
echo "🔧 编译数据库管理器..."
$CXX $CXXFLAGS -c $SRC_DIR/ap/db_manager.cpp -o $BUILD_DIR/db_manager.o

# 编译配置管理器
if [ -f "$SRC_DIR/common/config.cpp" ]; then
    echo "🔧 编译配置管理器..."
    $CXX $CXXFLAGS -c $SRC_DIR/common/config.cpp -o $BUILD_DIR/config.o
    CONFIG_OBJ="$BUILD_DIR/config.o"
else
    echo "⚠️  配置管理器源文件不存在，跳过编译"
    CONFIG_OBJ=""
fi

# 编译工具函数
if [ -f "$SRC_DIR/common/utils.cpp" ]; then
    echo "🔧 编译工具函数..."
    $CXX $CXXFLAGS -c $SRC_DIR/common/utils.cpp -o $BUILD_DIR/utils.o
    UTILS_OBJ="$BUILD_DIR/utils.o"
else
    echo "⚠️  工具函数源文件不存在，跳过编译"
    UTILS_OBJ=""
fi

# 编译增强版AP处理器
echo "🔧 编译增强版AP处理器..."
$CXX $CXXFLAGS -c $SRC_DIR/ap/processor.cpp -o $BUILD_DIR/processor_enhanced.o

# 编译增强版DISP请求处理器
echo "🔧 编译增强版DISP请求处理器..."
$CXX $CXXFLAGS -c $SRC_DIR/disp/request_handler_enhanced.cpp -o $BUILD_DIR/request_handler_enhanced.o

# 编译AP主程序
if [ -f "$SRC_DIR/ap/main.cpp" ]; then
    echo "🔧 编译AP主程序..."
    $CXX $CXXFLAGS -c $SRC_DIR/ap/main.cpp -o $BUILD_DIR/ap_main.o
    
    echo "🔗 链接增强版AP程序..."
    $CXX $CXXFLAGS $BUILD_DIR/ap_main.o $BUILD_DIR/processor_enhanced.o $BUILD_DIR/db_manager.o $BUILD_DIR/logger_enhanced.o $CONFIG_OBJ $UTILS_OBJ $LIBS -o $BUILD_DIR/ap_enhanced
    
    if [ $? -eq 0 ]; then
        echo "✅ 增强版AP程序编译成功: $BUILD_DIR/ap_enhanced"
    else
        echo "❌ 增强版AP程序编译失败"
    fi
else
    echo "⚠️  AP主程序源文件不存在，跳过AP程序编译"
fi

# 编译DISP主程序
if [ -f "$SRC_DIR/disp/main.cpp" ]; then
    echo "🔧 编译DISP主程序..."
    $CXX $CXXFLAGS -c $SRC_DIR/disp/main.cpp -o $BUILD_DIR/disp_main.o
    
    echo "🔗 链接增强版DISP程序..."
    $CXX $CXXFLAGS $BUILD_DIR/disp_main.o $BUILD_DIR/request_handler_enhanced.o $BUILD_DIR/logger_enhanced.o $CONFIG_OBJ $UTILS_OBJ $LIBS -o $BUILD_DIR/disp_enhanced
    
    if [ $? -eq 0 ]; then
        echo "✅ 增强版DISP程序编译成功: $BUILD_DIR/disp_enhanced"
    else
        echo "❌ 增强版DISP程序编译失败"
    fi
else
    echo "⚠️  DISP主程序源文件不存在，跳过DISP程序编译"
fi

echo ""
echo "🎉 编译完成!"
echo ""
echo "📋 编译摘要:"
echo "   - 增强日志库: ✅"
echo "   - 数据库管理器: ✅"
if [ -n "$CONFIG_OBJ" ]; then
    echo "   - 配置管理器: ✅"
else
    echo "   - 配置管理器: ⚠️  (源文件不存在)"
fi
if [ -n "$UTILS_OBJ" ]; then
    echo "   - 工具函数: ✅"
else
    echo "   - 工具函数: ⚠️  (源文件不存在)"
fi
echo "   - 增强版AP处理器: ✅"
echo "   - 增强版DISP处理器: ✅"

if [ -f "$BUILD_DIR/ap_enhanced" ]; then
    echo "   - 增强版AP程序: ✅"
else
    echo "   - 增强版AP程序: ⚠️  (主程序源文件不存在)"
fi

if [ -f "$BUILD_DIR/disp_enhanced" ]; then
    echo "   - 增强版DISP程序: ✅"
else
    echo "   - 增强版DISP程序: ⚠️  (主程序源文件不存在)"
fi

echo ""
echo "📖 使用说明:"
echo "   1. 确保MySQL服务正在运行"
echo "   2. 运行数据库迁移: ./migrate_database.sh"
echo "   3. 启动AP服务: $BUILD_DIR/ap_enhanced"
echo "   4. 启动DISP服务: $BUILD_DIR/disp_enhanced"
echo ""
echo "🔍 日志输出:"
echo "   - 增强版日志提供彩色输出和结构化信息"
echo "   - 包含请求ID追踪和性能监控"
echo "   - 支持不同日志级别的过滤"
echo ""