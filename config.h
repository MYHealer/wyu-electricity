// ==================== config.h ====================
// 全局配置、引脚定义、数据结构
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================== 引脚 ====================
#define PIN_SDA       4
#define PIN_SCL       5
#define PIN_ENC_A     0
#define PIN_ENC_B     1
#define PIN_ENC_BTN   6
#define PIN_BTN_OK    3
#define PIN_BTN_BACK  10
#define PIN_LED       2   // 板载LED，低电平点亮

// ==================== OLED ====================
#define OLED_WIDTH     128
#define OLED_HEIGHT    64
#define OLED_RESET     -1

// ==================== 参数 ====================
#define QUERY_INTERVAL  300000
#define API_URL         "http://202.192.240.231/scp-api/electricity-recharge/getCurrentRemaining_v2"

// ==================== 默认值 ====================
#define DEFAULT_BUILDING  46
#define DEFAULT_ROOM      416
#define SENDKEY           "SCTxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

// ==================== 菜单状态 ====================
enum MenuState {
    ST_IDLE,
    ST_MAIN, ST_SET, ST_QRY, ST_PUSH,
        ST_SET_BLD, ST_SET_ROOM, ST_SET_DEF, ST_SET_WIFI,
                    ST_PUSH_FREQ, ST_PUSH_DAY, ST_PUSH_EN, ST_ABOUT,
                                                ST_QRY_FREQ, ST_QRY_DAY,
                                
};

// ==================== 配置结构体 ====================
struct Config {
    int building;
    int room;
    bool pushEnabled;
    bool pushDaily;
        int pushDay;
    bool qryDaily;
    int qryDay;
    char sendkey[64];
};

// 菜单索引 → tm_wday：菜单 Mon=0,Tue=1,...,Sun=6；tm_wday Sun=0,Mon=1,...,Sat=6
inline int menuIdxToWday(int menuIdx) {
    return (menuIdx + 1) % 7;  // Mon(0)→1, Tue(1)→2, ..., Sun(6)→0
}

#endif // CONFIG_H
