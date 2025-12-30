#include "include/ai0.6.h"
#include <bits/stdc++.h>

using namespace std;

static int dx_06[] = {0, 0, 1, -1};
static int dy_06[] = {1, -1, 0, 0};

struct Point06 { int x, y; };

// 权重配置 
const double W_KILL_KING = 200000.0;     // Doubled for aggression
const double W_DEFEND_KING = 50000.0;    
const double W_ENEMY_TOWER = 50000.0;    // Increased aggression
const double W_DEFEND_TOWER = 25000.0;   
const double W_NEUTRAL_TOWER = 20000.0;  
const double W_FOG = 15000.0;            // Maintained (now much lower relative to attack)
const double W_EDGE_BASE = 100.0;        
const double W_ENEMY_LAND = 40000.0;     // Significantly Increased to ensure attack priority
const double W_HUNT_ENEMY = 45000.0;     // Increased to chase enemies
const double W_EMPTY_LAND = 600.0;       
const double DIST_PENALTY = 50.0;        // Reduced penalty (was 100) to allow long range tracking

// 辅助函数: 计算 5x5 区域内的加权友军防御力
double getWeightedDefense(int cx, int cy, int height, int width, const vector<vector<plot>>& mapData, int myState) {
    double defensePower = 0.0;
    defensePower += mapData[cy][cx].army * 1.0;
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            if (dx == 0 && dy == 0) continue; 
            int nx = cx + dx;
            int ny = cy + dy;
            if (nx >= 1 && nx <= width && ny >= 1 && ny <= height) {
                if (mapData[ny][nx].state == myState) {
                    int dist = max(abs(dx), abs(dy));
                    double weight = (dist == 1) ? 0.8 : 0.6;
                    defensePower += mapData[ny][nx].army * weight;
                }
            }
        }
    }
    return defensePower;
}

// 辅助函数: 梯度追踪计算路径上的总兵力
int getCollectedArmy(int startX, int startY, int maxSteps, int height, int width, const vector<vector<plot>>& mapData, int myState, const vector<vector<int>>& distMap) {
    int totalArmy = mapData[startY][startX].army;
    int currX = startX;
    int currY = startY;
    int steps = 0;
    
    if (distMap[startY][startX] >= 90000) return totalArmy;
    if (maxSteps > 200) maxSteps = 200; 

    while(steps < maxSteps) {
        int bestDist = distMap[currY][currX];
        int nextX = -1, nextY = -1;
        
        for(int i=0; i<4; i++) {
            int nx = currX + dx_06[i];
            int ny = currY + dy_06[i];
            if(nx>=1 && nx<=width && ny>=1 && ny<=height) {
                if(distMap[ny][nx] < bestDist && mapData[ny][nx].type != 1) {
                    bestDist = distMap[ny][nx];
                    nextX = nx;
                    nextY = ny;
                }
            }
        }
        
        if(nextX != -1) {
            currX = nextX;
            currY = nextY;
            if(mapData[currY][currX].state == myState) {
                totalArmy += mapData[currY][currX].army;
            }
            steps++;
            if(bestDist == 0) break; 
        } else {
            break; 
        }
    }
    return totalArmy;
}

// 辅助函数: 检查区域内敌军总兵力
int checkThreat(int cx, int cy, int range, int height, int width, const vector<vector<plot>>& mapData, int myState) {
    int enemyForce = 0;
    for (int dy = -range; dy <= range; dy++) {
        for (int dx = -range; dx <= range; dx++) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (nx >= 1 && nx <= width && ny >= 1 && ny <= height) {
                if (mapData[ny][nx].state != myState && mapData[ny][nx].state != 0 && mapData[ny][nx].type != 1 && mapData[ny][nx].type != 2) { 
                     enemyForce += mapData[ny][nx].army;
                }
            }
        }
    }
    return enemyForce;
}

// 辅助函数: BFS 计算到目标的距离图
void bfsDistance(int height, int width, const vector<vector<plot>>& mapData, 
                 const vector<Point06>& sources, vector<vector<int>>& distMap) {
    queue<Point06> q;
    for(int y=0; y<=height+1; y++) fill(distMap[y].begin(), distMap[y].end(), 99999);
    for (const auto& p : sources) {
        distMap[p.y][p.x] = 0;
        q.push(p);
    }
    while (!q.empty()) {
        Point06 curr = q.front(); q.pop();
        int d = distMap[curr.y][curr.x];
        for (int i = 0; i < 4; i++) {
            int nx = curr.x + dx_06[i];
            int ny = curr.y + dy_06[i];
            if (nx >= 1 && nx <= width && ny >= 1 && ny <= height) {
                if (mapData[ny][nx].type != 1 && distMap[ny][nx] > d + 1) { 
                    distMap[ny][nx] = d + 1;
                    q.push({nx, ny});
                }
            }
        }
    }
}

oprate getAIDecision06(int height, int width, const vector<vector<plot>>& mapData, int myState) {
    // 1. 识别关键点
    vector<Point06> enemyKings;
    vector<Point06> enemyTowers;
    vector<Point06> neutralTowers;
    vector<Point06> fogTiles; 
    Point06 myKing = {-1, -1};
    vector<Point06> myTowers;

    for (int y = 1; y <= height; y++) {
        for (int x = 1; x <= width; x++) {
            plot p = mapData[y][x];
            
            if (p.type == -1) {
                fogTiles.push_back({x, y});
                continue; 
            }

            if (p.type == 3) {
                if (p.state == myState) myKing = {x, y};
                else if (p.state != 0) enemyKings.push_back({x, y});
            } else if (p.type == 2) {
                if (p.state == myState) myTowers.push_back({x, y});
                else if (p.state != 0) enemyTowers.push_back({x, y});
                else neutralTowers.push_back({x, y}); 
            }
        }
    }

    // 2. 威胁评估
    vector<Point06> activeDefenseTargets; 
    if (myKing.x != -1) {
        int threat = checkThreat(myKing.x, myKing.y, 3, height, width, mapData, myState); 
        double myDef = getWeightedDefense(myKing.x, myKing.y, height, width, mapData, myState); 
        if (myDef < threat) activeDefenseTargets.push_back(myKing);
    }
    for (auto t : myTowers) {
        int threat = checkThreat(t.x, t.y, 2, height, width, mapData, myState); 
        double myDef = getWeightedDefense(t.x, t.y, height, width, mapData, myState); 
        if (myDef < threat) activeDefenseTargets.push_back(t);
    }

    // 3. 构建距离场
    vector<vector<int>> distToEnemyKing(height + 2, vector<int>(width + 2));
    bfsDistance(height, width, mapData, enemyKings, distToEnemyKing);

    vector<vector<int>> distToEnemyTower(height + 2, vector<int>(width + 2));
    bfsDistance(height, width, mapData, enemyTowers, distToEnemyTower);

    vector<vector<int>> distToNeutralTower(height + 2, vector<int>(width + 2));
    bfsDistance(height, width, mapData, neutralTowers, distToNeutralTower);

    vector<vector<int>> distToDefense(height + 2, vector<int>(width + 2));
    bfsDistance(height, width, mapData, activeDefenseTargets, distToDefense);

    vector<vector<int>> distToFog(height + 2, vector<int>(width + 2));
    bfsDistance(height, width, mapData, fogTiles, distToFog);

    vector<Point06> allEnemies;
    for(int y=1; y<=height; y++) {
        for(int x=1; x<=width; x++) {
            if(mapData[y][x].state != myState && mapData[y][x].state != 0 && mapData[y][x].type != 1) {
                 allEnemies.push_back({x, y});
            }
        }
    }
    vector<vector<int>> distToEnemyGlobal(height + 2, vector<int>(width + 2));
    bfsDistance(height, width, mapData, allEnemies, distToEnemyGlobal);

    // 4. 决策评分
    double globalMaxScore = -1.0;
    oprate bestOp = {0, 0, 0, 0, myState};
    bool moveFound = false;

    vector<Point06> myArmies;
    for (int y = 1; y <= height; y++) {
        for (int x = 1; x <= width; x++) {
            if (mapData[y][x].state == myState && mapData[y][x].army > 1) {
                myArmies.push_back({x, y});
            }
        }
    }
    
    // Manual shuffle 
    for (int i = myArmies.size() - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        swap(myArmies[i], myArmies[j]);
    }

    if (myArmies.empty()) return {0,0,0,0,myState};

    for (const auto& armyPos : myArmies) {
        int cx = armyPos.x;
        int cy = armyPos.y;
        int armySize = mapData[cy][cx].army;
        bool isLargeArmy = (armySize > 50);

        for (int i = 0; i < 4; i++) {
            int nx = cx + dx_06[i];
            int ny = cy + dy_06[i];

            if (nx < 1 || nx > width || ny < 1 || ny > height) continue;
            int targetType = mapData[ny][nx].type;
            if (targetType == 1) continue; 

            double moveScore = 0;
            
            // Task: Kill Enemy King
            if (!enemyKings.empty()) {
                int nextDist = distToEnemyKing[ny][nx];
                if (nextDist < distToEnemyKing[cy][cx]) {
                    double armyFactor = 0.5 + (armySize / 40.0); 
                    if (nextDist == 0) {
                         int targetArmy = mapData[ny][nx].army;
                         if (armySize > targetArmy + 2) {
                             if (armySize > targetArmy * 2) armyFactor *= 2.0;
                             moveScore += W_KILL_KING * armyFactor;
                         } else {
                             moveScore = -999999.0;
                         }
                    } else if (nextDist < 90000) {
                         // Hunt King with passion
                         double distScore = W_KILL_KING - (nextDist * DIST_PENALTY);
                         if (isLargeArmy) distScore *= 1.5; // Big armies really want the king
                         
                         moveScore += distScore * armyFactor;
                    }
                }
            }

            // Task: Defense
            // Reduced sensitivity: Only highly value defense if close
            if (!activeDefenseTargets.empty()) {
                int nextDist = distToDefense[ny][nx];
                if (nextDist < distToDefense[cy][cx]) {
                    if (armySize < 200) { // Don't pull massive armies back for small defense
                        moveScore += W_DEFEND_KING / (nextDist + 1.0);
                    }
                }
            }

            // Task: Enemy Towers
            if (!enemyTowers.empty()) {
                int nextDist = distToEnemyTower[ny][nx];
                if (nextDist < distToEnemyTower[cy][cx]) {
                     double armyFactor = 0.5 + (armySize / 40.0);
                     if (nextDist == 0) { 
                         // Capture logic
                         int targetArmy = mapData[ny][nx].army;
                         bool canAttack = (armySize > targetArmy + 2) || (isLargeArmy && armySize > targetArmy * 0.8);
                         if (canAttack) {
                             moveScore += W_ENEMY_TOWER * armyFactor * 1.2; // Bonus for taking infrastructure
                             // Attack Bonus includes Fog Clear implicit bonus
                             moveScore += W_FOG * 0.5; 
                         } else {
                             moveScore = -999999.0;
                         }
                     } else if (nextDist < 90000) {
                         double distScore = W_ENEMY_TOWER - (nextDist * DIST_PENALTY);
                         if(isLargeArmy) distScore *= 1.2;
                         if (distScore > 0) {
                                 moveScore += distScore * armyFactor;
                         }
                     }
                }
            }
            
            // Task: Neutral Towers
            if (!neutralTowers.empty()) {
                int nextDist = distToNeutralTower[ny][nx];
                if (nextDist < distToNeutralTower[cy][cx]) {
                    double armyFactor = 0.5 + (armySize / 40.0);
                    if (nextDist == 0) { 
                        int targetArmy = mapData[ny][nx].army;
                         if (armySize > targetArmy + 2) {
                             moveScore += W_NEUTRAL_TOWER * armyFactor;
                             moveScore += W_FOG * 0.5; // Tower gives vision
                         } else {
                             moveScore = -999999.0;
                         }
                    } else {
                        moveScore += (W_NEUTRAL_TOWER * armyFactor) / (nextDist + 1.0);
                    }
                }
            }

            // Task: Explore Fog
            // Reduced priority for large armies if there are enemies
            if (targetType == -1) {
                 double fogScore = W_FOG;
                 if (isLargeArmy && !enemyKings.empty()) fogScore *= 0.2; // Focus on war if big
                 else fogScore *= 2.0;
                 
                 moveScore += fogScore;
            } else {
                 int curDist = distToFog[cy][cx];
                 int nextDist = distToFog[ny][nx];
                 if (nextDist < curDist) {
                     double distScore = W_FOG - (nextDist * 20.0); 
                     if (isLargeArmy && !enemyKings.empty()) distScore *= 0.1;
                     if (distScore > 0) moveScore += distScore;
                 }
            }

            // Task: Empty / Edge
            if (mapData[ny][nx].state == 0 && targetType != -1 && targetType != 2 && targetType != 3) {
                 moveScore += W_EMPTY_LAND;
                 // Expansion bonus reduces as army gets huge (focus on killing)
                 if (!isLargeArmy) moveScore += (armySize * 50.0); 

                 int openNeighbors = 0;
                 for(int k=0; k<4; k++) {
                     int nnx = nx + dx_06[k];
                     int nny = ny + dy_06[k];
                     if (nnx>=1 && nnx<=width && nny>=1 && nny<=height) {
                         if (mapData[nny][nnx].state == 0) {
                             openNeighbors++;
                         }
                     }
                 }
                 moveScore += openNeighbors * 200.0;
                 // Bonus: Taking empty land also explores
                 moveScore += W_FOG * 0.2; 
            }  
            else if (mapData[ny][nx].state == myState) {
                // Transit logic
            }

            // Task: Attack (General)
            if (targetType != -1 && mapData[ny][nx].state != myState && mapData[ny][nx].state != 0 && targetType != 3 && targetType != 2) {
                int targetArmy = mapData[ny][nx].army;
                
                // Aggressive Engagement Rule:
                // If small: need win (+1)
                // If large: can trade (0.8 ratio)
                bool canAttack = false;
                if (isLargeArmy) {
                    if (armySize > targetArmy * 0.8) canAttack = true; 
                } else {
                    if (armySize > targetArmy + 1) canAttack = true;
                }

                if (canAttack) {
                     moveScore += W_ENEMY_LAND;
                     moveScore += targetArmy * 10;
                     // Bonus: Attack reveals map
                     moveScore += W_FOG * 0.5;
                     
                     if (armySize > targetArmy * 2) {
                         moveScore += (armySize - targetArmy) * 10; // Stomp bonus
                     }
                } else {
                    moveScore = -999999.0;
                }
            }

            // Task: Global Enemy Hunt
            {
                 int curDist = distToEnemyGlobal[cy][cx];
                 int nextDist = distToEnemyGlobal[ny][nx];
                 if(nextDist < curDist) {
                      double distScore = W_HUNT_ENEMY - (nextDist * DIST_PENALTY);
                      // Massive armies hunt endlessly
                      if (isLargeArmy) distScore = W_HUNT_ENEMY - (nextDist * 10.0); 

                      if (distScore > 0) {
                          moveScore += distScore;
                      }
                 }
            }

            moveScore += (rand() % 5);

            if (moveScore > globalMaxScore) {
                globalMaxScore = moveScore;
                bestOp = {cx, cy, dx_06[i], dy_06[i], myState};
                moveFound = true;
            }
        }
    }

    // 5. Fallback Mechanism 
    if (!moveFound || globalMaxScore <= 0) {
        vector<oprate> fallbackMoves;
        for (const auto& armyPos : myArmies) {
             int cx = armyPos.x;
             int cy = armyPos.y;
             if(mapData[cy][cx].army < 2) continue;

             for (int i = 0; i < 4; i++) {
                int nx = cx + dx_06[i];
                int ny = cy + dy_06[i];
                if (nx < 1 || nx > width || ny < 1 || ny > height) continue;
                if (mapData[ny][nx].type == 1) continue; 
                
                if (mapData[ny][nx].state == 0 && mapData[ny][nx].type != 2 && mapData[ny][nx].type != 3) {
                     fallbackMoves.push_back({cx, cy, dx_06[i], dy_06[i], myState}); // Weighting
                }
                else if (mapData[ny][nx].state == myState) {
                     fallbackMoves.push_back({cx, cy, dx_06[i], dy_06[i], myState});
                }
             }
        }
        
        if (!fallbackMoves.empty()) {
            return fallbackMoves[rand() % fallbackMoves.size()];
        }
    }

    if (!moveFound) return {0,0,0,0,myState};
    return bestOp;
}
