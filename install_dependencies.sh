#!/bin/bash

# 安装必要的依赖项
echo "正在安装必要的依赖项..."

# 更新包列表
sudo apt-get update

# 安装编译工具
sudo apt-get install -y build-essential cmake

# 安装MySQL客户端库
sudo apt-get install -y libmysqlclient-dev

# 安装CURL库
sudo apt-get install -y libcurl4-openssl-dev

# 安装OpenSSL库
sudo apt-get install -y libssl-dev

# 安装pkg-config
sudo apt-get install -y pkg-config

# 安装nlohmann-json库
sudo apt-get install -y nlohmann-json3-dev

echo "所有依赖项已安装完成！"
echo "现在您可以使用以下命令编译项目："
echo "mkdir -p build && cd build && cmake .. && make"