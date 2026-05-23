// ==================== globals.h ====================
// 全局变量声明（extern）+ 菜单文本常量
#ifndef GLOBALS_H
#define GLOBALS_H

#include "config.h"
#include <Adafruit_SH110X.h>
#include <Preferences.h>

// ==================== OLED ====================
extern Adafruit_SH1106G display;

// ==================== 配置 ====================
extern Config config;
extern Preferences prefs;

// ==================== 电费数据 ====================
extern float balance;
extern float usage;
extern bool  queryOk;
extern char  queryMsg[32];
extern unsigned long lastQuery;
extern unsigned long lastPush;

// ==================== WiFi ====================
extern bool wifiConnected;
extern bool wifiWasConnected;

// ==================== 菜单状态 ====================
extern MenuState menuState;
extern int menuIndex;
extern int menuCount;

extern int editValue;
extern int editMin;
extern int editMax;
extern char editLabel[16];

// ==================== 屏保 ====================
extern bool screensaverActive;
extern unsigned long lastActivity;
extern int screensaverPage;

#define SCREENSAVER_TIMEOUT 30000

// ==================== 菜单文本（定义在 .ino） ====================
extern const char* txtMain[];
extern const char* txtSet[];
extern const char* txtQry[];
extern const char* txtPush[];
extern const char* txtPushFreq[];
extern const char* txtPushDay[];

#endif // GLOBALS_H
