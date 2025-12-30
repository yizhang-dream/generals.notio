#include "include/ai0.1.h"
#include <bits/stdc++.h>

// 基础方向定义
static int dx_01[] = {0, 0, 1, -1};
static int dy_01[] = {1, -1, 0, 0};

// 贪心评价函数
int evaluateMove01(int x, int y, int dir, const std::vector<std::vector<plot>>& mapData, int myState) {
    int tx = x + dx_01[dir];
    int ty = y + dy_01[dir];
    int score = 0;

    // 1. 获取目标 (调用前需保证 tx, ty 在边界内)
    plot target = mapData[ty][tx];

    // 2. 目标类型价值
    // 敌方基地 (最高 - 斩首)
    if (target.type == 3 && target.state != myState && target.state != 0) {
        score += 10000;
    }
    // 空的中立基地/敌方基地 (即 state=0, type=3)
    else if (target.type == 3 && target.state == 0) {
        score += 5000;
    }
    // 敌方/中立塔 (高 - 抢城)
    else if (target.type == 2 && target.state != myState) {
        score += 2000;
    }
    // 敌方普通地块 (攻地)
    else if (target.state != myState && target.state != 0) {
        score += 500;
    }
    // 空白地块 (扩张)
    else if (target.state == 0) {
        score += 100;
    }

    // 3. 兵力差判定
    int myArmy = mapData[y][x].army;
    int targetArmy = target.army;

    if (target.state != myState) { // 打别人
        if (myArmy > targetArmy + 2) score += 50; // 能打过
        else score -= 9999; // 打不过送死
    } else { // 自己人
        // 合并兵力，暂时给负分避免过度聚集，除非前线需要
        score -= 10; 
    }

    return score;
}

// BFS 寻找最近的高价值目标 (Head/City)
// 返回建议的第一步方向 (0-3)，如果没找到返回 -1
int findNearestTarget01(int startX, int startY, int height, int width, const std::vector<std::vector<plot>>& mapData, int myState) {
    struct Node { int x, y, first_dir; };
    std::queue<Node> q;
    std::vector<std::vector<bool>> visited(height + 2, std::vector<bool>(width + 2, false));

    // 初始四方向入队
    for(int i=0; i<4; i++) {
        int nx = startX + dx_01[i];
        int ny = startY + dy_01[i];
        // 边界与障碍
        if (nx < 1 || nx > width || ny < 1 || ny > height) continue;
        if (mapData[ny][nx].type == 1) continue; // 山脉

        q.push({nx, ny, i});
        visited[ny][nx] = true;
    }

    while(!q.empty()) {
        Node curr = q.front();
        q.pop();

        plot p = mapData[curr.y][curr.x];
        
        // 目标判定
        if (p.type == 3 && p.state != myState && p.state != 0) return curr.first_dir; // 敌方基地
        if (p.type == 2 && p.state != myState) return curr.first_dir; // 塔

        // 扩展
        for(int i=0; i<4; i++) {
            int nx = curr.x + dx_01[i];
            int ny = curr.y + dy_01[i];
            
            if (nx < 1 || nx > width || ny < 1 || ny > height) continue;
            if (visited[ny][nx]) continue;
            if (mapData[ny][nx].type == 1) continue;

            visited[ny][nx] = true;
            q.push({nx, ny, curr.first_dir});
        }
    }
    return -1;
}

// 主决策函数
oprate getAIDecision01(int height, int width, const std::vector<std::vector<plot>>& mapData, int myState) {
    oprate bestMove = {0, 0, 0, 0, myState};
    
    // 1. 寻找所有可移动的己方兵力 (>1)
    struct ArmySrc { int x, y, army; };
    std::vector<ArmySrc> sources;
    
    for(int y=1; y<=height; y++) {
        for(int x=1; x<=width; x++) {
            if(mapData[y][x].state == myState && mapData[y][x].army > 1) {
                sources.push_back({x, y, mapData[y][x].army});
            }
        }
    }

    if(sources.empty()) return bestMove; // 无兵可动

    // 按兵力从大到小排序 (优先让主力干活)
    std::sort(sources.begin(), sources.end(), [](const ArmySrc& a, const ArmySrc& b){
        return a.army > b.army;
    });

    // 2. 遍历主力，尝试做决策
    // 为了性能，只取前几个主力做复杂运算(BFS)，剩下的简单扩张
    int checkCount = std::min((int)sources.size(), 5); // 只看最多5个主力

    for(int i=0; i<checkCount; i++) {
        ArmySrc src = sources[i];

        // 策略A: BFS 找高价值目标
        int bfsDir = findNearestTarget01(src.x, src.y, height, width, mapData, myState);
        if(bfsDir != -1) {
            // 检查兵力是否足够第一步
            int tx = src.x + dx_01[bfsDir];
            int ty = src.y + dy_01[bfsDir];
            plot target = mapData[ty][tx];
            
            bool canWin = true;
            if(target.state != myState && target.state != 0) { // 打敌人
               if(src.army <= target.army + 1) canWin = false;
            }
            // 如果是塔，需要更多兵
            if(target.type == 2 && target.state == 0) {
               if(src.army <= target.army + 1) canWin = false;
            }

            if(canWin) {
                return {src.x, src.y, dx_01[bfsDir], dy_01[bfsDir], myState};
            }
        }

        // 策略B: 贪心打分 (Greedy)
        int bestScore = -999999;
        int bestD = -1;
        for(int d=0; d<4; d++) {
            int tx = src.x + dx_01[d];
            int ty = src.y + dy_01[d];
            // 基础检查
            if(tx < 1 || tx > width || ty < 1 || ty > height) continue;
            if(mapData[ty][tx].type == 1) continue;

            int s = evaluateMove01(src.x, src.y, d, mapData, myState);
            if(s > bestScore) {
                bestScore = s;
                bestD = d;
            }
        }
        
        // 如果找到了不错的移动，有一定概率直接执行 (防止所有兵都只盯着最优解，增加随机性)
        if(bestD != -1 && bestScore > 0) {
             return {src.x, src.y, dx_01[bestD], dy_01[bestD], myState};
        }
    }

    // 3. 如果主力都没找到特别好的，随便找个兵动一下 (保持活跃)
    if(!sources.empty()) {
        int idx = rand() % sources.size();
        ArmySrc s = sources[idx];
        int d = rand() % 4;
        // 简单合法性检查后返回
        int tx = s.x + dx_01[d];
        int ty = s.y + dy_01[d];
        // [Fix] 允许迷雾 (-1)
        if(tx >= 1 && tx <= width && ty >= 1 && ty <= height) {
             if (mapData[ty][tx].type != 1) 
                 return {s.x, s.y, dx_01[d], dy_01[d], myState};
        }
    }

    return bestMove;
}
