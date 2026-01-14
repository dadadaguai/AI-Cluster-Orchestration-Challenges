// solution.cpp - 改进版本（HD算法 + 优化）
#include "solution.h"
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

using namespace std;

namespace SolutionUtils {

// 判断是否为2的幂
bool IsPowerOfTwo(uint32_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

// 计算以2为底的对数（向上取整）
uint32_t Log2Ceil(uint32_t n) {
    uint32_t result = 0;
    uint32_t temp = n;
    while (temp > 1) {
        temp >>= 1;
        result++;
    }
    if (n > (1u << result)) result++;
    return result;
}

// HD算法：计算每个阶段应该发送哪个slice
uint32_t CalculateSliceForHD(uint32_t rank, uint32_t step, uint32_t total_steps, uint32_t N) {
    if (step == 0) {
        // 第一阶段：发送自己的slice给partner
        return rank;
    }
    
    uint32_t mask = 1 << (step - 1);
    uint32_t base = (rank & ~(mask - 1)) << 1;
    uint32_t offset = rank & (mask - 1);
    return (base + offset) % N;
}

// 构建HD算法的Phase
Phase ConstructHDPhase(uint32_t N, uint32_t step, uint32_t planeId, uint32_t total_steps) {
    Phase phase;
    uint32_t mask = 1 << step;
    
    for (uint32_t rank = 0; rank < N; ++rank) {
        uint32_t partner = rank ^ mask;
        
        if (partner >= N) {
            // 如果partner不存在，创建一个虚拟的发送动作（实际不通信）
            // 但为了保持每个phase都有N个action，我们还是创建一个
            Action action;
            action.srcRank = rank;
            action.dstRank = rank; // 发送给自己（实际无通信）
            action.planeId = planeId;
            action.sliceId = CalculateSliceForHD(rank, step, total_steps, N);
            phase.push_back(action);
            continue;
        }
        
        // 确定通信方向：让编号较小的rank发送
        Action action;
        action.planeId = planeId;
        
        if (rank < partner) {
            action.srcRank = rank;
            action.dstRank = partner;
        } else {
            action.srcRank = partner;
            action.dstRank = rank;
        }
        
        // 计算发送的slice
        uint32_t sender = action.srcRank;
        action.sliceId = CalculateSliceForHD(sender, step, total_steps, N);
        
        phase.push_back(action);
    }
    
    return phase;
}

// 构建通用算法（处理任意N）
Phase ConstructGeneralPhase(uint32_t N, uint32_t step, uint32_t planeId, uint32_t total_steps) {
    Phase phase;
    
    if (IsPowerOfTwo(N)) {
        // 2的幂：使用标准HD算法
        return ConstructHDPhase(N, step, planeId, total_steps);
    } else {
        // 非2的幂：使用改进的recursive halving
        uint32_t group_size = 1 << (step + 1);
        
        for (uint32_t rank = 0; rank < N; ++rank) {
            uint32_t group_start = (rank / group_size) * group_size;
            uint32_t offset = rank % (1 << step);
            
            // 计算partner：组内对称位置
            uint32_t partner = group_start + (1 << step) + offset;
            
            Action action;
            action.planeId = planeId;
            
            if (partner < N) {
                // 正常通信
                if (rank < partner) {
                    action.srcRank = rank;
                    action.dstRank = partner;
                } else {
                    action.srcRank = partner;
                    action.dstRank = rank;
                }
                
                // 计算发送的slice
                // 对于recursive halving，每个rank发送自己拥有的部分slice
                uint32_t slice = group_start + offset;
                action.sliceId = slice % N;
            } else {
                // partner不存在，发送给自己（实际无通信）
                action.srcRank = rank;
                action.dstRank = rank;
                action.sliceId = rank; // 发送自己的slice
            }
            
            phase.push_back(action);
        }
    }
    
    return phase;
}

// 构建完整的Schedule
Schedule ConstructSchedule(uint32_t N, uint32_t planeId) {
    Schedule schedule;
    
    if (N <= 1) return schedule;
    
    // 计算需要的阶段数
    uint32_t total_steps = Log2Ceil(N);
    
    for (uint32_t step = 0; step < total_steps; ++step) {
        Phase phase = ConstructGeneralPhase(N, step, planeId, total_steps);
        schedule.push_back(phase);
    }
    
    return schedule;
}

} // namespace SolutionUtils

Blueprint Solution::ConstructBluePrint(uint32_t rankSize, uint32_t planeNum) {
    Blueprint blueprint;
    
    // 为每个plane（通信层）构建schedule
    for (uint32_t planeId = 0; planeId < planeNum; ++planeId) {
        Schedule schedule = SolutionUtils::ConstructSchedule(rankSize, planeId);
        blueprint.push_back(schedule);
    }
    
    return blueprint;
}