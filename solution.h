//
// Created by z00813738 on 25-9-11.
//

#ifndef CPP_SOLUTION_H
#define CPP_SOLUTION_H

#include <cstdint>
#include <vector>

constexpr const uint32_t DEFAULT_PLANE_ID = 65535;
constexpr const uint32_t INVALID_RANK_ID = 65535;
constexpr const uint32_t INVALID_SLICE_ID = 65535;

struct Action {
    uint32_t srcRank{INVALID_RANK_ID};
    uint32_t dstRank{INVALID_RANK_ID};
    uint32_t planeId{DEFAULT_PLANE_ID};
    uint32_t sliceId{INVALID_SLICE_ID};
};

using Phase = std::vector<Action>;
using Schedule = std::vector<Phase>;
using Blueprint = std::vector<Schedule>;

// Solution 类声明
class Solution {
public:
    Blueprint ConstructBluePrint(uint32_t rankSize, uint32_t planeNum);
};


#endif // CPP_SOLUTION_H
