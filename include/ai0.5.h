#ifndef AI05_H
#define AI05_H

#include "common.h"
#include <vector>

// AI 0.5 Decision Function
// Strategy: Merge internal armies into deathballs, then march on the King.
oprate getAIDecision05(int height, int width, const std::vector<std::vector<plot>>& mapData, int myState);

#endif
