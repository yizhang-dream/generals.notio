#include "include/ai.h"
#include <cstdlib>
#include <ctime>
#include <vector>

// 获取 AI 决策
oprate getAIDecision(int height, int width, const std::vector<std::vector<plot>>& mapData, int myState) {
    oprate moveOp;
    moveOp.sx = 0; // 默认无操作
    moveOp.sy = 0;
    moveOp.dx = 0;
    moveOp.dy = 0;

    // 1. 找到所有属于 AI 的且兵力大于 1 的格子
    std::vector<oprate> validSources;
    for (int y = 1; y <= height; ++y) {
        for (int x = 1; x <= width; ++x) {
            // 注意：mapData 的索引通过转换访问，因为传入的是 vector
            // vector 下标从 0 开始，而游戏逻辑一般用 1-based 循环
            // 这里我们需要注意 mapData 的构建方式。 
            // 假设外部传入时 mapData[y][x] 对应 land[y][x]
            // 为了安全，我们假设传入的 mapData 已经包含了 padding 或者我们要小心下标
            
            // 更安全的做法是假设 mapData[0] 是无效的或者 mapData[y][x] 直接对应
            if (y >= mapData.size() || x >= mapData[y].size()) continue;

            const plot& p = mapData[y][x];
            if (p.state == myState && p.army > 1) {
                validSources.push_back({x, y, 0, 0});
            }
        }
    }

    if (validSources.empty()) {
        return moveOp;
    }

    // 2. 随机选择一个源格子
    int idx = rand() % validSources.size();
    oprate src = validSources[idx];

    // 3. 随机选择一个方向
    int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}; // 上, 下, 左, 右
    int dirIdx = rand() % 4;
    
    // 简单的合法性检查（不撞墙，不撞山）
    // 尝试最多 4 次找到合法方向
    for(int i=0; i<4; i++) {
        int d = (dirIdx + i) % 4;
        int targetX = src.sx + dirs[d][0];
        int targetY = src.sy + dirs[d][1];

        // 检查边界
        if (targetX < 1 || targetX > width || targetY < 1 || targetY > height) continue;

        // 检查地形 (假设 mapData 足够大)
        if (mapData[targetY][targetX].type == 1) continue; // 是山脉
        
        // 简单 AI 也可以走进迷雾 (-1)
        // if (mapData[targetY][targetX].type == -1) ... 默认是允许的

        // 找到了合法移动
        moveOp.sx = src.sx;
        moveOp.sy = src.sy;
        moveOp.dx = dirs[d][0];
        moveOp.dy = dirs[d][1];
        moveOp.player = myState; // 设置玩家 ID
        return moveOp;
    }

    return moveOp; // 如果所有方向都不可行，返回空操作
}
