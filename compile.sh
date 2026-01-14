#!/bin/bash
# compile.sh - 简单编译脚本

echo "开始编译..."

# 编译主解决方案
g++ -O2 -std=c++11 -c solution.cpp -o solution.o

# 编译测试程序（不依赖JSON）
g++ -O2 -std=c++11 test_simple.cpp solution.o -o test_simple

# 如果安装了json库，也可以编译支持JSON的版本
if pkg-config --exists nlohmann_json 2>/dev/null; then
    echo "检测到 nlohmann/json 库，编译完整评测程序..."
    g++ -O2 -std=c++11 evaluator_simple.cpp solution.o -o evaluator_simple $(pkg-config --cflags --libs nlohmann_json)
elif [ -f /usr/include/nlohmann/json.hpp ] || [ -f /usr/local/include/nlohmann/json.hpp ]; then
    echo "检测到 nlohmann/json.hpp，编译完整评测程序..."
    g++ -O2 -std=c++11 evaluator_simple.cpp solution.o -o evaluator_simple
else
    echo "未检测到 nlohmann/json 库，只编译简单测试程序"
fi

echo "编译完成！"
echo "可执行文件："
echo "  - test_simple: 简单测试程序"
if [ -f evaluator_simple ]; then
    echo "  - evaluator_simple: 完整评测程序（需要sample.json）"
fi