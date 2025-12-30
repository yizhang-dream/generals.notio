#include <bits/stdc++.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tchar.h> // 用于 _stprintf
#include <mmsystem.h> //paly music !
#include "include/graphics.h"
#include "include/common.h"
#include "include/ai.h"
#include "include/ai0.1.h"
#include "include/ai0.2.h"
#include "include/ai0.3.h"
#include "include/ai0.4.h"
#include "include/ai0.5.h"
#include "include/ai0.6.h"



// 链接 Winsock 库
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

// Debug Logger
void log_debug(const char* format, ...) {
    FILE* f = fopen("server_log.txt", "a");
    if (f) {
        va_list args;
        va_start(args, format);
        vfprintf(f, format, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

/******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************/
// 窗口与图形变量
HWND window_pointer;             
RECT window_pos;                 
POINT mouse_pos;                 
int window_height = 900;         
int window_width = 900;          
int plot_width, plot_height;     
int offset_x = 0, offset_y = 0;
IMAGE landpicture[4][9];         // [type][state] (Updated to 9 states: 0-Neutral, 1-P1...8-P8)
IMAGE fog_mask;                  
IMAGE background_img;            // 背景图片 (Deprecated)
IMAGE original_bg_img;           // [New] 原始高分辨率背景
// Resize Debounce Variables
long last_resize_tick = 0;
bool resize_pending = false;
bool has_background = false;
plot land[MAX_MATRIX_HEIGHT + 5][MAX_MATRIX_WIDTH + 5];   
bool is_visible[MAX_MATRIX_HEIGHT + 5][MAX_MATRIX_WIDTH + 5]; 
int timm = 0;                    
int start_tick, end_time;        
bool is_game_over = false;       
int winner = 0;                  

// Pause State
bool is_paused = false;
long last_pause_toggle_time = 0;

// 玩家控制设置
int network_map_width = 0;
int network_map_height = 0;
int matrix_width = 20; // Default Medium
int matrix_height = 20;
int map_size_option = 1; // 0:Small, 1:Medium, 2:Large, 3:Huge
const char* getMapSizeName(int opt) {
    switch(opt) {
        case 0: return "Small (15x15)";
        case 1: return "Medium (20x20)";
        case 2: return "Large (30x30)";
        case 3: return "Huge (40x40)";
        default: return "Unknown";
    }
}
void setMapSize(int opt) {
    if (opt == 0) { matrix_width = 15; matrix_height = 15; }
    else if (opt == 1) { matrix_width = 20; matrix_height = 20; }
    else if (opt == 2) { matrix_width = 30; matrix_height = 30; }
    else if (opt == 3) { matrix_width = 40; matrix_height = 40; }
    // Ensure safety
    if (matrix_width > MAX_MATRIX_WIDTH) matrix_width = MAX_MATRIX_WIDTH;
    if (matrix_height > MAX_MATRIX_HEIGHT) matrix_height = MAX_MATRIX_HEIGHT;
}
int player_count = 2;            
int p1_type = 0; // 0:人类
int p2_type = 7; 
int p3_type = 4; 
int p4_type = 7; // Default AI for P4 
int p5_type = 7; 
int p6_type = 7; 
int p7_type = 7; 
int p8_type = 7; 
int local_player_id = 1;         
bool is_lan_mode = false;        

// 输入防抖变量
DWORD last_input_time[9] = {0};  
int last_dir[9] = {0};           
// [优化1] 降低防抖时间，从 100ms 降至 55ms，提升操作流畅度
const int INPUT_DEBOUNCE_MS = 30; 

// 网络变量
SOCKET connSocket = INVALID_SOCKET; 
std::vector<SOCKET> client_sockets; // 服务器专用
bool is_server = false;

// Global Mouse State (updated via PeekMessage)
int mouse_x = 0;
int mouse_y = 0;
bool mouse_lb = false;

void update_input() {
    ExMessage msg;
    while(peekmessage(&msg, EX_MOUSE)) {
        mouse_x = msg.x;
        mouse_y = msg.y;
        if(msg.message == WM_LBUTTONDOWN) mouse_lb = true;
        if(msg.message == WM_LBUTTONUP) mouse_lb = false;
    }
}

void play_sound(const TCHAR* filename) {
    // Alias map for pre-loaded sounds
    if (_tcsstr(filename, _T("click")) != NULL) {
        mciSendString(_T("play sfx_click from 0"), NULL, 0, NULL);
    } 
    else if (_tcsstr(filename, _T("EatEmpty")) != NULL) {
        mciSendString(_T("play sfx_eat_empty from 0"), NULL, 0, NULL);
    }
    else if (_tcsstr(filename, _T("EatNeutral")) != NULL) {
        mciSendString(_T("play sfx_eat_neutral from 0"), NULL, 0, NULL);
    }
    else if (_tcsstr(filename, _T("EatAnemyTower")) != NULL) {
        mciSendString(_T("play sfx_eat_enemy from 0"), NULL, 0, NULL);
    }
    else if (_tcsstr(filename, _T("KillKing")) != NULL) {
        mciSendString(_T("play sfx_kill from 0"), NULL, 0, NULL);
    }
    else {
        // Fallback for non-aliased sounds (if any)
        TCHAR cmd[256];
        _stprintf(cmd, _T("play \"sounds/%s\""), filename);
        mciSendString(cmd, NULL, 0, NULL);
    }
}

/******************************************************************************
 * 图形辅助函数 (GRAPHICS HELPER)
 ******************************************************************************/

// 绘制半透明圆角矩形 (模拟)
void drawRoundBox(int x, int y, int w, int h, COLORREF color, int alpha) {
    // EasyX 本身不支持 alpha 通道混合，这里我们用简单的填充
    // 如果想要半透明，需要读取像素混合，性能较低。
    // 为了流畅，我们使用深色背景 + 边框来模拟现代感
    
    setlinecolor(color);
    setfillcolor(color);
    fillroundrect(x, y, x + w, y + h, 20, 20);
}

// 绘制带阴影的文字
void drawShadowText(int x, int y, const TCHAR* text, int char_height, COLORREF color) {
    setbkmode(TRANSPARENT);
    settextstyle(char_height, 0, _T("Consolas"), 0, 0, FW_BOLD, false, false, false);
    
    settextcolor(BLACK); // 阴影
    outtextxy(x + 2, y + 2, text);
    
    settextcolor(color);
    outtextxy(x, y, text);
}

// 绘制现代风格按钮
// 返回 true 表示鼠标在按钮上 (Hover)
bool drawModernButton(int x, int y, int w, int h, const TCHAR* text, bool isActive = false, bool isFocused = false) {
    bool isHover = (mouse_x >= x && mouse_x <= x + w && mouse_y >= y && mouse_y <= y + h);
    if (isFocused) isHover = true; // Force hover effect if focused
    
    COLORREF baseColor = isActive ? RGB(60, 150, 255) : RGB(50, 50, 60);
    COLORREF hoverColor = isActive ? RGB(80, 180, 255) : RGB(70, 70, 80);
    COLORREF borderColor = isActive ? WHITE : RGB(100, 100, 100);
    if (isFocused) borderColor = RGB(255, 215, 0); // Gold border for focus
    
    if (isHover) baseColor = hoverColor;

    setlinecolor(borderColor);
    setlinestyle(PS_SOLID, 2);
    setfillcolor(baseColor);
    
    fillroundrect(x, y, x + w, y + h, 15, 15);
    
    // 文字居中
    int textLen = _tcslen(text);
    int textHeight = 24;
    
    // 使用 RECT 绘制居中文字
    RECT r = {x, y, x + w, y + h};
    settextcolor(WHITE);
    settextstyle(textHeight, 0, _T("Consolas"), 0, 0, FW_NORMAL, false, false, false);
    drawtext(text, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    return isHover;
}

// [优化3] 初始化背景图片
void init_background() {
    // 1. 优先尝试加载 PNG
    LPCTSTR png_path = _T("images/background.png");
    loadimage(&original_bg_img, png_path, 0, 0, true);

    if (original_bg_img.getwidth() > 0) {
        has_background = true;
        return;
    }

    // 2. 如果 PNG 失败，尝试加载 JPG
    LPCTSTR jpg_path = _T("images/background.jpg"); 
    loadimage(&original_bg_img, jpg_path, 0, 0, true);
    
    // 3. 检查加载是否成功
    if (original_bg_img.getwidth() > 0) {
        has_background = true;
    } else {
        has_background = false;
        std::cout<<"Background Load Failed"<<std::endl;
    }
}

// 绘制背景（替代 cleardevice）
void draw_background() {
    if (has_background) {
        // 使用 StretchBlt 进行拉伸绘制
        HDC hDest = GetImageHDC(NULL); // 获取窗口 HDC
        HDC hSrc = GetImageHDC(&original_bg_img); // 获取图片 HDC
        
        // 设置高质量拉伸模式
        SetStretchBltMode(hDest, HALFTONE);
        SetBrushOrgEx(hDest, 0, 0, NULL);

        StretchBlt(hDest, 0, 0, window_width, window_height, 
                   hSrc, 0, 0, original_bg_img.getwidth(), original_bg_img.getheight(), SRCCOPY);

    } else {
        setbkcolor(BLACK);
        cleardevice();
    }
}



// [New] 绘制战斗区域的纯黑背景 (遮挡主菜单背景，但保留边缘)
void draw_battle_area_bg() {
    // 计算地图区域的矩形范围
    int map_x = offset_x;
    int map_y = offset_y;
    int map_w = plot_width * matrix_width;
    int map_h = plot_height * matrix_height;
    
    // 绘制纯黑矩形
    setfillcolor(BLACK);
    setlinecolor(BLACK);
    fillrectangle(map_x, map_y, map_x + map_w, map_y + map_h);
}

/******************************************************************************
 * 网络辅助函数
 ******************************************************************************/

bool send_vect_to_socket(SOCKET s, std::vector<int> send_info) {
    if (s == INVALID_SOCKET) return false;
    int len = send_info.size();
    if (send(s, (const char *)&len, sizeof(int), 0) == SOCKET_ERROR) return false;
    if (send(s, (const char *)send_info.data(), sizeof(int) * len, 0) == SOCKET_ERROR) return false;
    return true;
}

void broadcast_vect(std::vector<int> send_info, SOCKET exclude_socket = INVALID_SOCKET) {
    if (!is_server) return;
    for (SOCKET s : client_sockets) {
        if (s != exclude_socket && s != INVALID_SOCKET) {
            send_vect_to_socket(s, send_info);
        }
    }
}

void send_vect(std::vector<int> send_info) {
    if (is_lan_mode) {
        if (is_server) {
            broadcast_vect(send_info);
        } else {
            send_vect_to_socket(connSocket, send_info);
        }
    }
}

bool recv_all(SOCKET s, char* buf, int len) {
    int total = 0;
    while(total < len) {
        int r = recv(s, buf + total, len - total, 0);
        if(r <= 0) return false; 
        total += r;
    }
    return true;
}

std::vector<int> recv_vect_from_socket(SOCKET s, bool* disconnected = nullptr) {
    int recv_len = 0;
    // Check if we can read the length
    if(!recv_all(s, (char*)&recv_len, sizeof(int))) {
        if (disconnected) *disconnected = true;
        return {};
    }
    
    if(recv_len < 0 || recv_len > 2500000) {
        // Data corruption, treat as disconnect or ignore?
        // Ideally should assume stream is desync'd -> disconnect
         if (disconnected) *disconnected = true;
        return {};
    }

    std::vector<int> rece(recv_len);
    if(!recv_all(s, (char*)rece.data(), sizeof(int)*recv_len)) {
         if (disconnected) *disconnected = true;
        return {};
    }
    return rece;
}

std::vector<int> recv_vect(bool* disconnected = nullptr) {
    return recv_vect_from_socket(connSocket, disconnected);
}

bool is_socket_ready(SOCKET s) {
    if (s == INVALID_SOCKET) return false;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    int result = select(0, &readfds, NULL, NULL, &timeout);
    return result > 0; 
}

bool is_data_available() {
    return is_socket_ready(connSocket);
}

int oprate_head[9], oprate_tail[9]; 
bool bo_opratequeue[9];             
oprate oprate_queue[9][N + 5];      
int p1_opx, p1_opy;               
int p2_opx, p2_opy;               
int p3_opx, p3_opy;    
int p4_opx, p4_opy;               
int p5_opx, p5_opy;
int p6_opx, p6_opy;
int p7_opx, p7_opy;
int p8_opx, p8_opy;
int posx, posy;                   

int tower_sum = 20;                 
int mount_sum = 60;                 
int initialx, initialy;             
int grid_gap = 0; 
// Update resource sums based on map size
void update_resource_sums() {
    int area = matrix_width * matrix_height;
    // Base: 20x20 = 400. 
    // Ratio: area / 400.
    double ratio = (double)area / 400.0;
    tower_sum = (int)(20 * ratio);
    mount_sum = (int)(60 * ratio);
    if (tower_sum < 5) tower_sum = 5;
    if (mount_sum < 10) mount_sum = 10;
}                   

void reload_assets(int w, int h) {
    grid_gap = std::max(2, w / 15); 
    int img_w = (w > grid_gap) ? w - grid_gap : 1; 
    int img_h = (h > grid_gap) ? h - grid_gap : 1;
    
    TCHAR str1[100];
    for (int a = 0; a <= 3; a++) {
        for (int b = 0; b <= 8; b++) {
            _stprintf(str1, _T("./images/plot.%d.%d.png"), a, b);
            loadimage(&landpicture[a][b], str1, img_w, img_h, true);
        }
    }
    loadimage(&fog_mask, _T("./images/plot.0.x.png"), img_w, img_h, true);
}

std::vector<int> serialize_map() {
    std::vector<int> data;
    data.push_back(matrix_width);
    data.push_back(matrix_height);
    for(int y=1; y<=matrix_height; y++) {
        for(int x=1; x<=matrix_width; x++) {
            data.push_back(land[y][x].type);
            data.push_back(land[y][x].state);
            data.push_back(land[y][x].army);
        }
    }
    return data;
}

void deserialize_map(std::vector<int> data) {
    if(data.size() < 2) return;
    int w = data[0];
    int h = data[1];
    
    // Update Global Dimensions
    matrix_width = w;
    matrix_height = h;
    if (matrix_width > MAX_MATRIX_WIDTH) matrix_width = MAX_MATRIX_WIDTH;
    if (matrix_height > MAX_MATRIX_HEIGHT) matrix_height = MAX_MATRIX_HEIGHT;

    int idx = 2;
    for(int y=1; y<=h; y++) {
        for(int x=1; x<=w; x++) {
            if(idx + 2 >= data.size()) break;
            land[y][x].type = data[idx++];
            land[y][x].state = data[idx++];
            land[y][x].army = data[idx++];
        }
    }
}

// --- 网络初始化 ---

// --- 网络初始化 ---

void check_and_handle_resize(bool force = false); 

bool init_network_server() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    client_sockets.clear(); 
    log_debug("init_network_server start. Needed: %d", 0); // Placeholder 

    std::vector<std::string> ips;
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints = {}, *res;
        hints.ai_family = AF_INET; 
        if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
            struct addrinfo *p = res;
            while(p != NULL) {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &((struct sockaddr_in*)p->ai_addr)->sin_addr, ip, INET_ADDRSTRLEN);
                ips.push_back(std::string(ip));
                p = p->ai_next;
            }
            freeaddrinfo(res);
        }
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    // [Fix] Allow address reuse to prevent bind errors
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8888); 
    
    if (bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        // Show error on screen since we are in graphics mode
        drawRoundBox(window_width/2 - 200, window_height/2 - 50, 400, 100, RGB(50, 0, 0), 255);
        drawShadowText(window_width/2 - 180, window_height/2 - 20, _T("Bind Failed: Port 8888 busy"), 24, RED);
        FlushBatchDraw();
        Sleep(2000);
        
        std::cerr << "Bind Failed: Port 8888 occupied." << std::endl;
        closesocket(serverSocket);
        is_lan_mode = false;
        return false;
    }
    
    if (listen(serverSocket, 5) == SOCKET_ERROR) { 
        std::cerr << "Listen Failed." << std::endl;
        closesocket(serverSocket);
        is_lan_mode = false;
        return false;
    }

    int human_count = 0;
    if (p1_type == 0) human_count++;
    if (p2_type == 0) human_count++;
    if (player_count >= 3 && p3_type == 0) human_count++;
    if (player_count >= 4 && p4_type == 0) human_count++;
    if (player_count >= 5 && p5_type == 0) human_count++;
    if (player_count >= 6 && p6_type == 0) human_count++;
    if (player_count >= 7 && p7_type == 0) human_count++;
    if (player_count >= 8 && p8_type == 0) human_count++;

    int needed_players = human_count - 1; 
    if (needed_players < 0) needed_players = 0; 
    log_debug("Needed players calculated: %d (Human Count: %d)", needed_players, human_count); 

    bool cancelled = false;

    while(client_sockets.size() < needed_players) {
        check_and_handle_resize(); // [Add] Support resize during wait
        update_input();

        draw_background(); 
        
        // 服务器等待面板
        int panel_w = 600;
        int panel_h = 400;
        int panel_x = (window_width - panel_w) / 2;
        int panel_y = (window_height - panel_h) / 2;
        
        drawRoundBox(panel_x, panel_y, panel_w, panel_h, RGB(20, 20, 30), 220);
        drawShadowText(panel_x + 150, panel_y + 30, _T("WAITING FOR PLAYERS"), 30, RGB(255, 200, 100));

        TCHAR msg[100];
        _stprintf(msg, _T("Connected: %d / %d"), (int)client_sockets.size(), needed_players);
        drawShadowText(panel_x + 200, panel_y + 80, msg, 24, WHITE);

        // Debug Info
        TCHAR debug_info[128];
        _stprintf(debug_info, _T("Target: P2=%s P3=%s P4=%s P5=%s P6=%s P7=%s P8=%s"), 
            p2_type==0?"H":"AI", 
            (player_count>=3 && p3_type==0)?"H":"AI",
            (player_count>=4 && p4_type==0)?"H":"AI",
            (player_count>=5 && p5_type==0)?"H":"AI",
            (player_count>=6 && p6_type==0)?"H":"AI",
            (player_count>=7 && p7_type==0)?"H":"AI",
            (player_count>=8 && p8_type==0)?"H":"AI");
        outtextxy(panel_x + 200, panel_y + 110, debug_info);
        
        drawShadowText(panel_x + 50, panel_y + 140, _T("Your IP Addresses:"), 20, RGB(180, 180, 180));
        
        int y_offset = panel_y + 170;
        settextcolor(WHITE);
        settextstyle(20, 0, _T("Consolas"));
        
        for(const auto& ip : ips) {
            TCHAR msg_ip[100];
            _stprintf(msg_ip, _T("- %hs"), ip.c_str());
            outtextxy(panel_x + 70, y_offset, msg_ip);
            y_offset += 25;
        }
        
        TCHAR port_msg[64];
        _stprintf(port_msg, _T("Port: 8888"));
        outtextxy(panel_x + 50, y_offset + 20, port_msg);

        // Cancel Button
        if (drawModernButton(panel_x + 200, panel_y + 320, 200, 50, _T("CANCEL"), false) && (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000))) {
            cancelled = true;
            break;
        }
        
        FlushBatchDraw();

        // Check for cancel key
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            cancelled = true;
            break;
        }

        // 检查是否有连接 (accept 仍然是阻塞的，为了 UI 真正流畅需要改为非阻塞或多线程，但保持逻辑简单不动核心)
        // 使用 select 来避免长时间阻塞 UI
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; // 10ms
        
        int result = select(0, &readfds, NULL, NULL, &timeout);
        
        if (result > 0) {
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);
            SOCKET newSocket = accept(serverSocket, (sockaddr *)&clientAddr, &clientAddrLen);
            
            if (newSocket != INVALID_SOCKET) {
                 int flag = 1;
                 setsockopt(newSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
                 client_sockets.push_back(newSocket);
                 
                 // Determine ID
                 int assigned_id = -1;
                 // Check P2
                 bool p2_is_human = (p2_type == 0);
                 bool p3_is_human = (player_count >= 3 && p3_type == 0);
                 bool p4_is_human = (player_count >= 4 && p4_type == 0);
                 
                 // How many clients connected so far (including this one)?
                 int current_client_index = client_sockets.size(); // 1 for first client
                 
                 // Logic: Fill human slots in order P2 -> P3 -> P4
                 // This assumes clients connect in the order they are "expected" which is lazy but works for simple LAN
                 // Better: Assign based on what is available.
                 
                 std::vector<int> human_slots;
                 if (p2_is_human) human_slots.push_back(2);
                 if (p3_is_human) human_slots.push_back(3);
                 if (p4_is_human) human_slots.push_back(4);
                 if (player_count >= 5 && p5_type == 0) human_slots.push_back(5);
                 if (player_count >= 6 && p6_type == 0) human_slots.push_back(6);
                 if (player_count >= 7 && p7_type == 0) human_slots.push_back(7);
                 if (player_count >= 8 && p8_type == 0) human_slots.push_back(8);
                 
                 // current_client_index is 1-based (size). 1st client gets 1st slot.
                 if (current_client_index > 0 && current_client_index <= human_slots.size()) {
                     assigned_id = human_slots[current_client_index - 1];
                 }
                 
                 // Fallback (should not happen if logic is sound)
                 if (assigned_id == -1) assigned_id = 1 + current_client_index;

                 std::vector<int> id_packet;
                 id_packet.push_back(-1); 
                 id_packet.push_back(assigned_id);
                 id_packet.push_back(assigned_id);
                 send_vect_to_socket(newSocket, id_packet);
                 
                 log_debug("Client connected! Socket: %d, Assigned ID: %d", (int)newSocket, assigned_id);
            }
        }
    }
    
    if (cancelled) {
         closesocket(serverSocket);
         for(SOCKET s : client_sockets) closesocket(s);
         client_sockets.clear();
         is_lan_mode = false;
         return false;
    }

    outtextxy(50, 300, _T("All Players Connected! Preparing Game..."));
    FlushBatchDraw();
    Sleep(1000);
    
    outtextxy(50, 300, _T("All Players Connected! Preparing Game..."));
    FlushBatchDraw();
    Sleep(1000);
    
    log_debug("init_network_server success. Closing listener.");
    closesocket(serverSocket); 
    return true;
}

void init_network_client() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    connSocket = socket(AF_INET, SOCK_STREAM, 0);

    FreeConsole(); 
    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    
    std::string ip_str;
    std::cout << "========================================" << std::endl;
    std::cout << "Enter Server IP (shown on Host screen): ";
    std::cin >> ip_str;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(ip_str.c_str());
    serverAddr.sin_port = htons(8888);

    std::cout << "Connecting to " << ip_str << "..." << std::endl;
    
    int timeout = 5000;
    setsockopt(connSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(connSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    
    int flag = 1;
    setsockopt(connSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    if (connect(connSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "Connection Failed!" << std::endl;
        system("pause");
        exit(0);
    }
    std::cout << "Connected! Waiting for Game Data..." << std::endl;
    
    std::cout << "Connected! Waiting for Game Data..." << std::endl;
    
    std::vector<int> id_packet = recv_vect();
    if (id_packet.size() >= 2 && id_packet[0] == -1) {
        local_player_id = id_packet[1];
        std::cout << "Assigned Player ID: " << local_player_id << std::endl;
    } else {
        std::cout << "Protocol Error: ID not received." << std::endl;
    }

    int zero_timeout = 0;
    setsockopt(connSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&zero_timeout, sizeof(zero_timeout));

    Sleep(500); 
    FreeConsole(); 
}

/******************************************************************************
 * AI & LOGIC
 ******************************************************************************/
const char* getTypeName(int type) {
    switch(type) {
        case 0: return "Human";
        case 1: return "AI Legacy";
        case 2: return "AI v0.1";
        case 3: return "AI v0.2";
        case 4: return "AI v0.3 (Aggro)";
        case 5: return "AI v0.4 (General)";
        case 6: return "AI v0.5 (Warlord)";
        case 7: return "AI v0.6 (Strategist)";
        default: return "Unknown";
    }
}

oprate callAI(int type, int h, int w, const std::vector<std::vector<plot>>& map, int state) {
    if (type == 1) return getAIDecision(h, w, map, state);
    if (type == 2) return getAIDecision01(h, w, map, state);
    if (type == 3) return getAIDecision02(h, w, map, state);
    if (type == 4) return getAIDecision03(h, w, map, state);
    if (type == 5) return getAIDecision04(h, w, map, state);
    if (type == 6) return getAIDecision05(h, w, map, state);
    if (type == 7) return getAIDecision06(h, w, map, state);
    
    return {0,0,0,0,0};
    
    return {0,0,0,0,0};
}

void moveopratepos(int player, int dx, int dy) {
    int* opx = nullptr;
    int* opy = nullptr;
    if (player == 1) { opx = &p1_opx; opy = &p1_opy; }
    else if (player == 2) { opx = &p2_opx; opy = &p2_opy; }
    else if (player == 3) { opx = &p3_opx; opy = &p3_opy; }
    else if (player == 4) { opx = &p4_opx; opy = &p4_opy; }
    else if (player == 5) { opx = &p5_opx; opy = &p5_opy; }
    else if (player == 6) { opx = &p6_opx; opy = &p6_opy; }
    else if (player == 7) { opx = &p7_opx; opy = &p7_opy; }
    else if (player == 8) { opx = &p8_opx; opy = &p8_opy; }
    else return;
    
    *opx += dx;
    *opy += dy;
    if (*opx > matrix_width)  *opx = matrix_width;
    if (*opx < 1)             *opx = 1;
    if (*opy > matrix_height) *opy = matrix_height;
    if (*opy < 1)             *opy = 1;
}

void generate(int type, int sum) {
    int i = 1;
    while (i <= sum) {
        int x = rand() % matrix_width + 1;
        int y = rand() % matrix_height + 1;
        if (land[y][x].type == 0) { 
            land[y][x].type = type;
            if (type == 2) land[y][x].army = 40 + rand() % 10; 
            i++;
        }
    }
}

void towerincrease() {
    for (int i = 1; i <= matrix_height; i++) {
        for (int j = 1; j <= matrix_width; j++) {
            if (land[i][j].type == 2 && land[i][j].state != 0) { 
                land[i][j].army++;
            }
            if (land[i][j].type == 3 && land[i][j].state != 0) { 
                land[i][j].army++;
            }
        }
    }
}

void landincrease() {
    for (int i = 1; i <= matrix_height; i++) {
        for (int j = 1; j <= matrix_width; j++) {
            if (land[i][j].state != 0 && land[i][j].type != 1) { 
                land[i][j].army++;
            }
        }
    }
}

void updateFog() {
    memset(is_visible, 0, sizeof(is_visible));
    int viewer = 1;
    bool enable_fog = false;

    if (is_lan_mode) {
        viewer = local_player_id;
        enable_fog = true;
    } else {

        bool p1_human = (p1_type == 0);
        bool p2_human = (p2_type == 0);
        bool p3_human = (player_count >= 3 && p3_type == 0);
        bool p4_human = (player_count >= 4 && p4_type == 0);
        bool p5_human = (player_count >= 5 && p5_type == 0);
        bool p6_human = (player_count >= 6 && p6_type == 0);
        bool p7_human = (player_count >= 7 && p7_type == 0);
        bool p8_human = (player_count >= 8 && p8_type == 0);

        int human_count = 0;
        int viewer_candidate = 0;

        if (p1_human) { human_count++; viewer_candidate = 1; }
        if (p2_human) { human_count++; viewer_candidate = 2; }
        if (p3_human) { human_count++; viewer_candidate = 3; }
        if (p4_human) { human_count++; viewer_candidate = 4; }
        if (p5_human) { human_count++; viewer_candidate = 5; }
        if (p6_human) { human_count++; viewer_candidate = 6; }
        if (p7_human) { human_count++; viewer_candidate = 7; }
        if (p8_human) { human_count++; viewer_candidate = 8; }

        if (human_count == 1) {
            viewer = viewer_candidate;
            enable_fog = true;
        } else {
            enable_fog = false;
        }
    }

    if (!enable_fog) {
        memset(is_visible, 1, sizeof(is_visible));
        return;
    }

    for(int y=1; y<=matrix_height; y++) {
        for(int x=1; x<=matrix_width; x++) {
            if(land[y][x].state == viewer) { 
                for(int dy=-2; dy<=2; dy++) {
                    for(int dx=-2; dx<=2; dx++) {
                        int ny = y + dy;
                        int nx = x + dx;
                        if(ny >= 1 && ny <= matrix_height && nx >= 1 && nx <= matrix_width) {
                            is_visible[ny][nx] = true;
                        }
                    }
                }
            }
        }
    }
}

// [New] AI 迷雾生成器
std::vector<std::vector<plot>> getFoggedMap(int player) {
    // 1. 初始化全迷雾地图
    std::vector<std::vector<plot>> fogMap(matrix_height + 2, std::vector<plot>(matrix_width + 2));
    
    // 临时可见性数组
    bool ai_visible[MAX_MATRIX_HEIGHT + 5][MAX_MATRIX_WIDTH + 5];
    memset(ai_visible, 0, sizeof(ai_visible));

    // 2. 计算 AI 视野 (同 updateFog 逻辑)
    for(int y=1; y<=matrix_height; y++) {
        for(int x=1; x<=matrix_width; x++) {
            if(land[y][x].state == player) { 
                for(int dy=-2; dy<=2; dy++) {
                    for(int dx=-2; dx<=2; dx++) {
                        int ny = y + dy;
                        int nx = x + dx;
                        if(ny >= 1 && ny <= matrix_height && nx >= 1 && nx <= matrix_width) {
                            ai_visible[ny][nx] = true;
                        }
                    }
                }
            }
        }
    }

    // 3. 填充地图数据
    for(int r = 1; r <= matrix_height; ++r) {
        for(int c = 1; c <= matrix_width; ++c) {
            if(ai_visible[r][c]) {
                fogMap[r][c] = land[r][c]; // 可见：复制真实数据
            } else {
                // 不可见：设置为迷雾占位符
                // 使用 type = -1 表示迷雾
                fogMap[r][c].type = -1;  
                fogMap[r][c].state = -1; // 未知归属
                fogMap[r][c].army = 0;   // 未知兵力
            }
        }
    }
    return fogMap;
}

void issueOrder(int sx, int sy, int dx, int dy, int player) {
    if (is_lan_mode) {
        if (player == local_player_id) {
            std::vector<int> moveData = {sx, sy, dx, dy, player};
            send_vect(moveData);
        } else if (is_server) {
            // Server generating AI move
            bool is_ai = false;
            if (player == 1 && p1_type != 0) is_ai = true;
            if (player == 2 && p2_type != 0) is_ai = true;
            if (player == 3 && p3_type != 0) is_ai = true;
            if (player == 4 && p4_type != 0) is_ai = true;
            if (player == 5 && p5_type != 0) is_ai = true;
            if (player == 6 && p6_type != 0) is_ai = true;
            if (player == 7 && p7_type != 0) is_ai = true;
            if (player == 8 && p8_type != 0) is_ai = true;
            
            if (is_ai) {
                std::vector<int> moveData = {sx, sy, dx, dy, player};
                broadcast_vect(moveData);
            }
        }
    }
    
    oprate_tail[player]++;
    if (oprate_tail[player] > N) oprate_tail[player] -= N;
    oprate_queue[player][oprate_tail[player]] = { sx, sy, dx, dy, player };
    if (!bo_opratequeue[player]) bo_opratequeue[player] = true;
}

void applyRemoteOrder(int sx, int sy, int dx, int dy, int player) {
    oprate_tail[player]++;
    if (oprate_tail[player] > N) oprate_tail[player] -= N;
    oprate_queue[player][oprate_tail[player]] = { sx, sy, dx, dy, player };
    if (!bo_opratequeue[player]) bo_opratequeue[player] = true;
}

void extendqueue(int dx, int dy, int player) {
    int sx, sy;
    if (player == 1) { sx = p1_opx; sy = p1_opy; }
    else if (player == 2) { sx = p2_opx; sy = p2_opy; }
    else { sx = p3_opx; sy = p3_opy; }
    
    issueOrder(sx, sy, dx, dy, player);
}

bool check(oprate q) {
    if (q.sx + q.dx > matrix_width || q.sx + q.dx < 1) return false;
    if (q.sy + q.dy > matrix_height || q.sy + q.dy < 1) return false;
    if (land[q.sy][q.sx].state != q.player) return false;
    if (land[q.sy][q.sx].army <= 1) return false;
    if (land[q.sy + q.dy][q.sx + q.dx].type == 1) return false;
    return true;
}

void resolve_move(int player) {
    oprate k = oprate_queue[player][oprate_head[player]]; 
    if (!check(k)) {
        oprate_head[player]++;
        if (oprate_head[player] > N) oprate_head[player] -= N;
        if (oprate_head[player] == oprate_tail[player] + 1) bo_opratequeue[player] = false;
        if (bo_opratequeue[player]) resolve_move(player);
        return;
    }
    int tx = k.sx + k.dx;
    int ty = k.sy + k.dy;
    int myID = k.player;
    int targetState = land[ty][tx].state;
    int moveArmy = land[k.sy][k.sx].army - 1; 

    // Sound Logic Helpers
    auto is_local_human = [&](int pid) {
        if (is_lan_mode) return pid == local_player_id;
        int p_type = 7; 
        switch(pid) {
            case 1: p_type = p1_type; break;
            case 2: p_type = p2_type; break;
            case 3: p_type = p3_type; break;
            case 4: p_type = p4_type; break; 
            case 5: p_type = p5_type; break;
            case 6: p_type = p6_type; break;
            case 7: p_type = p7_type; break; 
            case 8: p_type = p8_type; break;
        }
        return p_type == 0;
    };
    
    // Event Flags
    bool sound_played = false;

    if (targetState == myID) {
        land[ty][tx].army += moveArmy;
    }
    else {
        if (land[ty][tx].type != 2 && targetState == 0) {
            // Capture Empty Land
            land[ty][tx].state = myID;
            land[ty][tx].army += moveArmy;
            
            // Sound: EatEmpty (Only if Local Human)
            if (is_local_human(myID)) {
                play_sound(_T("EatEmpty.wav"));
            }
        }
        else {
            if (moveArmy > land[ty][tx].army) {
                land[ty][tx].army = moveArmy - land[ty][tx].army;
                land[ty][tx].state = myID;
                
                // Sound: Capture (Enemy or Neutral) check
                // Will be overridden if King Kill
                bool played_special = false;

                if (land[ty][tx].type == 3) {
                    // King Capture -> ALWAYS PLAY
                    play_sound(_T("KillKing.wav"));
                    played_special = true;

                    land[ty][tx].type = 2; // Convert captured Crown to Tower
                    int eliminated = targetState;  
                    for(int y=1; y<=matrix_height; y++) {
                        for(int x=1; x<=matrix_width; x++) {
                            if(land[y][x].state == eliminated && !(y == ty && x == tx)) {
                                if(land[y][x].type == 2) {
                                    land[y][x].state = myID;
                                    land[y][x].army = 0;
                                } else {
                                    land[y][x].state = 0;
                                    land[y][x].army = 0;
                                }
                            }
                        }
                    }
                    if (player_count == 2) {
                        is_game_over = true;
                        winner = myID;
                    } else {
                        bool alive[9] = {false, false, false, false, false, false, false, false, false};
                        for(int y=1; y<=matrix_height; y++) {
                            for(int x=1; x<=matrix_width; x++) {
                                if(land[y][x].type == 3 && land[y][x].state > 0) {
                                    alive[land[y][x].state] = true;
                                }
                            }
                        }
                        int alive_count = 0;
                        int last_alive = 0;
                        for(int i=1; i<=8; i++) {
                            if(alive[i]) { alive_count++; last_alive = i; }
                        }
                        if(alive_count <= 1) { 
                            is_game_over = true;
                            winner = last_alive;
                        }
                    }
                }
                
                // Normal Capture Logic
                if (!played_special && is_local_human(myID)) {
                    if (land[ty][tx].type == 2) {
                        if (targetState == 0) play_sound(_T("EatNeutral.mp3"));
                        else play_sound(_T("EatAnemyTower.wav"));
                    } else {
                         // Enemy Land
                         play_sound(_T("EatEmpty.wav"));
                    }
                }

            } else {
                land[ty][tx].army -= moveArmy;
            }
        }
    }
    land[k.sy][k.sx].army = 1;
    oprate_head[player]++;
    if (oprate_head[player] > N) oprate_head[player] -= N;
    if (oprate_head[player] == oprate_tail[player] + 1) bo_opratequeue[player] = false;
}

bool checkConnectivity(int p1x, int p1y) {
    bool visited[MAX_MATRIX_HEIGHT + 5][MAX_MATRIX_WIDTH + 5];
    memset(visited, 0, sizeof(visited));
    struct Point { int x, y; };
    std::queue<Point> q;
    q.push({p1x, p1y});
    visited[p1y][p1x] = true;
    int reachable_count = 0;
    
    while(!q.empty()) {
        Point curr = q.front();
        q.pop();
        reachable_count++;
        int dx[] = {0, 0, -1, 1};
        int dy[] = {-1, 1, 0, 0};
        for(int i=0; i<4; i++) {
            int nx = curr.x + dx[i];
            int ny = curr.y + dy[i];
            if(nx < 1 || nx > matrix_width || ny < 1 || ny > matrix_height) continue;
            if(visited[ny][nx]) continue;
            if(land[ny][nx].type == 1) continue;
            visited[ny][nx] = true;
            q.push({nx, ny});
        }
    }
    return (reachable_count > 20); 
}

// 简单的曼哈顿距离
inline int dist(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

/******************************************************************************
 * MAIN FUNCTION
 ******************************************************************************/

// [New] 统一的窗口缩放处理函数
void check_and_handle_resize(bool force) {
    RECT client_rect;
    GetClientRect(window_pointer, &client_rect);
    int new_w = client_rect.right - client_rect.left;
    int new_h = client_rect.bottom - client_rect.top;

    if (force || (new_w > 0 && new_h > 0 && (new_w != window_width || new_h != window_height))) {
        window_width = new_w;
        window_height = new_h;
        int size = std::min(window_width / matrix_width, window_height / matrix_height);
        
        plot_width = size;
        plot_height = size;
        offset_x = (window_width - plot_width * matrix_width) / 2;
        offset_y = (window_height - plot_height * matrix_height) / 2;
        
        reload_assets(plot_width, plot_height);
        
        // 标记需要重置缓冲区 (防抖)
        resize_pending = true;
        last_resize_tick = clock();

        draw_background();
    }

    // 防抖 Resize 逻辑
    if (resize_pending && (clock() - last_resize_tick > 200)) {
        resize_pending = false;
        
        // 1. 获取当前窗口位置和大小
        RECT current_rect;
        GetWindowRect(window_pointer, &current_rect);
        int win_w = current_rect.right - current_rect.left;
        int win_h = current_rect.bottom - current_rect.top;

        // 2. 调整 EasyX 绘图缓冲区大小
        Resize(NULL, window_width, window_height); 

        // 3. 强制恢复窗口大小
        MoveWindow(window_pointer, current_rect.left, current_rect.top, win_w, win_h, TRUE);
        
        // 4. 重绘
        draw_background();
    }
}
int main() {
    initgraph(window_width, window_height);
    setbkmode(TRANSPARENT); 
    
    // 初始化背景图片
    init_background();
    
    plot_width = window_width / matrix_width;
    plot_height = window_height / matrix_height;

    reload_assets(plot_width, plot_height);
    
    srand(time(NULL));
	window_pointer = GetHWnd();
    SetWindowLong(window_pointer, GWL_STYLE, GetWindowLong(window_pointer, GWL_STYLE) | WS_SIZEBOX | WS_MAXIMIZEBOX);
    SetWindowLong(window_pointer, GWL_STYLE, GetWindowLong(window_pointer, GWL_STYLE) | WS_SIZEBOX | WS_MAXIMIZEBOX);
    
    // --- Audio Init & Mixing ---
    // 1. BGM: Lower volume (300/1000) to make SFX pop
    mciSendString(_T("open \"sounds/bgm.mp3\" alias bgm"), NULL, 0, NULL);
    mciSendString(_T("setaudio bgm volume to 500"), NULL, 0, NULL);
    mciSendString(_T("play bgm repeat"), NULL, 0, NULL);

    // 2. Pre-load SFX with Max Volume (1000)
    // Click
    mciSendString(_T("open \"sounds/click.wav\" alias sfx_click"), NULL, 0, NULL);
    mciSendString(_T("setaudio sfx_click volume to 1000"), NULL, 0, NULL);
    
    // Game SFX
    mciSendString(_T("open \"sounds/EatEmpty.wav\" alias sfx_eat_empty"), NULL, 0, NULL);
    mciSendString(_T("setaudio sfx_eat_empty volume to 1000"), NULL, 0, NULL);

    mciSendString(_T("open \"sounds/EatNeutral.mp3\" alias sfx_eat_neutral"), NULL, 0, NULL);
    mciSendString(_T("setaudio sfx_eat_neutral volume to 1000"), NULL, 0, NULL);

    mciSendString(_T("open \"sounds/EatAnemyTower.wav\" alias sfx_eat_enemy"), NULL, 0, NULL);
    mciSendString(_T("setaudio sfx_eat_enemy volume to 400"), NULL, 0, NULL);

    mciSendString(_T("open \"sounds/KillKing.wav\" alias sfx_kill"), NULL, 0, NULL);
    mciSendString(_T("setaudio sfx_kill volume to 1000"), NULL, 0, NULL);



GAME_START:
    is_game_over = false;
    winner = 0;
    timm = 0;
    is_lan_mode = false;
    player_count = 2;
    memset(oprate_head, 0, sizeof(oprate_head));
    memset(oprate_tail, 0, sizeof(oprate_tail));
    memset(bo_opratequeue, 0, sizeof(bo_opratequeue));
    if(connSocket != INVALID_SOCKET) { closesocket(connSocket); connSocket = INVALID_SOCKET; }
    for(SOCKET s : client_sockets) closesocket(s);
    client_sockets.clear();
    BeginBatchDraw();
    while(true) {
        check_and_handle_resize(); // [Fix] 主菜单也支持缩放
        draw_background(); // 绘制背景
        
        // 绘制主菜单面板
        int panel_w = 500;
        int panel_h = 400;
        int panel_x = (window_width - panel_w) / 2;
        int panel_y = (window_height - panel_h) / 2;
        
        // 半透明背景模拟 (深色)
        drawRoundBox(panel_x, panel_y, panel_w, panel_h, RGB(30, 30, 40), 200);
        
        // 标题 (Removed as per request)
        // drawShadowText(panel_x + 130, panel_y + 40, _T("GENERALS.IO"), 50, RGB(255, 215, 0));
        // drawShadowText(panel_x + 190, panel_y + 90, _T("C++ EDITION"), 20, RGB(200, 200, 200));

        // 按钮位置 - 稍微上移以平衡视觉
        int btn_w = 300;
        int btn_h = 60;
        int btn_x = panel_x + (panel_w - btn_w) / 2;
        int btn_y_start = panel_y + 100; // Adjusted from 150
        
        // 按钮 1: 本地游戏
        bool hover1 = drawModernButton(btn_x, btn_y_start, btn_w, btn_h, _T("1. Local Game"), false);
        
        // 按钮 2: 局域网联机
        bool hover2 = drawModernButton(btn_x, btn_y_start + 80, btn_w, btn_h, _T("2. LAN Multiplayer"), false);
        
        // 按钮 3: 退出游戏
        bool hover3 = drawModernButton(btn_x, btn_y_start + 160, btn_w, btn_h, _T("3. Exit Game"), false);
        
        FlushBatchDraw();

        // 更新输入
        update_input();
        bool click = mouse_lb; // Use tracked state OR GetAsyncKeyState
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) click = true; // Fallback
        
        // 逻辑处理
        if((hover1 && click) || (GetAsyncKeyState('1') & 0x8000)) {
            play_sound(_T("click.wav")); // 假设有点击音效，如果没有也没关系
            Sleep(200);
            // reset mouse state to prevent double click
             mouse_lb = false; 
            goto LOCAL_GAME_SETUP;
        }
        if((hover2 && click) || (GetAsyncKeyState('2') & 0x8000)) {
            play_sound(_T("click.wav"));
            is_lan_mode = true;
            Sleep(200);
             mouse_lb = false; 
            break;
        }
        if((hover3 && click) || (GetAsyncKeyState('3') & 0x8000) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
            play_sound(_T("click.wav"));
            Sleep(200);
            exit(0);
        }
        
        Sleep(30); 
    }
    

    
    goto MODE_SELECTED;
    
LOCAL_GAME_SETUP:
    // Defaults
    p1_type = 0;  
    p2_type = 3;  
    p3_type = 4;
    p4_type = 7;
    player_count = 2;
    
    {
        int setup_step = 0; // 0: Count, 1: P1, 2: P2, 3: P3, 4: P4, 5: Map
        
        while(true) {
            check_and_handle_resize(); 
            draw_background();
            
            // Layout constants
            int panel_w = 600;
            int panel_h = 500;
            int panel_x = (window_width - panel_w) / 2;
            int panel_y = (window_height - panel_h) / 2 + 50;
            
            drawRoundBox(panel_x, panel_y, panel_w, panel_h, RGB(30,30,40), 230);
            
            int btn_w = 400;
            int btn_h = 60;
            int btn_x = panel_x + (panel_w - btn_w) / 2;
            int content_y = panel_y + 120;
            
            // --- STEP 0: Select Player Count ---
            if (setup_step == 0) {
                drawShadowText(panel_x + 100, panel_y + 40, _T("SELECT PLAYER COUNT"), 35, WHITE);
                
                TCHAR countStr[100];
                _stprintf(countStr, _T("Player Count: %d"), player_count);
                bool hoverCount = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, countStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to shuffle"), 20, RGB(180,180,180));
                
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverExit = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("EXIT"), false);
                
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                
                // Keyboard: Space = toggle setting, Enter = next, Esc = exit
                if ((hoverCount && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { 
                    player_count++;
                    if (player_count > 8) player_count = 2;
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { setup_step = 1; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverExit && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { play_sound(_T("click.wav")); Sleep(200); goto GAME_START; }
            }
            // --- STEP 1: P1 Setup ---
            else if (setup_step == 1) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 1"), 35, RGB(255, 100, 100)); // Red
                
                TCHAR typeStr[100];
                _stprintf(typeStr, _T("Type: %hs"), getTypeName(p1_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                
                drawShadowText(btn_x, content_y + 120, _T("Click to change type"), 20, RGB(180,180,180));

                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                
                // Keyboard: Space = toggle setting, Enter = next, Esc = back
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { 
                    p1_type = (p1_type + 1) % 8; 
                    play_sound(_T("click.wav")); 
                    Sleep(200); mouse_lb = false; 
                }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { 
                    setup_step = 2; 
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { 
                    setup_step = 0; 
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
            }
            // --- STEP 2: P2 Setup ---
            else if (setup_step == 2) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 2"), 35, RGB(100, 100, 255)); // Blue
                
                TCHAR typeStr[100];
                _stprintf(typeStr, _T("Type: %hs"), getTypeName(p2_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);

                drawShadowText(btn_x, content_y + 120, _T("Click to change type"), 20, RGB(180,180,180));

                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                
                // Keyboard: Space = toggle setting, Enter = next, Esc = back
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { 
                    p2_type = (p2_type + 1) % 8; 
                    play_sound(_T("click.wav")); 
                    Sleep(200); mouse_lb = false; 
                }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { 
                    if(player_count == 2) setup_step = 9; // Skip rest, go to Map
                    else setup_step = 3; 
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { 
                    setup_step = 1; 
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
            }
            // --- STEP 3: P3 Setup ---
            else if (setup_step == 3) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 3"), 35, RGB(200, 100, 255)); // Purple
                
                TCHAR typeStr[100];
                _stprintf(typeStr, _T("Type: %hs"), getTypeName(p3_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);

                drawShadowText(btn_x, content_y + 120, _T("Click to change type"), 20, RGB(180,180,180));

                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                
                // Keyboard: Space = toggle setting, Enter = next, Esc = back
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { 
                    p3_type = (p3_type + 1) % 8; 
                    play_sound(_T("click.wav")); 
                    Sleep(200); mouse_lb = false; 
                }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { 
                    if(player_count == 3) setup_step = 9; // Skip rest
                    else setup_step = 4; // Go to P4
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { 
                    setup_step = 2; 
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
            }
            // --- STEP 4: P4 Setup ---
            else if (setup_step == 4) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 4"), 35, RGB(50, 200, 50)); // Green

                TCHAR typeStr[100];
                _stprintf(typeStr, _T("Type: %hs"), getTypeName(p4_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);

                drawShadowText(btn_x, content_y + 120, _T("Click to change type"), 20, RGB(180,180,180));

                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { 
                    p4_type = (p4_type + 1) % 8; 
                    play_sound(_T("click.wav")); 
                    Sleep(200); mouse_lb = false; 
                }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { 
                    if(player_count == 4) setup_step = 9; // Map
                    else setup_step = 5; // P5
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { 
                    setup_step = 3; 
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
            }
            // --- STEP 5: P5 Setup ---
            else if (setup_step == 5) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 5"), 35, RGB(0, 255, 255)); // Cyan
                TCHAR typeStr[100];
                _stprintf(typeStr, _T("Type: %hs"), getTypeName(p5_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to change type"), 20, RGB(180,180,180));
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { p5_type = (p5_type + 1) % 8; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { if(player_count == 5) setup_step = 9; else setup_step = 6; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 4; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
            // --- STEP 6: P6 Setup ---
            else if (setup_step == 6) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 6"), 35, RGB(255, 255, 0)); // Yellow
                TCHAR typeStr[100];
                _stprintf(typeStr, _T("Type: %hs"), getTypeName(p6_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to change type"), 20, RGB(180,180,180));
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { p6_type = (p6_type + 1) % 8; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { if(player_count == 6) setup_step = 9; else setup_step = 7; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 5; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
             // --- STEP 7: P7 Setup ---
            else if (setup_step == 7) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 7"), 35, RGB(255, 165, 0)); // Orange
                TCHAR typeStr[100];
                _stprintf(typeStr, _T("Type: %hs"), getTypeName(p7_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to change type"), 20, RGB(180,180,180));
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { p7_type = (p7_type + 1) % 8; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { if(player_count == 7) setup_step = 9; else setup_step = 8; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 6; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
             // --- STEP 8: P8 Setup ---
            else if (setup_step == 8) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 8"), 35, RGB(128, 128, 128)); // Grey
                TCHAR typeStr[100];
                _stprintf(typeStr, _T("Type: %hs"), getTypeName(p8_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to change type"), 20, RGB(180,180,180));
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { p8_type = (p8_type + 1) % 8; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { setup_step = 9; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 7; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
            // --- STEP 9: Map & Start ---
            else if (setup_step == 9) {
                 drawShadowText(panel_x + 150, panel_y + 40, _T("MAP SETTINGS"), 35, RGB(100, 255, 100));
                 
                 TCHAR typeStr[100];
                 _stprintf(typeStr, _T("Size: %hs"), getMapSizeName(map_size_option));
                 bool hoverMap = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                 
                 drawShadowText(btn_x, content_y + 120, _T("Click to change size"), 20, RGB(180,180,180));
                 
                 bool hoverStart = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("START GAME"), true);
                 bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                 
                 FlushBatchDraw();
                 update_input();
                 bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                 
                 // Keyboard: Space = toggle setting, Enter = start, Esc = back
                 if ((hoverMap && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
                     map_size_option = (map_size_option + 1) % 4;
                     play_sound(_T("click.wav")); Sleep(200); mouse_lb = false;
                 }
                 if ((hoverStart && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) {
                     play_sound(_T("click.wav")); Sleep(200); 
                     break; // Breaks loop, starts game
                 }
                 if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
                     if(player_count == 8) setup_step = 8;
                     else if(player_count == 7) setup_step = 7;
                     else if(player_count == 6) setup_step = 6;
                     else if(player_count == 5) setup_step = 5;
                     else if(player_count == 4) setup_step = 4;
                     else if(player_count == 3) setup_step = 3;
                     else setup_step = 2;
                     play_sound(_T("click.wav")); Sleep(200); mouse_lb = false;
                 }
            }
            
            Sleep(30);
        }
    }
    
    // Apply Map Size
    setMapSize(map_size_option);
    update_resource_sums();
    check_and_handle_resize(true);

    goto POST_LAN_SELECTION;

LAN_HOST_SETUP:
    {
        int setup_step = 0; // 0: Count, 1: P1(Skip/Info), 2: P2, 3: P3, 4: P4, 5: Map
        while(true) {
            check_and_handle_resize();
            draw_background();
            
            int panel_w = 600;
            int panel_h = 500;
            int panel_x = (window_width - panel_w) / 2;
            int panel_y = (window_height - panel_h) / 2 + 50;
            
            drawRoundBox(panel_x, panel_y, panel_w, panel_h, RGB(30,30,50), 230);
            
            int btn_w = 400;
            int btn_h = 60;
            int btn_x = panel_x + (panel_w - btn_w) / 2;
            int content_y = panel_y + 120;
            
            if (setup_step == 0) {
                drawShadowText(panel_x + 100, panel_y + 40, _T("SELECT PLAYER COUNT"), 35, RGB(100, 200, 255));
                
                TCHAR countStr[100];
                _stprintf(countStr, _T("Player Count: %d"), player_count);
                bool hoverCount = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, countStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to shuffle"), 20, RGB(180,180,180));
                
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverExit = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("EXIT"), false);
                
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                
                // Keyboard: Space = toggle setting, Enter = next, Esc = exit
                if ((hoverCount && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { 
                    player_count++;
                    if (player_count > 8) player_count = 2;
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { setup_step = 1; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverExit && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { play_sound(_T("click.wav")); Sleep(200); goto MODE_SELECTED; }
            }
            else if (setup_step == 1) {
                // P1 is always Human Host in LAN
                drawShadowText(panel_x + 140, panel_y + 40, _T("HOST PLAYER (YOU)"), 35, RGB(255, 100, 100));
                
                drawModernButton(btn_x, content_y + 40, btn_w, btn_h, _T("Type: Human (Host)"), false);
                drawShadowText(btn_x, content_y + 120, _T("You are the server host."), 20, RGB(180,180,180));
                
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                
                // Keyboard: Enter = next, Esc = back
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { setup_step = 2; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 0; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
            else if (setup_step == 2) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 2"), 35, RGB(100, 100, 255));
                
                TCHAR typeStr[100];
                if (p2_type == 0) _stprintf(typeStr, _T("Type: Human (Client)"));
                else _stprintf(typeStr, _T("Type: %hs"), getTypeName(p2_type));
                
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                 drawShadowText(btn_x, content_y + 120, _T("Click to toggle Human/AI"), 20, RGB(180,180,180));
                
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                
                 // Keyboard: Space = toggle setting, Enter = next, Esc = back
                 if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { 
                     // In LAN, toggle between Human (0) and AI (7). Or cycle AIs?
                     // Let's cycle 0 -> 7 -> 0
                     if (p2_type == 0) p2_type = 7; 
                     else p2_type = 0;
                     play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { 
                    if(player_count == 2) setup_step = 9; // Skip P3/P4+, go to Map
                    else setup_step = 3; 
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 1; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
            else if (setup_step == 3) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 3"), 35, RGB(200, 100, 255));
                
                TCHAR typeStr[100];
                if (p3_type == 0) _stprintf(typeStr, _T("Type: Human (Client)"));
                else _stprintf(typeStr, _T("Type: %hs"), getTypeName(p3_type));
                
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to toggle Human/AI"), 20, RGB(180,180,180));
                
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                
                 // Keyboard: Space = toggle setting, Enter = next, Esc = back
                 if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { 
                     if (p3_type == 0) p3_type = 7; 
                     else p3_type = 0;
                     play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { 
                       if(player_count == 3) setup_step = 9; // Skip rest
                       else setup_step = 4; 
                       play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 2; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
            else if (setup_step == 4) { // New P4 Setup
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 4"), 35, RGB(50, 200, 50));
                
                TCHAR typeStr[100];
                if (p4_type == 0) _stprintf(typeStr, _T("Type: Human (Client)"));
                else _stprintf(typeStr, _T("Type: %hs"), getTypeName(p4_type));
                
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to toggle Human/AI"), 20, RGB(180,180,180));
                
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                
                 // Keyboard: Space = toggle setting, Enter = next, Esc = back
                 if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { 
                     if (p4_type == 0) p4_type = 7; 
                     else p4_type = 0;
                     play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { 
                    if(player_count == 4) setup_step = 9;
                    else setup_step = 5; 
                    play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; 
                }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 3; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
            // --- STEP 5: P5 Setup ---
            else if (setup_step == 5) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 5"), 35, RGB(0, 255, 255)); // Cyan
                TCHAR typeStr[100];
                if (p5_type == 0) _stprintf(typeStr, _T("Type: Human (Client)"));
                else _stprintf(typeStr, _T("Type: %hs"), getTypeName(p5_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to toggle Human/AI"), 20, RGB(180,180,180));
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { if (p5_type == 0) p5_type = 7; else p5_type = 0; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { if(player_count == 5) setup_step = 9; else setup_step = 6; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 4; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
            // --- STEP 6: P6 Setup ---
            else if (setup_step == 6) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 6"), 35, RGB(255, 255, 0)); // Yellow
                TCHAR typeStr[100];
                if (p6_type == 0) _stprintf(typeStr, _T("Type: Human (Client)"));
                else _stprintf(typeStr, _T("Type: %hs"), getTypeName(p6_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to toggle Human/AI"), 20, RGB(180,180,180));
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { if (p6_type == 0) p6_type = 7; else p6_type = 0; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { if(player_count == 6) setup_step = 9; else setup_step = 7; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 5; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
            // --- STEP 7: P7 Setup ---
            else if (setup_step == 7) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 7"), 35, RGB(255, 165, 0)); // Orange
                TCHAR typeStr[100];
                if (p7_type == 0) _stprintf(typeStr, _T("Type: Human (Client)"));
                else _stprintf(typeStr, _T("Type: %hs"), getTypeName(p7_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to toggle Human/AI"), 20, RGB(180,180,180));
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { if (p7_type == 0) p7_type = 7; else p7_type = 0; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { if(player_count == 7) setup_step = 9; else setup_step = 8; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 6; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
             // --- STEP 8: P8 Setup ---
            else if (setup_step == 8) {
                drawShadowText(panel_x + 180, panel_y + 40, _T("SETUP PLAYER 8"), 35, RGB(128, 128, 128)); // Grey
                TCHAR typeStr[100];
                if (p8_type == 0) _stprintf(typeStr, _T("Type: Human (Client)"));
                else _stprintf(typeStr, _T("Type: %hs"), getTypeName(p8_type));
                bool hoverType = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                drawShadowText(btn_x, content_y + 120, _T("Click to toggle Human/AI"), 20, RGB(180,180,180));
                bool hoverNext = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("NEXT >"), true);
                bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                FlushBatchDraw();
                update_input();
                bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                if ((hoverType && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { if (p8_type == 0) p8_type = 7; else p8_type = 0; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverNext && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) { setup_step = 9; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
                if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { setup_step = 7; play_sound(_T("click.wav")); Sleep(200); mouse_lb = false; }
            }
            else if (setup_step == 9) {
                 drawShadowText(panel_x + 150, panel_y + 40, _T("MAP SETTINGS"), 35, RGB(100, 255, 100));
                 
                 TCHAR typeStr[100];
                 _stprintf(typeStr, _T("Size: %hs"), getMapSizeName(map_size_option));
                 bool hoverMap = drawModernButton(btn_x, content_y + 40, btn_w, btn_h, typeStr, false);
                 
                 bool hoverStart = drawModernButton(panel_x + 350, panel_y + 400, 200, 60, _T("START SERVER"), true);
                 bool hoverBack = drawModernButton(panel_x + 50, panel_y + 400, 200, 60, _T("< BACK"), false);
                 
                 FlushBatchDraw();
                 update_input();
                 bool click = (mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000));
                 
                 // Keyboard: Space = toggle setting, Enter = start, Esc = back
                 if ((hoverMap && click) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
                     map_size_option = (map_size_option + 1) % 4;
                     play_sound(_T("click.wav")); Sleep(200); mouse_lb = false;
                 }
                 if ((hoverStart && click) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) {
                     play_sound(_T("click.wav")); Sleep(200); 
                     setMapSize(map_size_option);
                    update_resource_sums();
                    check_and_handle_resize(true);
                    if (init_network_server()) {
                        goto POST_LAN_SELECTION;
                    } else {
                        // Failed, maybe text warning?
                        // Stay on this step
                    }
                 }
                 if ((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
                     if(player_count == 8) setup_step = 8;
                     else if(player_count == 7) setup_step = 7;
                     else if(player_count == 6) setup_step = 6;
                     else if(player_count == 5) setup_step = 5;
                     else if(player_count == 4) setup_step = 4;
                     else if(player_count == 3) setup_step = 3;
                     else setup_step = 2;
                     play_sound(_T("click.wav")); Sleep(200); mouse_lb = false;
                 }
            }
            
            Sleep(30);
        }
    }

MODE_SELECTED:
    if (is_lan_mode) {
        while(true) {
            check_and_handle_resize(); // [Fix] LAN模式选择也支持缩放
            draw_background();
            
            // LAN 模式选择面板
            int panel_w = 500;
            int panel_h = 400;
            int panel_x = (window_width - panel_w) / 2;
            int panel_y = (window_height - panel_h) / 2;
            
            drawRoundBox(panel_x, panel_y, panel_w, panel_h, RGB(30, 30, 50), 220);
            drawShadowText(panel_x + 130, panel_y + 40, _T("LAN MODE"), 40, RGB(100, 200, 255));

            int btn_w = 300;
            int btn_h = 70;
            int btn_x = panel_x + (panel_w - btn_w) / 2;
            
            bool hoverHost = drawModernButton(btn_x, panel_y + 120, btn_w, btn_h, _T("1. HOST GAME"), false);
            bool hoverJoin = drawModernButton(btn_x, panel_y + 210, btn_w, btn_h, _T("2. JOIN GAME"), false);
            bool hoverBack = drawModernButton(btn_x, panel_y + 300, btn_w, 50, _T("BACK"), false);

            FlushBatchDraw();
            
            update_input();
            bool click = mouse_lb;
            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) click = true;
            
            // [Debug Info]
            /*
            TCHAR debug_msg[128];
            _stprintf(debug_msg, _T("Mouse: %d, %d | Join Hover: %d"), mouse_x, mouse_y, hoverJoin);
            outtextxy(10, 10, debug_msg);
            FlushBatchDraw();
            */

             if((hoverBack && click) || (GetAsyncKeyState(VK_ESCAPE) & 0x0001)) { 
                is_lan_mode = false; 
                play_sound(_T("click.wav"));
                Sleep(200); mouse_lb = false;
                goto GAME_START; 
            }
            if((hoverHost && click) || (GetAsyncKeyState('1') & 0x0001)) {
                play_sound(_T("click.wav"));
                is_server = true; local_player_id = 1;
                // Default settings for LAN
                p1_type = 0; // Host is always Human
                p2_type = 0; // Default P2 Human (Client)
                p3_type = 7; // Default P3 AI (Strategist) if 3 players
                p4_type = 7; 
                player_count = 2;
                Sleep(200); mouse_lb = false;
                goto LAN_HOST_SETUP;
            }
             if((hoverJoin && click) || (GetAsyncKeyState('2') & 0x0001)) {
                is_server = false; 
                p1_type = 0; p2_type = 0; p3_type = 0;
                init_network_client();
                break;
            }
            Sleep(30);
        }
        }

    
POST_LAN_SELECTION:
    
    if (is_lan_mode && !is_server) {
        draw_background();
        
        int w = 500, h = 300;
        int x = (window_width - w)/2;
        int y = (window_height - h)/2;
        
        drawRoundBox(x, y, w, h, RGB(20, 20, 30), 200);
        drawShadowText(x + 100, y + 100, _T("CONNECTED"), 40, RGB(100, 255, 100));
        drawShadowText(x + 120, y + 160, _T("Receiving Map..."), 24, WHITE);
        
        FlushBatchDraw();
        
        FlushBatchDraw();
        
        std::vector<int> mapData = recv_vect(); 
        
        if (mapData.empty()) { std::cerr << "Map Recv Failed (Empty)." << std::endl; return 0; }

        drawShadowText(x + 120, y + 160, _T("Map Received!   "), 24, WHITE); // Overwrite text
        FlushBatchDraw();
        Sleep(500);

        deserialize_map(mapData);
        // Force refresh layout for new map size
        check_and_handle_resize(true);

        for(int y=1; y<=matrix_height; y++) {
            for(int x=1; x<=matrix_width; x++) {
                if(land[y][x].state == 1 && land[y][x].type == 3) { p1_opx = x; p1_opy = y; }
                if(land[y][x].state == 2 && land[y][x].type == 3) { p2_opx = x; p2_opy = y; }
                if(land[y][x].state == 3 && land[y][x].type == 3) { p3_opx = x; p3_opy = y; }
                if(land[y][x].state == 4 && land[y][x].type == 3) { p4_opx = x; p4_opy = y; }
            }
        }
        int cx, cy;
        if (local_player_id == 1) { cx = p1_opx; cy = p1_opy; }
        else if (local_player_id == 2) { cx = p2_opx; cy = p2_opy; }
        else if (local_player_id == 3) { cx = p3_opx; cy = p3_opy; }
        else if (local_player_id == 4) { cx = p4_opx; cy = p4_opy; }

        if (p3_opx != 0 && p3_opy != 0) player_count = std::max(player_count, 3);
        if (p4_opx != 0 && p4_opy != 0) player_count = 4;
        else if (player_count < 3) player_count = 2;

    } else {
        // [优化2] 改进的出生点生成逻辑
        log_debug("Entering Map Gen Loop");
        while(true) {
            log_debug("Map Gen Attempt Start");
            memset(land, 0, sizeof(land)); 
            
            // 设定最小安全距离 (Min safe distance)
            // 根据玩家数量动态调整：玩家越多，允许距离越近，防止生成失败
            int division_factor = 3;
            if (player_count >= 5) division_factor = 4;
            if (player_count >= 7) division_factor = 5;
            
            int min_dist = (matrix_width + matrix_height) / division_factor;

            // P1 Base
            initialx = rand() % matrix_width + 1;
            initialy = rand() % matrix_height + 1;
            posx = initialx; posy = initialy;
            p1_opx = initialx; p1_opy = initialy;
            land[posy][posx].type = 3; land[posy][posx].state = 1; land[posy][posx].army = 1;

            log_debug("P1 Placed: %d,%d", posx, posy);
            
            // P2 Base
            int attempts = 0;
            int x2, y2;
            bool p2_ok = false;
            while(attempts++ < 1000) {
                x2 = rand() % matrix_width + 1;
                y2 = rand() % matrix_height + 1;
                // 距离检测: 必须离 P1 足够远
                if(dist(x2, y2, posx, posy) < min_dist) continue;
                if(land[y2][x2].type == 0) { p2_ok=true; break; }
            }
            if (!p2_ok) { log_debug("P2 Place Failed"); continue; }
            land[y2][x2].type = 3; land[y2][x2].state = 2; p2_opx = x2; p2_opy = y2; land[y2][x2].army = 1;
            log_debug("P2 Placed: %d,%d", x2, y2);
            
            // P3 Base
            if (player_count >= 3) {
                attempts = 0;
                int x3, y3;
                bool p3_ok = false;
                while(attempts++ < 1000) {
                    x3 = rand() % matrix_width + 1;
                    y3 = rand() % matrix_height + 1;
                    // 距离检测: 必须离 P1 和 P2 都足够远
                    if(dist(x3, y3, posx, posy) < min_dist) continue;
                    if(dist(x3, y3, x2, y2) < min_dist) continue;
                    if(land[y3][x3].type == 0) { p3_ok = true; break; }
                }
                if (!p3_ok) { log_debug("P3 Place Failed"); continue; }
                land[y3][x3].type = 3; land[y3][x3].state = 3; p3_opx = x3; p3_opy = y3; land[y3][x3].army = 1;
                log_debug("P3 Placed: %d,%d", x3, y3);
            }

            // P4 Base
            if (player_count >= 4) {
                attempts = 0;
                int x4, y4;
                bool p4_ok = false;
                while(attempts++ < 1000) {
                    x4 = rand() % matrix_width + 1;
                    y4 = rand() % matrix_height + 1;
                    if(dist(x4, y4, posx, posy) < min_dist) continue;
                    if(dist(x4, y4, x2, y2) < min_dist) continue;
                    if(dist(x4, y4, p3_opx, p3_opy) < min_dist) continue;
                    if(land[y4][x4].type == 0) { p4_ok = true; break; }
                }
                if (!p4_ok) { log_debug("P4 Place Failed"); continue; }
                land[y4][x4].type = 3; land[y4][x4].state = 4; p4_opx = x4; p4_opy = y4; land[y4][x4].army = 1;
                log_debug("P4 Placed: %d,%d", x4, y4);
            }
            
            // P5 Base
            if (player_count >= 5) {
                attempts = 0; int x5, y5; bool p5_ok = false;
                while(attempts++ < 1000) {
                    x5 = rand() % matrix_width + 1; y5 = rand() % matrix_height + 1;
                    if(dist(x5, y5, posx, posy) < min_dist) continue;
                    if(dist(x5, y5, x2, y2) < min_dist) continue;
                    if(dist(x5, y5, p3_opx, p3_opy) < min_dist) continue;
                    if(dist(x5, y5, p4_opx, p4_opy) < min_dist) continue;
                    if(land[y5][x5].type == 0) { p5_ok = true; break; }
                }
                if (!p5_ok) { log_debug("P5 Place Failed"); continue; }
                land[y5][x5].type = 3; land[y5][x5].state = 5; p5_opx = x5; p5_opy = y5; land[y5][x5].army = 1;
            }
            // P6 Base
            if (player_count >= 6) {
                attempts = 0; int x6, y6; bool p6_ok = false;
                while(attempts++ < 1000) {
                    x6 = rand() % matrix_width + 1; y6 = rand() % matrix_height + 1;
                    if(dist(x6, y6, posx, posy) < min_dist) continue;
                    if(dist(x6, y6, x2, y2) < min_dist) continue;
                    if(dist(x6, y6, p3_opx, p3_opy) < min_dist) continue;
                    if(dist(x6, y6, p4_opx, p4_opy) < min_dist) continue;
                    if(dist(x6, y6, p5_opx, p5_opy) < min_dist) continue;
                    if(land[y6][x6].type == 0) { p6_ok = true; break; }
                }
                if (!p6_ok) { log_debug("P6 Place Failed"); continue; }
                land[y6][x6].type = 3; land[y6][x6].state = 6; p6_opx = x6; p6_opy = y6; land[y6][x6].army = 1;
            }
            // P7 Base
            if (player_count >= 7) {
                attempts = 0; int x7, y7; bool p7_ok = false;
                while(attempts++ < 1000) {
                    x7 = rand() % matrix_width + 1; y7 = rand() % matrix_height + 1;
                    if(dist(x7, y7, posx, posy) < min_dist) continue;
                    if(dist(x7, y7, x2, y2) < min_dist) continue;
                    if(dist(x7, y7, p3_opx, p3_opy) < min_dist) continue;
                    if(dist(x7, y7, p4_opx, p4_opy) < min_dist) continue;
                    if(dist(x7, y7, p5_opx, p5_opy) < min_dist) continue;
                    if(dist(x7, y7, p6_opx, p6_opy) < min_dist) continue;
                    if(land[y7][x7].type == 0) { p7_ok = true; break; }
                }
                if (!p7_ok) { log_debug("P7 Place Failed"); continue; }
                land[y7][x7].type = 3; land[y7][x7].state = 7; p7_opx = x7; p7_opy = y7; land[y7][x7].army = 1;
            }
            // P8 Base
            if (player_count >= 8) {
                attempts = 0; int x8, y8; bool p8_ok = false;
                while(attempts++ < 1000) {
                    x8 = rand() % matrix_width + 1; y8 = rand() % matrix_height + 1;
                    if(dist(x8, y8, posx, posy) < min_dist) continue;
                    if(dist(x8, y8, x2, y2) < min_dist) continue;
                    if(dist(x8, y8, p3_opx, p3_opy) < min_dist) continue;
                    if(dist(x8, y8, p4_opx, p4_opy) < min_dist) continue;
                    if(dist(x8, y8, p5_opx, p5_opy) < min_dist) continue;
                    if(dist(x8, y8, p6_opx, p6_opy) < min_dist) continue;
                    if(dist(x8, y8, p7_opx, p7_opy) < min_dist) continue;
                    if(land[y8][x8].type == 0) { p8_ok = true; break; }
                }
                if (!p8_ok) { log_debug("P8 Place Failed"); continue; }
                land[y8][x8].type = 3; land[y8][x8].state = 8; p8_opx = x8; p8_opy = y8; land[y8][x8].army = 1;
            }
            
            log_debug("Generating Terrain...");
            generate(2, tower_sum); 
            generate(1, mount_sum); 
            log_debug("Checking Connectivity...");
            if(checkConnectivity(posx, posy)) {
                log_debug("Map Gen Success");
                break; 
            }
            log_debug("Connectivity Failed - Retrying");
        }
        
        if (is_lan_mode && is_server) {
            log_debug("Starting Broadcast Sequence");
            draw_background();
            outtextxy(100, 100, _T("Map Generated. Broadcasting..."));
            TCHAR debug_info[128];
            _stprintf(debug_info, _T("Clients: %d | Map Size: %d x %d"), (int)client_sockets.size(), matrix_width, matrix_height);
            outtextxy(100, 130, debug_info);
            FlushBatchDraw();
            Sleep(500); // Give time to read
            
            // Check socket validity
            for(size_t i=0; i<client_sockets.size(); i++) {
                if (client_sockets[i] == INVALID_SOCKET) {
                     outtextxy(100, 160 + i*30, _T("Invalid Socket Detected!"));
                }
            }
            FlushBatchDraw();
            
            std::vector<int> mapData = serialize_map();
            outtextxy(100, 160, _T("Serialized. Sending..."));
            FlushBatchDraw();
            
            broadcast_vect(mapData); 
            
            outtextxy(100, 190, _T("Broadcast Done. Starting..."));
            FlushBatchDraw();
            Sleep(1000); 
        }
    }

    oprate_head[1] = 1; bo_opratequeue[1] = false;
    oprate_head[2] = 1; bo_opratequeue[2] = false;
    oprate_head[3] = 1; bo_opratequeue[3] = false;
    oprate_head[4] = 1; bo_opratequeue[4] = false;
    oprate_head[5] = 1; bo_opratequeue[5] = false;
    oprate_head[6] = 1; bo_opratequeue[6] = false;
    oprate_head[7] = 1; bo_opratequeue[7] = false;
    oprate_head[8] = 1; bo_opratequeue[8] = false;

    BeginBatchDraw();
    
    while (true) {
        start_tick = clock();
        
        // --- PAUSE TOGGLE ---
        if (!is_lan_mode && (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
            long now = clock();
            if (now - last_pause_toggle_time > 300) {
                is_paused = !is_paused;
                last_pause_toggle_time = now;
            }
        }

        if (!is_paused) {
             timm++;
        }

        check_and_handle_resize(); // [Optimized] 使用函数调用替代原本的逻辑

        if (!is_paused) { // GAME LOGIC START

        if (is_lan_mode) {
            if (is_server) {
                for (auto it = client_sockets.begin(); it != client_sockets.end(); ) {
                    SOCKET s = *it;
                    if (is_socket_ready(s)) {
                        bool disconnected = false;
                        std::vector<int> remoteMove = recv_vect_from_socket(s, &disconnected);
                        
                        if (disconnected) {
                            closesocket(s);
                            it = client_sockets.erase(it);
                            // Should probably notify other players or end game if fewer than 2 players?
                            // For now, just continue, maybe AI takes over?
                            // (Not implemented: AI takeover. The unit just sits there)
                            continue;
                        }

                        if (remoteMove.size() == 5) {
                            int p = remoteMove[4];
                            applyRemoteOrder(remoteMove[0], remoteMove[1], remoteMove[2], remoteMove[3], p);
                            broadcast_vect(remoteMove, s);
                        }
                    }
                    ++it;
                }
            } else {
                if (is_data_available()) {
                    bool disconnected = false;
                    std::vector<int> remoteMove = recv_vect(&disconnected);
                    
                    if (disconnected) {
                         // Server Closed Connection
                         is_game_over = true;
                         // Show message
                         drawRoundBox(window_width/2 - 200, window_height/2 - 50, 400, 100, RGB(50, 0, 0), 200);
                         drawShadowText(window_width/2 - 180, window_height/2 - 20, _T("Server Disconnected"), 30, RED);
                         FlushBatchDraw();
                         Sleep(2000);
                         
                         // Close socket and return to menu
                         if(connSocket != INVALID_SOCKET) { closesocket(connSocket); connSocket = INVALID_SOCKET; }
                         goto GAME_START;
                    }

                    if (remoteMove.size() == 5) {
                        int p = remoteMove[4];
                        applyRemoteOrder(remoteMove[0], remoteMove[1], remoteMove[2], remoteMove[3], p);
                    }
                }
            }
        }

        if (is_game_over) {
            draw_background();
            settextcolor(WHITE);
            settextstyle(40, 0, _T("Consolas"));
            TCHAR msg[100];
            _stprintf(msg, _T("Game Over! Winner: Player %d"), winner);
            outtextxy(window_width / 2 - 250, window_height / 2 - 20, msg);
            FlushBatchDraw();
            play_sound(_T("win.mp3"));
            Sleep(3000); 
            if(connSocket != INVALID_SOCKET) { closesocket(connSocket); connSocket = INVALID_SOCKET; }
            for(SOCKET s : client_sockets) closesocket(s);
            client_sockets.clear();
            goto GAME_START;
        }

        // Local Human/LAN games: 0.5s/step (interval = 4)
        // Local AI vs AI: 0.25s/step (interval = 2)
        // Local Human/LAN games: 0.5s/step (interval = 4)
        // Local AI vs AI: 0.25s/step (interval = 2)
        int current_interval = operate_interval; 
        
        bool has_human = (p1_type == 0) || (p2_type == 0) || (player_count >= 3 && p3_type == 0) || (player_count >= 4 && p4_type == 0);

        // 仅当非局域网模式且全员 AI 时加速
        if (!is_lan_mode && !has_human) {
             current_interval = 2;
        }

        if (timm % current_interval == 0) {
            std::vector<std::vector<plot>> mapSnapshot(matrix_height + 2, std::vector<plot>(matrix_width + 2));
            for(int r = 1; r <= matrix_height; ++r) 
                 for(int c = 1; c <= matrix_width; ++c) mapSnapshot[r][c] = land[r][c];

            if (!is_lan_mode || (is_lan_mode && is_server)) {
                if (p1_type != 0) {
                     // AI 0.6 (Type 7) 恢复迷雾模式
                     oprate aiMove = callAI(p1_type, matrix_height, matrix_width, getFoggedMap(1), 1);
                     if (aiMove.sx != 0) issueOrder(aiMove.sx, aiMove.sy, aiMove.dx, aiMove.dy, 1);
                }
                if (p2_type != 0) {
                     oprate aiMove = callAI(p2_type, matrix_height, matrix_width, getFoggedMap(2), 2);
                     if (aiMove.sx != 0) issueOrder(aiMove.sx, aiMove.sy, aiMove.dx, aiMove.dy, 2);
                }
                if (player_count >= 3 && p3_type != 0) {
                     oprate aiMove = callAI(p3_type, matrix_height, matrix_width, getFoggedMap(3), 3);
                     if (aiMove.sx != 0) issueOrder(aiMove.sx, aiMove.sy, aiMove.dx, aiMove.dy, 3);
                }
                if (player_count >= 4 && p4_type != 0) {
                     oprate aiMove = callAI(p4_type, matrix_height, matrix_width, getFoggedMap(4), 4);
                     if (aiMove.sx != 0) issueOrder(aiMove.sx, aiMove.sy, aiMove.dx, aiMove.dy, 4);
                }
                if (player_count >= 5 && p5_type != 0) {
                     oprate aiMove = callAI(p5_type, matrix_height, matrix_width, getFoggedMap(5), 5);
                     if (aiMove.sx != 0) issueOrder(aiMove.sx, aiMove.sy, aiMove.dx, aiMove.dy, 5);
                }
                if (player_count >= 6 && p6_type != 0) {
                     oprate aiMove = callAI(p6_type, matrix_height, matrix_width, getFoggedMap(6), 6);
                     if (aiMove.sx != 0) issueOrder(aiMove.sx, aiMove.sy, aiMove.dx, aiMove.dy, 6);
                }
                if (player_count >= 7 && p7_type != 0) {
                     oprate aiMove = callAI(p7_type, matrix_height, matrix_width, getFoggedMap(7), 7);
                     if (aiMove.sx != 0) issueOrder(aiMove.sx, aiMove.sy, aiMove.dx, aiMove.dy, 7);
                }
                if (player_count >= 8 && p8_type != 0) {
                     oprate aiMove = callAI(p8_type, matrix_height, matrix_width, getFoggedMap(8), 8);
                     if (aiMove.sx != 0) issueOrder(aiMove.sx, aiMove.sy, aiMove.dx, aiMove.dy, 8);
                }
            }

            if (bo_opratequeue[1]) resolve_move(1);
            if (bo_opratequeue[2]) resolve_move(2);
            if (player_count >= 3 && bo_opratequeue[3]) resolve_move(3);
            if (player_count >= 4 && bo_opratequeue[4]) resolve_move(4);
            if (player_count >= 5 && bo_opratequeue[5]) resolve_move(5);
            if (player_count >= 6 && bo_opratequeue[6]) resolve_move(6);
            if (player_count >= 7 && bo_opratequeue[7]) resolve_move(7);
            if (player_count >= 8 && bo_opratequeue[8]) resolve_move(8);

            towerincrease();
            // [Fix] Correct timing check: run once every (operate_interval * landinc_interval) ticks
            // timm increases by 1 every loop. operate_interval=4, landinc_interval=25 -> 100 ticks
            if (timm % (operate_interval * landinc_interval) == 0) landincrease();
            updateFog();
        }
        } // GAME LOGIC END

        bool allow_mouse = false;
        if (!is_paused) { // INPUT START
        if (is_lan_mode) {
             allow_mouse = true;
        } else {
             if (p1_type == 0 && p2_type != 0) allow_mouse = true;
             else if (p2_type == 0 && p1_type != 0) allow_mouse = true;
        }

        if (allow_mouse && GetAsyncKeyState(VK_LBUTTON) & 0x8000) { 
            GetWindowRect(window_pointer, &window_pos);
            GetCursorPos(&mouse_pos);
            int client_x = mouse_pos.x - window_pos.left - 4 - offset_x;
            int client_y = mouse_pos.y - window_pos.top - 27 - offset_y;
            int newx = floor(client_x / plot_width) + 1;
            int newy = floor(client_y / plot_height) + 1;
            
            if (newx >= 1 && newx <= matrix_width && newy >= 1 && newy <= matrix_height) {
                if (is_lan_mode) {
                    if (local_player_id == 1) { p1_opx = newx; p1_opy = newy; }
                    else if (local_player_id == 2) { p2_opx = newx; p2_opy = newy; }
                    else if (local_player_id == 3) { p3_opx = newx; p3_opy = newy; }
                } else {
                    if (p1_type == 0) { p1_opx = newx; p1_opy = newy; }
                    else if (p2_type == 0) { p2_opx = newx; p2_opy = newy; }
                    else if (p3_type == 0 && player_count >= 3) { p3_opx = newx; p3_opy = newy; }
                    else if (p4_type == 0 && player_count >= 4) { p4_opx = newx; p4_opy = newy; }
                    else if (p5_type == 0 && player_count >= 5) { p5_opx = newx; p5_opy = newy; }
                    else if (p6_type == 0 && player_count >= 6) { p6_opx = newx; p6_opy = newy; }
                    else if (p7_type == 0 && player_count >= 7) { p7_opx = newx; p7_opy = newy; }
                    else if (p8_type == 0 && player_count >= 8) { p8_opx = newx; p8_opy = newy; }
                }
            }
        }

        // --- 键盘输入处理 (优化版) ---
        
        bool p1_uses_arrows = (!is_lan_mode && p2_type != 0); 
        if (!is_lan_mode && p1_type == 0) {
            if (p1_uses_arrows) {
                if (GetAsyncKeyState(VK_LEFT) & 0x8000)  { extendqueue(-1, 0, 1); moveopratepos(1, -1, 0); }
                if (GetAsyncKeyState(VK_UP) & 0x8000)    { extendqueue(0, -1, 1); moveopratepos(1, 0, -1); }
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { extendqueue(1, 0, 1);  moveopratepos(1, 1, 0); }
                if (GetAsyncKeyState(VK_DOWN) & 0x8000)  { extendqueue(0, 1, 1);  moveopratepos(1, 0, 1); }
            } else {
                if (GetAsyncKeyState('A') & 0x8000) { extendqueue(-1, 0, 1); moveopratepos(1, -1, 0); }
                if (GetAsyncKeyState('W') & 0x8000) { extendqueue(0, -1, 1); moveopratepos(1, 0, -1); }
                if (GetAsyncKeyState('D') & 0x8000) { extendqueue(1, 0, 1);  moveopratepos(1, 1, 0); }
                if (GetAsyncKeyState('S') & 0x8000) { extendqueue(0, 1, 1);  moveopratepos(1, 0, 1); }
            }
        }
        
        if (!is_lan_mode && p2_type == 0) {
             if (GetAsyncKeyState(VK_LEFT) & 0x8000)  { extendqueue(-1, 0, 2); moveopratepos(2, -1, 0); } 
             if (GetAsyncKeyState(VK_UP) & 0x8000)    { extendqueue(0, -1, 2); moveopratepos(2, 0, -1); } 
             if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { extendqueue(1, 0, 2);  moveopratepos(2, 1, 0); }  
             if (GetAsyncKeyState(VK_DOWN) & 0x8000)  { extendqueue(0, 1, 2);  moveopratepos(2, 0, 1); }  
        }

        if (is_lan_mode) {
            DWORD now = GetTickCount();
            int pid = local_player_id;
            
            // 优化的防抖逻辑：使用更短的间隔 (INPUT_DEBOUNCE_MS = 55)
            if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
                if (now - last_input_time[pid] > INPUT_DEBOUNCE_MS || last_dir[pid] != 3) {
                    extendqueue(-1, 0, pid); 
                    moveopratepos(pid, -1, 0);
                    last_input_time[pid] = now; last_dir[pid] = 3;
                }
            }
            if (GetAsyncKeyState(VK_UP) & 0x8000) {
                if (now - last_input_time[pid] > INPUT_DEBOUNCE_MS || last_dir[pid] != 1) {
                    extendqueue(0, -1, pid);
                    moveopratepos(pid, 0, -1);
                    last_input_time[pid] = now; last_dir[pid] = 1;
                }
            }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
                if (now - last_input_time[pid] > INPUT_DEBOUNCE_MS || last_dir[pid] != 4) {
                    extendqueue(1, 0, pid);
                    moveopratepos(pid, 1, 0);
                    last_input_time[pid] = now; last_dir[pid] = 4;
                }
            }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
                if (now - last_input_time[pid] > INPUT_DEBOUNCE_MS || last_dir[pid] != 2) {
                    extendqueue(0, 1, pid);
                    moveopratepos(pid, 0, 1);
                    last_input_time[pid] = now; last_dir[pid] = 2;
                }
            }
        }
        } // INPUT END

        // 渲染部分
        draw_background(); // 每一帧都先画背景，覆盖上一帧的内容，替代cleardevice
        
        // [New] 绘制战斗区域纯黑背景
        draw_battle_area_bg();
        
        // 注意：如果 land 绘制是覆盖式的，那么背景只会在网格缝隙中显示（如果 plot 图片有透明度或有缝隙）
        // 如果 plot 图片是完全不透明的矩形，背景图只会在地图边缘（offset区域）看到。
        
        for (int a = 1; a <= matrix_height; a++) {
            for (int b = 1; b <= matrix_width; b++) {
                int px = offset_x + (b - 1) * plot_width + 1;
                int py = offset_y + (a - 1) * plot_height + 1;
                int draw_x = px + grid_gap / 2;
                int draw_y = py + grid_gap / 2;

                if (!is_visible[a][b]) {
                    putimage(draw_x, draw_y, &fog_mask);
                } else {
                    putimage(draw_x, draw_y, &landpicture[land[a][b].type][land[a][b].state]);
                }

                int cx, cy;
                if (is_lan_mode) {
                    if (local_player_id == 1) { cx = p1_opx; cy = p1_opy; }
                    else if (local_player_id == 2) { cx = p2_opx; cy = p2_opy; }
                    else if (local_player_id == 3) { cx = p3_opx; cy = p3_opy; }
                    else { cx = p4_opx; cy = p4_opy; }
                    
                    if (a == cy && b == cx) {
                        setlinecolor(WHITE);
                        setlinestyle(PS_SOLID, 3);
                        rectangle(draw_x, draw_y, draw_x + landpicture[0][0].getwidth(), draw_y + landpicture[0][0].getheight());
                    }
                } else {
                    if (p1_type == 0 && a == p1_opy && b == p1_opx) {
                        setlinecolor(RED); setlinestyle(PS_SOLID, 3);
                        rectangle(draw_x, draw_y, draw_x + landpicture[0][0].getwidth(), draw_y + landpicture[0][0].getheight());
                    }
                    if (p2_type == 0 && a == p2_opy && b == p2_opx) {
                        setlinecolor(BLUE); setlinestyle(PS_SOLID, 3);
                        rectangle(draw_x, draw_y, draw_x + landpicture[0][0].getwidth(), draw_y + landpicture[0][0].getheight());
                    }
                    if (player_count >= 3 && p3_type == 0 && a == p3_opy && b == p3_opx) {
                        setlinecolor(MAGENTA); setlinestyle(PS_SOLID, 3);
                        rectangle(draw_x, draw_y, draw_x + landpicture[0][0].getwidth(), draw_y + landpicture[0][0].getheight());
                    }
                    if (player_count >= 4 && p4_type == 0 && a == p4_opy && b == p4_opx) {
                        setlinecolor(GREEN); setlinestyle(PS_SOLID, 3);
                        rectangle(draw_x, draw_y, draw_x + landpicture[0][0].getwidth(), draw_y + landpicture[0][0].getheight());
                    }
                    if (player_count >= 5 && p5_type == 0 && a == p5_opy && b == p5_opx) {
                        setlinecolor(CYAN); setlinestyle(PS_SOLID, 3);
                        rectangle(draw_x, draw_y, draw_x + landpicture[0][0].getwidth(), draw_y + landpicture[0][0].getheight());
                    }
                    if (player_count >= 6 && p6_type == 0 && a == p6_opy && b == p6_opx) {
                        setlinecolor(YELLOW); setlinestyle(PS_SOLID, 3);
                        rectangle(draw_x, draw_y, draw_x + landpicture[0][0].getwidth(), draw_y + landpicture[0][0].getheight());
                    }
                    if (player_count >= 7 && p7_type == 0 && a == p7_opy && b == p7_opx) {
                        setlinecolor(RGB(255, 165, 0)); setlinestyle(PS_SOLID, 3);
                        rectangle(draw_x, draw_y, draw_x + landpicture[0][0].getwidth(), draw_y + landpicture[0][0].getheight());
                    }
                    if (player_count >= 8 && p8_type == 0 && a == p8_opy && b == p8_opx) {
                        setlinecolor(RGB(128, 128, 128)); setlinestyle(PS_SOLID, 3);
                        rectangle(draw_x, draw_y, draw_x + landpicture[0][0].getwidth(), draw_y + landpicture[0][0].getheight());
                    }
                }
                
                if (is_visible[a][b] && ((land[a][b].type != 1 && land[a][b].state != 0) || land[a][b].type == 2)) {
                    RECT r = { draw_x, draw_y, draw_x + landpicture[0][0].getwidth(), draw_y + landpicture[0][0].getheight() };
                    settextcolor(BLACK); 
                    settextstyle(20, 0, _T("Consolas"), 0, 0, FW_BOLD, false, false, false); 
                    TCHAR str2[32];
                    _stprintf(str2, _T("%d"), land[a][b].army);
                    drawtext(str2, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            }
        }
        
        settextcolor(WHITE);
        settextstyle(16, 0, _T("Consolas"));
        if (is_lan_mode) {
             TCHAR role[64];
             _stprintf(role, _T("LAN P%d | Clients: %d"), local_player_id, (int)client_sockets.size());
             outtextxy(5, 5, role);
        } else {
             outtextxy(5, 5, _T("Local Game"));
        }
        if (is_paused) {
            // Darken background
            drawRoundBox(0, 0, window_width, window_height, BLACK, 100);

            // Ensure mouse input is correct for UI
            update_input();

            int panel_w = 400;
            int panel_h = 300;
            int panel_x = (window_width - panel_w) / 2;
            int panel_y = (window_height - panel_h) / 2;

            drawRoundBox(panel_x, panel_y, panel_w, panel_h, RGB(40, 40, 50), 240);
            drawShadowText(panel_x + 130, panel_y + 40, _T("PAUSED"), 50, WHITE);

            int btn_w = 250;
            int btn_h = 60;
            int btn_x = panel_x + (panel_w - btn_w) / 2;

            // Buttons
            bool hoverResume = drawModernButton(btn_x, panel_y + 120, btn_w, btn_h, _T("RESUME"), false);
            bool hoverExit = drawModernButton(btn_x, panel_y + 200, btn_w, btn_h, _T("EXIT"), false);
            
            // Handle Menu Input
            bool click = mouse_lb || (GetAsyncKeyState(VK_LBUTTON) & 0x8000);
            
            if (hoverResume && click) {
                is_paused = false;
                play_sound(_T("click.wav"));
                Sleep(200);
                mouse_lb = false; // consume click
            }
            
            if (hoverExit && click) {
                is_paused = false;
                is_game_over = false; // Reset flags if needed
                play_sound(_T("click.wav"));
                Sleep(200);
                mouse_lb = false;
                
                // Cleanup and return to menu
                if(connSocket != INVALID_SOCKET) { closesocket(connSocket); connSocket = INVALID_SOCKET; }
                for(SOCKET s : client_sockets) closesocket(s);
                client_sockets.clear();
                
                goto GAME_START;
            }
        }
        
        FlushBatchDraw();

        end_time = clock();
        int wait_time = 1000 / frame - (end_time - start_tick);
        if (wait_time > 0) Sleep(wait_time);
    }
    
    EndBatchDraw();
    closegraph();
    
    return 0;
}