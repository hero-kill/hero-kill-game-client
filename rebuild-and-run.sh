#!/bin/bash

# 英雄杀客户端重新编译和运行脚本

set -e  # 遇到错误立即退出

echo "=== 开始重新编译客户端 ==="

# 进入 build 目录
cd ./build

# 清理旧的构建文件
echo "清理旧的构建文件..."
rm -rf *

# 运行 CMake 配置
echo "运行 CMake 配置..."
cmake ..

# 编译（使用 8 个并行任务）
echo "开始编译（使用 8 个并行任务）..."
make -j8

# 返回上级目录
cd ../

echo "=== 编译完成，启动客户端 ==="

# 运行客户端
./build/FreeKill
