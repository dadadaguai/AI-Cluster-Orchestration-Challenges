// evaluator_simple.cpp - 支持JSON的评测程序
#include "solution.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <cmath>
#include <map>
#include <algorithm>

// 如果没有JSON库，使用手动解析
#ifndef NO_JSON

// 尝试包含JSON库
#ifdef __has_include
#  if __has_include(<nlohmann/json.hpp>)
#    include <nlohmann/json.hpp>
#    define HAS_JSON 1
     using json = nlohmann::json;
#  else
#    define HAS_JSON 0
#  endif
#else
#  define HAS_JSON 0
#endif

#else
#  define HAS_JSON 0
#endif

using namespace std;

// 评测常量
constexpr double L = 0.002;  // 阶段启动时延 ms
constexpr double S = 40.0 * 1024.0 * 1024.0;  // 40 MB
constexpr double B = 400.0 * 1024.0 * 1024.0 * 1024.0;  // 400 GB/s
constexpr double BETA = 1.5;
constexpr double SCORE_MAX = 100.0;

// 手动解析JSON的辅助函数（如果没JSON库）
#if !HAS_JSON
vector<pair<uint32_t, uint32_t>> ParseSampleJson(const string& filename) {
    vector<pair<uint32_t, uint32_t>> test_cases;
    ifstream file(filename);
    string line, content;
    
    if (!file.is_open()) {
        cerr << "无法打开文件: " << filename << endl;
        return test_cases;
    }
    
    // 读取整个文件
    while (getline(file, line)) {
        content += line;
    }
    file.close();
    
    // 简单解析（针对sample.json的格式）
    size_t pos = content.find("\"test_case_list\"");
    if (pos == string::npos) {
        cerr << "找不到test_case_list" << endl;
        return test_cases;
    }
    
    // 提取数组内容
    pos = content.find('[', pos);
    size_t end_pos = content.find(']', pos);
    
    if (pos == string::npos || end_pos == string::npos) {
        cerr << "解析JSON数组失败" << endl;
        return test_cases;
    }
    
    string array_content = content.substr(pos, end_pos - pos + 1);
    
    // 解析每个测试用例
    pos = 0;
    while ((pos = array_content.find("\"rank_size\"", pos)) != string::npos) {
        // 找到rank_size的值
        size_t colon_pos = array_content.find(':', pos);
        size_t comma_pos = array_content.find(',', colon_pos);
        string rank_str = array_content.substr(colon_pos + 1, comma_pos - colon_pos - 1);
        
        // 找到plane_num的值
        pos = array_content.find("\"plane_num\"", comma_pos);
        colon_pos = array_content.find(':', pos);
        comma_pos = array_content.find(',', colon_pos);
        if (comma_pos == string::npos) {
            comma_pos = array_content.find('}', colon_pos);
        }
        string plane_str = array_content.substr(colon_pos + 1, comma_pos - colon_pos - 1);
        
        // 转换为数字
        try {
            uint32_t rank_size = stoi(rank_str);
            uint32_t plane_num = stoi(plane_str);
            test_cases.push_back({rank_size, plane_num});
        } catch (...) {
            cerr << "解析数字失败: " << rank_str << ", " << plane_str << endl;
        }
    }
    
    return test_cases;
}
#endif

// 计算理论最小时间
double CalculateTheoreticalMinTime(uint32_t N, uint32_t P) {
    double min_phases = N - 1;
    double T1_min = min_phases * L;
    double total_data = (N - 1) * S / N;
    double T2_min = total_data / (B * P);
    return T1_min + T2_min;
}

// 简单验证Blueprint
bool ValidateBlueprint(const Blueprint& bp, uint32_t N, uint32_t P) {
    if (bp.size() != P) {
        cerr << "错误: Plane数量不正确 (" << bp.size() << " != " << P << ")" << endl;
        return false;
    }
    
    if (bp.empty()) return true;
    
    size_t expected_phases = bp[0].size();
    for (size_t p = 1; p < bp.size(); ++p) {
        if (bp[p].size() != expected_phases) {
            cerr << "错误: plane " << p << "的phase数不一致" << endl;
            return false;
        }
    }
    
    return true;
}

// 计算得分
double CalculateScore(double T, double T_min) {
    if (T <= T_min) return SCORE_MAX;
    double ratio = T / T_min;
    return SCORE_MAX * exp(-BETA * (ratio - 1));
}

int main() {
    cout << "=== Reduce Scatter 评测系统 ===" << endl;
    
    vector<pair<uint32_t, uint32_t>> test_cases;
    
#if HAS_JSON
    // 使用JSON库解析
    try {
        ifstream file("sample.json");
        if (!file.is_open()) {
            cerr << "无法打开 sample.json" << endl;
            return 1;
        }
        
        json data;
        file >> data;
        file.close();
        
        auto json_cases = data["test_case_list"];
        for (const auto& tc : json_cases) {
            test_cases.push_back({tc["rank_size"], tc["plane_num"]});
        }
        cout << "使用JSON库解析 sample.json" << endl;
        
    } catch (const exception& e) {
        cerr << "JSON解析错误: " << e.what() << endl;
        return 1;
    }
#else
    // 手动解析
    cout << "使用手动解析 sample.json" << endl;
    test_cases = ParseSampleJson("sample.json");
    if (test_cases.empty()) {
        cerr << "解析测试用例失败，使用默认测试用例" << endl;
        test_cases = {
            {4, 2}, {4, 6}, {5, 2}, {5, 4}, {10, 4},
            {10, 10}, {32, 6}, {33, 4}, {64, 8}, {128, 18}
        };
    }
#endif
    
    cout << "找到 " << test_cases.size() << " 个测试用例" << endl;
    
    Solution solution;
    double total_score = 0.0;
    
    for (size_t i = 0; i < test_cases.size(); i++) {
        uint32_t N = test_cases[i].first;
        uint32_t P = test_cases[i].second;
        
        cout << "\n[用例 " << (i+1) << "] N=" << N << ", P=" << P << endl;
        
        // 运行算法
        auto start = chrono::high_resolution_clock::now();
        Blueprint bp = solution.ConstructBluePrint(N, P);
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
        
        // 验证
        if (!ValidateBlueprint(bp, N, P)) {
            cout << "  验证失败，跳过评分" << endl;
            continue;
        }
        
        // 计算阶段数
        uint32_t K = bp.empty() ? 0 : bp[0].size();
        
        // 简单估算通信时间（简化版）
        double T1 = K * L;
        
        // 简化估算T2：假设理想情况
        double total_data = (N - 1) * S / N;
        double T2_simple = total_data / (B * P);
        
        // 实际时间会比理想情况差一些，这里用启发式估算
        double efficiency = 0.7; // 假设70%效率
        double T2 = T2_simple / efficiency;
        
        double T = T1 + T2;
        double T_min = CalculateTheoreticalMinTime(N, P);
        double score = CalculateScore(T, T_min);
        
        cout << "  耗时: " << duration.count() / 1000.0 << " ms" << endl;
        cout << "  阶段数: " << K << endl;
        cout << "  估算通信时间: " << T << " ms" << endl;
        cout << "  理论最小时间: " << T_min << " ms" << endl;
        cout << "  得分: " << score << "/100" << endl;
        
        total_score += score;
    }
    
    cout << "\n=== 评测结果 ===" << endl;
    cout << "总得分: " << total_score << "/" << (test_cases.size() * 100) << endl;
    cout << "平均分: " << (total_score / test_cases.size()) << endl;
    
    return 0;
}