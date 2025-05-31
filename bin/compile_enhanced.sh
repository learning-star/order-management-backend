#!/bin/bash

# 编译增强日志的服务器组件
# 用法: ./compile_enhanced.sh

set -e

echo "🔨 开始编译增强版服务器组件..."

mkdir -p ../build
cd ../build
cmake ..
make

echo ""
echo "📖 使用说明:"
echo "   1. 确保MySQL服务正在运行"
echo "   2. 启动AP服务: $BUILD_DIR/ap"
echo "   3. 启动DISP服务: $BUILD_DIR/disp"
echo ""
echo "🔍 日志输出:"
echo "   - 增强版日志提供彩色输出和结构化信息"
echo "   - 包含请求ID追踪和性能监控"
echo "   - 支持不同日志级别的过滤"
echo ""