#include "include/ai0.2.h"
#include <bits/stdc++.h>

using namespace std;

static int dx_02[] = {0, 0, 1, -1};
static int dy_02[] = {1, -1, 0, 0};

struct Point { int x, y; };

// Helper: Calculate distance (Manhattan)
int dis(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// 1. Identify Threats (Enemy armies near my King)
bool isKingThreatened(int height, int width, const vector<vector<plot>>& mapData, int myState, Point& kingPos) {
    for(int y=1; y<=height; y++) {
        for(int x=1; x<=width; x++) {
            if(mapData[y][x].type == 3 && mapData[y][x].state == myState) {
                kingPos = {x, y};
                // Check neighbors for large enemy armies
                for(int i=0; i<4; i++) {
                     int nx = x + dx_02[i];
                     int ny = y + dy_02[i];
                     if(nx>=1 && nx<=width && ny>=1 && ny<=height) {
                         if(mapData[ny][nx].state != myState && mapData[ny][nx].state != 0) {
                             if(mapData[ny][nx].army > mapData[y][x].army) return true;
                         }
                     }
                }
            }
        }
    }
    return false;
}

// 2. BFS for global targets with scoring
struct Target {
    int x, y;
    int type; // 3: King, 2: Tower/City
    int dist;
    int first_dir;
};

// Global Target Search
vector<Target> findTargets(int startX, int startY, int height, int width, const vector<vector<plot>>& mapData, int myState) {
    vector<Target> targets;
    queue<pair<Point, int>> q; // Point, first_dir
    vector<vector<bool>> visited(height + 2, vector<bool>(width + 2, false));
    vector<vector<int>> distance(height + 2, vector<int>(width + 2, 0));

    q.push({{startX, startY}, -1});
    visited[startY][startX] = true;

    while(!q.empty()) {
        auto curr = q.front();
        q.pop();
        int cx = curr.first.x;
        int cy = curr.first.y;
        int fdir = curr.second;
        int d = distance[cy][cx];

        // Analyze current cell
        plot p = mapData[cy][cx];
        bool isTarget = false;
        
        // Enemy King
        if(p.type == 3 && p.state != myState && p.state != 0) {
            targets.push_back({cx, cy, 3, d, fdir});
        }
        // Neutral/Enemy Tower
        else if(p.type == 2 && p.state != myState) {
            targets.push_back({cx, cy, 2, d, fdir});
        }
        
        // Limit search depth to avoid perf hit
        if(d > 25) continue; 

        for(int i=0; i<4; i++) {
            int nx = cx + dx_02[i];
            int ny = cy + dy_02[i];

            if(nx < 1 || nx > width || ny < 1 || ny > height) continue;
            if(visited[ny][nx]) continue;
            if(mapData[ny][nx].type == 1) continue; // Mountain

            visited[ny][nx] = true;
            distance[ny][nx] = d + 1;
            int new_fdir = (fdir == -1) ? i : fdir;
            q.push({{nx, ny}, new_fdir});
        }
    }
    return targets;
}

// 3. Evaluation Function (Enhanced)
double evaluateTarget(const Target& t, int myArmy) {
    double score = 0;
    // Value: King >> Neutral City > Enemy Tower
    if (t.type == 3) score += 1000.0;
    else if (t.type == 2) score += 200.0;

    // Distance penalty
    score -= (t.dist * 10);

    return score;
}

oprate getAIDecision02(int height, int width, const vector<vector<plot>>& mapData, int myState) {
    // 0. Find all my armies
    struct Army { int x, y, army; };
    vector<Army> myArmies;
    Point kingPos = {0,0};
    
    for(int y=1; y<=height; y++) {
        for(int x=1; x<=width; x++) {
            if(mapData[y][x].state == myState) {
                if(mapData[y][x].army > 1) myArmies.push_back({x, y, mapData[y][x].army});
                if(mapData[y][x].type == 3) kingPos = {x, y};
            }
        }
    }

    if(myArmies.empty()) return {0,0,0,0,myState};

    // Sort by army size (desc)
    sort(myArmies.begin(), myArmies.end(), [](const Army& a, const Army& b){ return a.army > b.army; });

    // 1. Emergency Defense
    bool kingDanger = isKingThreatened(height, width, mapData, myState, kingPos);
    // If king is in danger, find biggest army and engage/return (Simplified: just continue normal logic but maybe prioritize king pos)
    // For v0.2, we stick to aggressive expansion, but maybe prioritize attacking nearby enemies.

    // 2. Iterate top armies to find best move
    int checkCount = min((int)myArmies.size(), 8); // Check more armies than v0.1
    
    double globalBestScore = -99999.0;
    oprate bestOp = {0,0,0,0,myState};

    for(int i=0; i<checkCount; i++) {
        Army src = myArmies[i];

        // Search for targets
        vector<Target> targets = findTargets(src.x, src.y, height, width, mapData, myState);
        
        if(!targets.empty()) {
            // Find best target for this army
            for(const auto& t : targets) {
                double score = evaluateTarget(t, src.army);
                
                // Frontline Bonus: If target is very close, boost score
                if(t.dist <= 2) score += 50;

                // Feasibility Check
                // Simple logic: if distance is small, army must be sufficient
                // if t.dist is large, acceptable to just move towards it
                
                if(score > globalBestScore) {
                     // Check if first step is valid
                     int nx = src.x + dx_02[t.first_dir];
                     int ny = src.y + dy_02[t.first_dir];
                     
                     // Avoid hitting mountains (BFS handles this, but mapData check needed for army update)
                     // If targeting enemy, check if we die
                     plot nextCell = mapData[ny][nx];
                     bool suicidal = false;
                     if(nextCell.state != myState && nextCell.state != 0) {
                         // Combat ratio check. Don't attack if we lose significantly.
                         if(src.army < nextCell.army + 2) suicidal = true;
                     }
                     // If neutral tower
                     if(nextCell.type == 2 && nextCell.state == 0) {
                         if(src.army <= 41) suicidal = true; // Need 42 to capture (41 vs 40 = capture)
                     }

                     if(!suicidal) {
                        globalBestScore = score;
                        bestOp = {src.x, src.y, dx_02[t.first_dir], dy_02[t.first_dir], myState};
                     }
                }
            }
        }
        
        // If no strategic target found, use simple greedy expansion
        // ... (Fallthrough or combined logic)
    }

    if(bestOp.sx != 0) return bestOp;

    // Fallback: Random Greedy (similar to 0.1 but simpler)
    // Just find any valid expansion
    for(const auto& src : myArmies) {
        for(int d=0; d<4; d++) {
            int nx = src.x + dx_02[d];
            int ny = src.y + dy_02[d];
            if(nx>=1 && nx<=width && ny>=1 && ny<=height) {
                // [Fix] 允许进入迷雾 (-1)
                int tType = mapData[ny][nx].type;
                if(tType != 1 && mapData[ny][nx].state != myState) {
                     // Try to capture empty land or fog
                     if (tType == -1 || mapData[ny][nx].army < src.army)
                        return {src.x, src.y, dx_02[d], dy_02[d], myState};
                }
            }
        }
    }
    
    // Total fallback: Random move
    if(!myArmies.empty()) {
        int idx = rand() % myArmies.size();
        int d = rand() % 4;
        return {myArmies[idx].x, myArmies[idx].y, dx_02[d], dy_02[d], myState};
    }

    return {0,0,0,0,myState};
}
