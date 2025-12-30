#include "include/ai0.3.h"
#include <bits/stdc++.h>

using namespace std;

static int dx_03[] = {0, 0, 1, -1};
static int dy_03[] = {1, -1, 0, 0};

struct Point { int x, y; };

// AI 0.3: The Expander (Refined)
// Update: Added BFS Distance Field to guide internal armies to frontline.

oprate getAIDecision03(int height, int width, const vector<vector<plot>>& mapData, int myState) {
    // 0. Pre-calculation: Move Map / Distance Field
    // Calculate distance from every cell to "Non-My-State" (Empty or Enemy)
    // This guides internal armies OUTWARD.
    
    vector<vector<int>> distToFront(height + 2, vector<int>(width + 2, 9999));
    queue<Point> q;

    // Init BFS: All non-my tiles are distance 0
    for(int y=1; y<=height; y++) {
        for(int x=1; x<=width; x++) {
            if(mapData[y][x].state != myState) {
                distToFront[y][x] = 0;
                q.push({x, y});
            }
        }
    }

    // Run BFS
    while(!q.empty()) {
        Point curr = q.front(); q.pop();
        int curD = distToFront[curr.y][curr.x];
        
        for(int i=0; i<4; i++) {
            int nx = curr.x + dx_03[i];
            int ny = curr.y + dy_03[i];
            if(nx>=1 && nx<=width && ny>=1 && ny<=height) {
                if(distToFront[ny][nx] > curD + 1) {
                    distToFront[ny][nx] = curD + 1;
                    q.push({nx, ny});
                }
            }
        }
    }

    // 1. Collect all movable armies
    struct ArmySrc { int x, y, army, dist; };
    vector<ArmySrc> myArmies;
    Point enemyKingPos = {-1, -1};
    int enemyState = (myState == 1) ? 2 : 1;

    for(int y=1; y<=height; y++) {
        for(int x=1; x<=width; x++) {
            if(mapData[y][x].state == myState && mapData[y][x].army > 1) {
                myArmies.push_back({x, y, mapData[y][x].army, distToFront[y][x]});
            }
            // Locate Enemy King if visible
            if(mapData[y][x].state == enemyState && mapData[y][x].type == 3) {
                enemyKingPos = {x, y};
            }
        }
    }

    if(myArmies.empty()) return {0,0,0,0,myState};

    // Sort:
    // 1. Prefer armies closer to frontline (dist small, but > 0) ? 
    //    Actually, we want to move ALL armies.
    //    Let's sort by army size primarily.
    sort(myArmies.begin(), myArmies.end(), [](const ArmySrc& a, const ArmySrc& b){
        return a.army > b.army; 
    });

    int checkCount = min((int)myArmies.size(), 20); // Increase check count to allow more flow

    oprate bestOp = {0,0,0,0,myState};
    double maxScore = -999999.0;

    for(int i=0; i<checkCount; i++) {
        ArmySrc src = myArmies[i];

        // Check 4 directions
        for(int d=0; d<4; d++) {
            int nx = src.x + dx_03[d];
            int ny = src.y + dy_03[d];
            
            // Boundary
            if(nx < 1 || nx > width || ny < 1 || ny > height) continue;
            
            plot target = mapData[ny][nx];
            if(target.type == 1) continue; // Mountain
            
            double score = 0;
            bool executable = true;

            // --- Scoring Logic ---

            // Guideline: Move towards front (lower distToFront)
            int currentDist = src.dist;
            int newDist = distToFront[ny][nx];
            
            // Significant bonus for moving closer to front
            if (newDist < currentDist) score += 500; 
            else if (newDist > currentDist) score -= 500; // Don't retreat inside
            
            // 1. Kill King
            if(target.type == 3 && target.state == enemyState) {
                score += 100000;
                if(src.army <= target.army) executable = false; 
            }
            // 2. Attack Enemy Land
            else if(target.state == enemyState) {
                 score += 1000; // Increased Aggression
                 if(src.army > target.army) score += 200; 
                 else executable = false; 
            }
            // 3. Capture Empty Land
            else if(target.state == 0) {
                 if(target.type == 0) { // Plain
                     score += 200;
                 }
                 else if(target.type == 2) { // Tower
                     score += 50; 
                     if(src.army <= 42) executable = false;
                 }
            }
            // 4. Merge (Target is self) - Internal Logistics
            else if(target.state == myState) {
                // If moving towards front, merging is acceptable "flow"
                if(newDist < currentDist) score += 100; 
                else score -= 50;
            }
            // 5. Explore Fog
            else if (target.type == -1) {
                score += 300; // 鼓励探索
            }

            // Random jitter
            score += (rand() % 50);

            if(executable && score > maxScore) {
                maxScore = score;
                bestOp = {src.x, src.y, dx_03[d], dy_03[d], myState};
            }
        }
        
        if(maxScore > 50000) return bestOp;
    }

    return bestOp;
}
