//
// Created by z00813738 on 25-9-11.
//

#include "solution.h"
#include <vector>
#include <cstdint>

using namespace std;

namespace SolutionUtils {

constexpr const uint32_t META_PLANE_NUM = 2;

enum class RingOrder {
    CLOCKWISE,
    COUNTER_CLOCKWISE
};

RingOrder CalcRingOrder(uint32_t planeId)
{
    return (planeId % 2 == 0) ? RingOrder::CLOCKWISE : RingOrder::COUNTER_CLOCKWISE;
}

Action ConstructAction(uint32_t rankSize, uint32_t rankId, uint32_t phaseId, RingOrder ringOrder)
{
    Action action;
    action.planeId = DEFAULT_PLANE_ID;  // 会被外层覆盖
    action.sliceId = INVALID_SLICE_ID;  // 会被外层覆盖
    
    if (ringOrder == RingOrder::CLOCKWISE) {
        action.srcRank = rankId;
        action.dstRank = (rankId + 1) % rankSize;
    } else {
        action.srcRank = rankId;
        action.dstRank = (rankId - 1 + rankSize) % rankSize;
    }
    
    return action;
}

Phase ConstructPhase(uint32_t rankSize, uint32_t phaseId, uint32_t planeId)
{
    Phase phase;
    RingOrder order = CalcRingOrder(planeId);
    
    for (uint32_t rankId = 0; rankId < rankSize; ++rankId) {
        Action action = ConstructAction(rankSize, rankId, phaseId, order);
        action.planeId = planeId;
        
        // 修正sliceId的计算逻辑
        // 在ring算法中，每个rank在phaseId阶段发送特定的slice
        if (order == RingOrder::CLOCKWISE) {
            // 顺时针：rank i 在phase p发送 slice (i - p - 1 + N) % N
            action.sliceId = (rankId - phaseId - 1 + rankSize) % rankSize;
        } else {
            // 逆时针：rank i 在phase p发送 slice (i + p + 1) % N
            action.sliceId = (rankId + phaseId + 1) % rankSize;
        }
        
        phase.push_back(action);
    }
    
    return phase;
}

Schedule ConstructSchedule(uint32_t rankSize, uint32_t planeId)
{
    Schedule schedule;
    
    // ring算法需要rankSize-1个phase（当rankSize>1时）
    if (rankSize <= 1) {
        return schedule;  // 不需要通信
    }
    
    for (uint32_t phaseId = 0; phaseId < rankSize - 1; ++phaseId) {
        Phase phase = ConstructPhase(rankSize, phaseId, planeId);
        schedule.push_back(phase);
    }
    
    return schedule;
}

} // namespace SolutionUtils

Blueprint Solution::ConstructBluePrint(uint32_t rankSize, uint32_t planeNum)
{
    Blueprint blueprint;
    
    // 对于每个plane（通信层），构造一个schedule
    for (uint32_t planeId = 0; planeId < planeNum; ++planeId) {
        Schedule schedule = SolutionUtils::ConstructSchedule(rankSize, planeId);
        blueprint.push_back(schedule);
    }
    
    return blueprint;
}