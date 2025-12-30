#ifndef COMMON_H
#define COMMON_H

// 游戏常量定义
extern int window_height;  // 窗口高度
extern int window_width;   // 窗口宽度
// const int colwidth = 200;    // [已弃用] 侧栏宽度
const int frame = 8;            // 目标帧率 (每秒帧数)
const int operate_interval = 4; // 操作间隔 (逻辑频率)
extern int matrix_height;       // 地图高度 (格) -> Variable
extern int matrix_width;        // 地图宽度 (格) -> Variable
const int MAX_MATRIX_HEIGHT = 60; // 最大地图高度
const int MAX_MATRIX_WIDTH = 60;  // 最大地图宽度
const int landinc_interval = 25;// 领土增长间隔
const int N = 10000;            // 队列最大长度

// 地块结构体
struct plot {
    int type;  // 地块类型: 0-平原, 1-山脉(障碍), 2-城市/塔(皇冠), 3-将军(基地)
    int state; // 归属状态: 0-中立, 1-玩家1(红), 2-玩家2(蓝/AI)
    int army;  // 当前兵力值
};

// 操作指令结构体
struct oprate {
    int sx, sy; // 源坐标
    int dx, dy; // 移动方向 (如 1,0 表示向右)
    int player; // 发起指令的玩家: 1 或 2
};

#endif
