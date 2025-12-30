#ifndef AI01_H
#define AI01_H

#include "common.h"
#include <vector>

// 获取 AI 0.1 版本的决策
// 参数：地图高度、宽度、当前地图数据的快照、AI的阵营ID
oprate getAIDecision01(int height, int width, const std::vector<std::vector<plot>>& mapData, int myState);

#endif
