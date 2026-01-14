// evaluator.cpp - 完整评测程序
#include "solution.h"
#include <iostream>
#include <vector>
#include <map>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace std;

// 评测常量
constexpr double L = 0.002;  // 阶段启动时延 ms
constexpr double S = 40.0 * 1024.0 * 1024.0;  // 40 MB
constexpr double B = 400.0 * 1024.0 * 1024.0 * 1024.0;  // 400 GB/s
constexpr double BETA = 1.5;
constexpr double SCORE_MAX = 100.0;

// 手动解析sample.json（避免依赖外部库）
vector<pair<uint32_t, uint32_t>> LoadTestCases(const string& filename) {
    vector<pair<uint32_t, uint32_t>> test_cases;
    ifstream file(filename);
    
    if (!file.is_open()) {
        cerr << "无法打开文件: " << filename << endl;
        // 使用硬编码的测试用例
        test_cases = {
            {4, 2}, {4, 6}, {5, 2}, {5, 4}, {10, 4},
            {10, 10}, {32, 6}, {33, 4}, {64, 8}, {128, 18}
        };
        cout << "使用内置测试用例" << endl;
        return test_cases;
    }
    
    string line, content;
    while (getline(file, line)) {
        content += line;
    }
    file.close();
    
    // 简单解析JSON
    size_t pos = content.find("\"test_case_list\"");
    if (pos == string::npos) {
        cerr << "JSON格式错误" << endl;
        return test_cases;
    }
    
    pos = content.find('[', pos);
    size_t end_pos = content.find(']', pos);
    
    if (pos == string::npos || end_pos == string::npos) {
        cerr << "JSON格式错误" << endl;
        return test_cases;
    }
    
    string array_content = content.substr(pos, end_pos - pos + 1);
    
    // 解析每个测试用例
    pos = 0;
    while ((pos = array_content.find("\"rank_size\"", pos)) != string::npos) {
        // rank_size
        size_t colon_pos = array_content.find(':', pos);
        size_t comma_pos = array_content.find(',', colon_pos);
        string rank_str = array_content.substr(colon_pos + 1, comma_pos - colon_pos - 1);
        
        // plane_num
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
            cerr << "解析数字失败" << endl;
        }
    }
    
    return test_cases;
}

// 验证Blueprint基本正确性
bool ValidateBlueprint(const Blueprint& bp, uint32_t N, uint32_t P, bool verbose = false) {
    if (bp.size() != P) {
        if (verbose) cerr << "错误: plane数量不正确 (" << bp.size() << " != " << P << ")" << endl;
        return false;
    }
    
    if (bp.empty()) {
        if (verbose) cerr << "错误: Blueprint为空" << endl;
        return false;
    }
    
    // 检查所有plane的phase数是否一致
    size_t expected_phases = bp[0].size();
    for (size_t p = 1; p < bp.size(); ++p) {
        if (bp[p].size() != expected_phases) {
            if (verbose) cerr << "错误: plane " << p << "的phase数不一致" << endl;
            return false;
        }
    }
    
    // 检查每个phase的action数
    for (size_t p = 0; p < bp.size(); ++p) {
        for (size_t ph = 0; ph < bp[p].size(); ++ph) {
            // 每个phase应该有N个action
            if (bp[p][ph].size() != N) {
                if (verbose) cerr << "错误: plane " << p << " phase " << ph 
                                 << "有" << bp[p][ph].size() << "个action，但N=" << N << endl;
                return false;
            }
        }
    }
    
    return true;
}

// 计算理论最小时间（保守估计）
double CalculateTheoreticalMinTime(uint32_t N, uint32_t P) {
    // 最小阶段数：log2(N)向上取整，但不能小于N-1
    double min_phases;
    if ((N & (N - 1)) == 0) { // 2的幂
        min_phases = log2(N);
    } else {
        min_phases = ceil(log2(N));
    }
    
    double T1_min = min_phases * L;
    
    // 数据传输最小时间：总数据量 / (带宽 * 并行度)
    // 每个rank需要发送和接收(N-1)个数据块，每个数据块大小为S/N
    double total_data_per_rank = (N - 1) * S / N;
    
    // 理想情况下，所有P个plane并行，所有链路满载
    double T2_min = total_data_per_rank / (B * P);
    
    return T1_min + T2_min;
}

// 模拟拓扑生成和冲突率计算
double CalculateCommunicationTime(const Blueprint& bp, uint32_t N, uint32_t P) {
    uint32_t K = bp[0].size(); // 阶段数
    
    // 1. 阶段启动时间
    double T1 = K * L;
    
    // 2. 统计总通信量 w(u,v)
    map<pair<uint32_t, uint32_t>, uint32_t> w;
    for (const auto& schedule : bp) {
        for (const auto& phase : schedule) {
            for (const auto& action : phase) {
                if (action.srcRank != action.dstRank) {
                    w[{action.srcRank, action.dstRank}]++;
                }
            }
        }
    }
    
    // 3. 分配边 m(u,v) - 简化算法
    map<pair<uint32_t, uint32_t>, uint32_t> m;
    map<uint32_t, uint32_t> degree; // 每个group的度数
    
    // 先为每个有通信的pair分配1条边
    for (const auto& item : w) {
        uint32_t u = item.first.first;
        uint32_t v = item.first.second;
        
        auto edge_key = make_pair(min(u, v), max(u, v));
        if (m[edge_key] == 0) {
            m[edge_key] = 1;
            degree[u]++;
            degree[v]++;
        }
    }
    
    // 检查度数约束
    for (uint32_t u = 0; u < N; ++u) {
        if (degree[u] > P) {
            // 度数超过P，尝试重新分配
            return 1e100; // 返回一个大数表示无效
        }
    }
    
    // 4. 计算每个阶段的链路冲突率
    double sum_T2_prime = 0.0;
    
    for (uint32_t phase_idx = 0; phase_idx < K; ++phase_idx) {
        map<pair<uint32_t, uint32_t>, uint32_t> cr_phase;
        
        // 统计该阶段的通信量
        for (const auto& schedule : bp) {
            if (phase_idx < schedule.size()) {
                const auto& phase = schedule[phase_idx];
                for (const auto& action : phase) {
                    if (action.srcRank != action.dstRank) {
                        cr_phase[{action.srcRank, action.dstRank}]++;
                    }
                }
            }
        }
        
        // 计算该阶段的最大冲突率
        double max_cr = 0.0;
        for (const auto& item : cr_phase) {
            uint32_t u = item.first.first;
            uint32_t v = item.first.second;
            auto edge_key = make_pair(min(u, v), max(u, v));
            
            if (m.find(edge_key) != m.end() && m[edge_key] > 0) {
                double cr = static_cast<double>(item.second) / m[edge_key];
                if (cr > max_cr) max_cr = cr;
            }
        }
        
        // 如果max_cr为0（可能该阶段没有通信），设置为1
        if (max_cr < 1e-6) max_cr = 1.0;
        
        sum_T2_prime += max_cr;
    }
    
    // 5. 计算总数据传输时间
    double T2 = (S / (N * B)) * sum_T2_prime;
    
    return T1 + T2;
}

// 计算单个测试用例的得分
double EvaluateTestCase(uint32_t N, uint32_t P, Solution& solution, bool verbose = false) {
    if (verbose) {
        cout << "  运行算法..." << endl;
    }
    
    auto start = chrono::high_resolution_clock::now();
    Blueprint bp = solution.ConstructBluePrint(N, P);
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
    
    if (verbose) {
        cout << "  运行时间: " << duration.count() / 1000.0 << " ms" << endl;
    }
    
    // 验证Blueprint
    if (!ValidateBlueprint(bp, N, P, verbose)) {
        if (verbose) {
            cout << "  ❌ 验证失败，得0分" << endl;
        }
        return 0.0;
    }
    
    // 计算理论最小时间
    double T_min = CalculateTheoreticalMinTime(N, P);
    
    // 计算实际通信时间
    double T = CalculateCommunicationTime(bp, N, P);
    
    if (T > 1e50) { // 无效方案
        if (verbose) {
            cout << "  ❌ 无效方案（度数超过P），得0分" << endl;
        }
        return 0.0;
    }
    
    // 计算得分
    double score;
    if (T <= T_min) {
        score = SCORE_MAX;
    } else {
        double ratio = T / T_min;
        score = SCORE_MAX * exp(-BETA * (ratio - 1));
    }
    
    if (verbose) {
        cout << fixed;
        cout.precision(3);
        cout << "  阶段数: " << bp[0].size() << endl;
        cout << "  理论最小时间: " << T_min << " ms" << endl;
        cout << "  实际通信时间: " << T << " ms" << endl;
        cout.precision(2);
        cout << "  得分: " << score << "/100" << endl;
    }
    
    return score;
}

int main() {
    cout << "========================================" << endl;
    cout << "  多平面 reduce_scatter 通信编排评测系统" << endl;
    cout << "========================================" << endl;
    
    // 加载测试用例
    vector<pair<uint32_t, uint32_t>> test_cases = LoadTestCases("sample.json");
    cout << "加载 " << test_cases.size() << " 个测试用例" << endl;
    
    Solution solution;
    double total_score = 0.0;
    
    // 评测每个测试用例
    for (size_t i = 0; i < test_cases.size(); i++) {
        uint32_t N = test_cases[i].first;
        uint32_t P = test_cases[i].second;
        
        cout << "\n[测试用例 " << (i+1) << "/" << test_cases.size() << "]" << endl;
        cout << "  N=" << N << ", P=" << P << endl;
        
        double score = EvaluateTestCase(N, P, solution, true);
        total_score += score;
        
        cout << "  ✅ 本用例得分: " << score << "/100" << endl;
    }
    
    // 输出总结果
    cout << "\n========================================" << endl;
    cout << "             评测结果汇总" << endl;
    cout << "========================================" << endl;
    
    cout << fixed;
    cout.precision(2);
    cout << "总得分: " << total_score << "/" << (test_cases.size() * 100) << endl;
    cout << "平均分: " << total_score / test_cases.size() << endl;
    
    // 根据得分给出评级
    double avg_score = total_score / test_cases.size();
    cout << "\n评级: ";
    if (avg_score >= 90) {
        cout << "★★★★★ 优秀" << endl;
    } else if (avg_score >= 80) {
        cout << "★★★★ 良好" << endl;
    } else if (avg_score >= 70) {
        cout << "★★★ 中等" << endl;
    } else if (avg_score >= 60) {
        cout << "★★ 及格" << endl;
    } else {
        cout << "★ 需要改进" << endl;
    }
    
    cout << "========================================" << endl;
    
    return 0;
}