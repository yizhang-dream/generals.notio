#include<bits/stdc++.h>
#include "include/common.h"

// 这是一个辅助文件，提供你在编写 AI 时可能需要的代码块。
// 你可以复制这些函数到 ai.cpp 中进行修改和使用。

/******************************************************************************
 * 基础工具：可用的移动方向
 ******************************************************************************/
int dx[] = {0, 0, 1, -1};
int dy[] = {1, -1, 0, 0};

/******************************************************************************
 * 模块1：简单的贪心评价函数 (Greedy Evaluation)
 * 作用：评估某一步移动的价值
 ******************************************************************************/
int evaluateMove(int x, int y, int dir, const std::vector<std::vector<plot>>& mapData, int myState) {
    int tx = x + dx[dir];
    int ty = y + dy[dir];
    int score = 0;

    // 1. 获取目标地块信息 (注意边界检查在调用前做，或者在这里加)
    plot target = mapData[ty][tx];

    // 2. 目标价值判定
    // 发现敌方基地 (最高优先级 - 斩首)
    if (target.type == 3 && target.state != myState && target.state != 0) {
        score += 10000;
    }
    // 发现空的中立基地/敌方基地 (如果state=0的type3存在的话)
    else if (target.type == 3 && target.state == 0) {
        score += 5000;
    }
    // 发现中立/敌方塔 (高优先级 - 占领城市)
    else if (target.type == 2 && target.state != myState) {
        score += 2000;
    }
    // 发现敌方普通地块 (中优先级 - 攻击)
    else if (target.state != myState && target.state != 0) {
        score += 500;
    }
    // 发现空白地块 (低优先级 - 扩张)
    else if (target.state == 0) {
        score += 100;
    }

    // 3. 兵力差值判定 (简单的战斗模拟)
    int myArmy = mapData[y][x].army;
    int targetArmy = target.army;

    if (target.state != myState) {
        if (myArmy > targetArmy + 2) { // 能打赢 (多留一点余量)
            score += 50; 
        } else { // 打不过，去送死
            score -= 9999; 
        }
    } else {
        // 目标是自己人 -> 或者是合并兵力，或者是无效移动
        // 如果前线缺兵，合并是有意义的，这里暂时给低分
        score -= 10; 
    }

    return score;
}

/******************************************************************************
 * 模块2：BFS 寻找最近的高价值目标 (Pathfinding)
 * 作用：寻找最近的皇冠或空城，并返回第一步该往哪走
 * 返回值：0-3 代表方向，-1 代表无目标
 ******************************************************************************/
int findNearestTarget(int startX, int startY, int height, int width, const std::vector<std::vector<plot>>& mapData, int myState) {
    struct Node { int x, y, first_dir; };
    std::queue<Node> q;
    std::vector<std::vector<bool>> visited(height + 2, std::vector<bool>(width + 2, false));

    // 初始化四个方向
    for (int i = 0; i < 4; i++) {
        int nx = startX + dx[i];
        int ny = startY + dy[i];
        // 边界与障碍检查
        if (nx < 1 || nx > width || ny < 1 || ny > height) continue;
        if (mapData[ny][nx].type == 1) continue; // 山脉

        q.push({nx, ny, i});
        visited[ny][nx] = true;
    }

    while (!q.empty()) {
        Node curr = q.front();
        q.pop();

        plot p = mapData[curr.y][curr.x];

        // --- 目标判定条件 ---
        // 1. 发现敌方皇冠
        if (p.type == 3 && p.state != myState && p.state != 0) return curr.first_dir;
        // 2. 发现无所属的塔 (抢城)
        if (p.type == 2 && p.state != myState) return curr.first_dir;

        // 继续搜索
        for (int i = 0; i < 4; i++) {
            int nx = curr.x + dx[i];
            int ny = curr.y + dy[i];

            if (nx < 1 || nx > width || ny < 1 || ny > height) continue;
            if (visited[ny][nx]) continue;
            if (mapData[ny][nx].type == 1) continue;

            visited[ny][nx] = true;
            q.push({nx, ny, curr.first_dir}); // 保持 first_dir 不变，用于回溯第一步
        }
    }

    return -1; // 没找到
}

/******************************************************************************
 * 模块3：基于优先级的决策框架 (Decision Making)
 * 作用：整合上面的逻辑，做出最终决定
 ******************************************************************************/
/*
oprate getAIDecision(...) {
    // 1. 遍历所有属于我的格子，找到兵力最多的一个作为“主力”
    //    (或者遍历所有兵力 > 1 的格子分别做决策)
    
    // ... 寻找主力代码 ...

    // 2. 尝试用 BFS 找高价值目标 (斩首/抢塔)
    int targetDir = findNearestTarget(mainX, mainY, ...);
    if (targetDir != -1) {
        // 检查兵力是否足够去打，如果够就走
        return {mainX, mainY, dx[targetDir], dy[targetDir], myState};
    }

    // 3. 如果没找到大目标，就用贪心算法扩张 (Evaluate 4 directions)
    int bestScore = -99999;
    int bestDir = -1;
    for(int i=0; i<4; i++) {
        // ... call evaluateMove ...
    }
    
    // 4. 返回最佳移动
}
*/
