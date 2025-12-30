#ifndef AI_H
#define AI_H

#include "common.h"
#include <vector>

// AI 决策函数
// 参数:
// height, width: 地图尺寸
// mapData: 当前地图数据的快照 (使用 vector 而不是原生数组以方便传递)
// myState: AI 的玩家编号 (例如 2)
// 返回值: 一个操作指令 (如果 sx=0 则表示无操作)
oprate getAIDecision(int height, int width, const std::vector<std::vector<plot>>& mapData, int myState);

#endif
