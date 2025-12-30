#include "include/ai0.5.h"
#include <bits/stdc++.h>

using namespace std;

static int dx_05[] = {0, 0, 1, -1};
static int dy_05[] = {1, -1, 0, 0};

struct Point { int x, y; };

// AI 0.5: 军阀 (合并与突击)
// 1. 聚集策略: 内部的小股部队向大部队合并
// 2. 突击策略: 大部队忽略小利，直接冲向关键目标（前线/敌方将军）

oprate getAIDecision05(int height, int width, const vector<vector<plot>>& mapData, int myState) {
    // --- 阶段 0: 地图分析 ---
    vector<vector<int>> distToFront(height + 2, vector<int>(width + 2, 9999));
    vector<vector<int>> distToKing(height + 2, vector<int>(width + 2, 9999));
    vector<vector<int>> distToDefense(height + 2, vector<int>(width + 2, 9999)); // 到我方关键防御点(塔/皇冠)的距离
    queue<Point> qFront, qKing, qDefense;
    
    int enemyState = (myState == 1) ? 2 : 1;
    Point enemyKingPos = {-1, -1};

    // 初始化 BFS 源点
    for(int y=1; y<=height; y++) {
        for(int x=1; x<=width; x++) {
            // 前线源点
            if(mapData[y][x].state != myState) {
                distToFront[y][x] = 0;
                qFront.push({x, y});
            }
            // 敌方将军源点
            if(mapData[y][x].type == 3 && mapData[y][x].state == enemyState) {
                enemyKingPos = {x, y};
                distToKing[y][x] = 0;
                qKing.push({x, y});
            }
        }
    }

    // 执行 BFS: 计算到前线的距离
    while(!qFront.empty()) {
        Point curr = qFront.front(); qFront.pop();
        int d = distToFront[curr.y][curr.x];
        for(int i=0; i<4; i++) {
            int nx = curr.x + dx_05[i], ny = curr.y + dy_05[i];
            if(nx>=1 && nx<=width && ny>=1 && ny<=height) {
                if(distToFront[ny][nx] > d + 1) {
                    distToFront[ny][nx] = d + 1;
                    qFront.push({nx, ny});
                }
            }
        }
    }

    // 计算防御BFS源点 (依赖 distToFront)
    // 策略: 仅保护 "外部" (靠近前线) 的塔和皇冠
    for(int y=1; y<=height; y++) {
        for(int x=1; x<=width; x++) {
             if(mapData[y][x].state == myState && (mapData[y][x].type == 2 || mapData[y][x].type == 3)) {
                 // 定义 "外部": 距离前线小于一定距离 (例如 8 格), 或者就是皇冠本身(始终保护)
                 if(distToFront[y][x] < 8 || mapData[y][x].type == 3) {
                     distToDefense[y][x] = 0;
                     qDefense.push({x, y});
                 }
             }
        }
    }

    // 执行 BFS: 计算到防御点的距离
    while(!qDefense.empty()) {
        Point curr = qDefense.front(); qDefense.pop();
        int d = distToDefense[curr.y][curr.x];
        for(int i=0; i<4; i++) {
            int nx = curr.x + dx_05[i], ny = curr.y + dy_05[i];
            if(nx>=1 && nx<=width && ny>=1 && ny<=height) {
                if(distToDefense[ny][nx] > d + 1) {
                    distToDefense[ny][nx] = d + 1;
                    qDefense.push({nx, ny});
                }
            }
        }
    }

    // 执行 BFS: 计算到敌方将军的距离 (如果已知)
    if(enemyKingPos.x != -1) {
        while(!qKing.empty()) {
            Point curr = qKing.front(); qKing.pop();
            int d = distToKing[curr.y][curr.x];
            for(int i=0; i<4; i++) {
                int nx = curr.x + dx_05[i], ny = curr.y + dy_05[i];
                if(nx>=1 && nx<=width && ny>=1 && ny<=height) {
                    if(distToKing[ny][nx] > d + 1) {
                        distToKing[ny][nx] = d + 1;
                        qKing.push({nx, ny});
                    }
                }
            }
        }
    }

    // --- 阶段 1: 决策 ---
    struct ArmySrc { int x, y, army, dist; };
    vector<ArmySrc> myArmies;
    for(int y=1; y<=height; y++) {
        for(int x=1; x<=width; x++) {
            if(mapData[y][x].state == myState && mapData[y][x].army > 1) {
                myArmies.push_back({x, y, mapData[y][x].army, distToFront[y][x]});
            }
        }
    }

    if(myArmies.empty()) return {0,0,0,0,myState};
    
    // 按兵力降序排序
    sort(myArmies.begin(), myArmies.end(), [](const ArmySrc& a, const ArmySrc& b){
        return a.army > b.army; 
    });

    int checkCount = min((int)myArmies.size(), 30); 
    oprate bestOp = {0,0,0,0,myState};
    double maxScore = -999999.0;
    
    // 配置参数
    int MERGE_THRESHOLD = 60; // 兵力低于此值尝试合并

    for(int i=0; i<checkCount; i++) {
        ArmySrc src = myArmies[i];

        bool isSafe = (src.dist > 1); // 不直接在前线
        bool isBig = (src.army >= MERGE_THRESHOLD);
        
        for(int d=0; d<4; d++) {
            int nx = src.x + dx_05[d];
            int ny = src.y + dy_05[d];
            
            if(nx < 1 || nx > width || ny < 1 || ny > height) continue;
            plot target = mapData[ny][nx];
            if(target.type == 1) continue; // 山脉障碍
            
            double score = 0;
            bool executable = true;
            
            // --- 评分逻辑 ---
            
            if (isSafe && !isBig) {
                // --- 合并模式 ---
                // 优先: 移动到更大的友军地块
                if (target.state == myState) {
                    if (target.army >= src.army) score += 500; // 向上合并
                    else score += 100; // 向下合并 (依然优于不动)
                    
                    // 奖励: 如果该邻居更靠近前线
                    if (distToFront[ny][nx] < distToFront[src.y][src.x]) score += 50;

                    // 奖励: 保护外部塔/皇冠 ("集结士兵保护")
                    // 1. 如果目标本身就是防御点
                    if (mapData[ny][nx].type == 2 || mapData[ny][nx].type == 3) {
                         score += 300; 
                    }
                    // 2. 移动方向靠近防御点
                    if (distToDefense[ny][nx] < distToDefense[src.y][src.x]) {
                         score += 150;
                    }

                } else {
                    // 理论上安全，但紧挨敌人? 
                    // 如果 dist > 1，意味着至少隔了一格
                    // 如果目标是敌人，说明我们在进攻
                    // 小股部队不要独自进攻，除非必胜
                    if (target.state == enemyState && src.army > target.army + 10) score += 200;
                    else executable = false; // 不要暴露小股部队
                }
            } else {
                // --- 突击模式 ---
                // 优先级 1: 将军路径
                if (enemyKingPos.x != -1) {
                     int curKD = distToKing[src.y][src.x];
                     int newKD = distToKing[ny][nx];
                     if(newKD < curKD) score += 2000; // 冲锋!
                     else if(newKD > curKD) score -= 1000;
                } 
                else {
                    // 优先级 2: 前线路径
                    int curFD = distToFront[src.y][src.x];
                    int newFD = distToFront[ny][nx];
                    if(newFD < curFD) score += 1000;
                }

                // 战术价值
                if(target.type == 3 && target.state == enemyState) score += 100000; // 胜利
                else if(target.state == enemyState) {
                    if(src.army > target.army) score += 300;
                    else executable = false;
                }
                else if(target.type == 2 && target.state == 0) { // 塔 (Tower)
                    score += 150;
                    if(src.army <= 42) executable = false;
                }
                else if(target.state == 0) { // 空地
                    score += 50;
                }
                // [New] Explore Fog
                else if (target.type == -1) {
                    score += 100;
                }
                else if(target.state == myState) {
                     // 大部队合并? 仅当向前线移动时
                     if (distToFront[ny][nx] < distToFront[src.y][src.x]) score += 100;
                     else score -= 100; // 防止拥堵
                }
            }

            score += (rand() % 20); // 随机扰动

            if(executable && score > maxScore) {
                maxScore = score;
                bestOp = {src.x, src.y, dx_05[d], dy_05[d], myState};
            }
        }
        
        if(maxScore > 50000) return bestOp;
    }

    return bestOp;
}
