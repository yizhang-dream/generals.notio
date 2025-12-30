#include "include/ai0.4.h"
#include <bits/stdc++.h>

using namespace std;

static int dx_04[] = {0, 0, 1, -1};
static int dy_04[] = {1, -1, 0, 0};

struct Point { int x, y; };

// AI 0.4: The General (Balanced)
// Combines Logistics (Distance Field) with Strategic Scoring (Towers/Land)

oprate getAIDecision04(int height, int width, const vector<vector<plot>>& mapData, int myState) {
    // --- Phase 0: Distance Field Calculation (Logistics) ---
    vector<vector<int>> distToFront(height + 2, vector<int>(width + 2, 9999));
    queue<Point> q;

    // Init BFS: Frontline is anywhere NOT my state (Enemy or Empty)
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
            int nx = curr.x + dx_04[i];
            int ny = curr.y + dy_04[i];
            if(nx>=1 && nx<=width && ny>=1 && ny<=height) {
                if(distToFront[ny][nx] > curD + 1) {
                    distToFront[ny][nx] = curD + 1;
                    q.push({nx, ny});
                }
            }
        }
    }

    // --- Phase 1: Army Collection ---
    struct ArmySrc { int x, y, army, dist; };
    vector<ArmySrc> myArmies;
    int enemyState = (myState == 1) ? 2 : 1;

    for(int y=1; y<=height; y++) {
        for(int x=1; x<=width; x++) {
            if(mapData[y][x].state == myState && mapData[y][x].army > 1) {
                myArmies.push_back({x, y, mapData[y][x].army, distToFront[y][x]});
            }
        }
    }

    if(myArmies.empty()) return {0,0,0,0,myState};

    // Sort: Larger armies first
    sort(myArmies.begin(), myArmies.end(), [](const ArmySrc& a, const ArmySrc& b){
        return a.army > b.army; 
    });

    int checkCount = min((int)myArmies.size(), 25); 

    oprate bestOp = {0,0,0,0,myState};
    double maxScore = -999999.0;

    for(int i=0; i<checkCount; i++) {
        ArmySrc src = myArmies[i];

        bool isFrontline = (src.dist <= 2);

        // Check 4 directions
        for(int d=0; d<4; d++) {
            int nx = src.x + dx_04[d];
            int ny = src.y + dy_04[d];
            
            // Boundary
            if(nx < 1 || nx > width || ny < 1 || ny > height) continue;
            
            plot target = mapData[ny][nx];
            if(target.type == 1) continue; // Mountain
            
            double score = 0;
            bool executable = true;

            int currentDist = src.dist;
            int newDist = distToFront[ny][nx];

            if(!isFrontline) {
                // --- Logistics Mode ---
                // Primary Goal: Move to front
                if (newDist < currentDist) score += 1000; // Strong pull
                else if (newDist > currentDist) score -= 1000;
                else score -= 100; // Prefer moving strictly closer
                
                // Keep some basic logic so it doesn't suicide while moving
                 if(target.state == enemyState && src.army <= target.army) executable = false;
                 if(target.type == 2 && target.state == 0 && src.army <= 42) executable = false;
                 
            } else {
                // --- Tactical Mode ---
                // Balanced evaluation of targets
                
                // 1. Kill King (Infinite Priority)
                if(target.type == 3 && target.state == enemyState) {
                    score += 100000;
                    if(src.army <= target.army) executable = false; 
                }
                // 2. Capture Tower (Strategic - v0.2 style)
                // High value, but requires investment
                else if(target.type == 2 && target.state == 0) {
                     score += 300; // Good value
                     if(src.army <= 42) executable = false; // Rigid safety
                }
                else if(target.type == 2 && target.state == enemyState) {
                     score += 400; // Increased value for denying enemy tower
                     if(src.army <= target.army) executable = false;
                }
                // 3. Attack Enemy Land (Aggression - v0.3 style)
                else if(target.state == enemyState) {
                     score += 200; 
                     if(src.army > target.army) score += 50; // Win bonus
                     else executable = false; 
                }
                // 4. Capture Empty Land (Expansion)
                else if(target.state == 0) {
                    score += 100; // Base expansion
                    // Frontline Logic: moving towards enemy is better?
                    // distance field already handles "towards enemy" implicitly if enemy is the nearest boundary
                }
                // [New] Explore Fog
                else if (target.type == -1) {
                    score += 200; 
                }
                
                // Distance Field influence in Tactical Mode
                // Still prefer moving "forward" generally
                if (newDist < currentDist) score += 50;
            }

            // Random jitter
            score += (rand() % 20);

            if(executable && score > maxScore) {
                maxScore = score;
                bestOp = {src.x, src.y, dx_04[d], dy_04[d], myState};
            }
        }
        
        if(maxScore > 50000) return bestOp;
    }

    return bestOp;
}
