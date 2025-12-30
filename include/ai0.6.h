#ifndef AI0_6_H
#define AI0_6_H

#include "common.h"
#include <vector>

// AI 0.6 Decision Function
// Strategy: Task Priority System (Crown > Edge > Tower > Fog > Land)
// Formula: Score = (Weight * Value) / Cost(Steps)
oprate getAIDecision06(int height, int width, const std::vector<std::vector<plot>>& mapData, int myState);

#endif
