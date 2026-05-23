/*
 * 五邑大学宿舍电费查询终端 v8.0
 * ESP32-C3 SuperMini + SH1106 OLED(128x64 I2C) + 旋钮 + 按钮
 * Deep Sleep 省电：30秒无操作休眠，GPIO6/定时器唤醒
 *
 * 接线：
 *   OLED SDA  → GPIO4    旋钮 A    → GPIO0
 *   OLED SCL  → GPIO5    旋钮 B    → GPIO1
 *   OLED VCC  → 3V3      旋钮 按下  → GPIO6
 *   OLED GND  → GND      CONFIRM   → GPIO3
 *                          BACK      → GPIO10
 *
 * 模块划分：
 *   config.h       — 引脚、常量、结构体定义
 *   globals.h      — 全局变量 extern 声明
 *   config_store.h — NVS 配置读写
 *   sleep_manager.h — Deep Sleep 管理
 *   wifi_manager.h — WiFi 连接 + Web 配网
 *   encoder.h      — 旋转编码器 + 按钮
 *   display.h      — OLED 绘图辅助 + 屏保
 *   api.h          — 电费查询 + Server酱推送
 *   menu.h         — 菜单状态机
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

#include "globals.h"
#include "config_store.h"
#include "encoder.h"
#include "display.h"
#include "api.h"
#include "wifi_manager.h"
#include "sleep_manager.h"
#include "ota_manager.h"
#include "menu.h"

// ==================== 全局变量定义 ====================
Adafruit_SH1106G display = Adafruit_SH1106G(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

Config config;
float balance = -1;
float usage = 0;
bool  queryOk = false;
char  queryMsg[32] = "";
unsigned long lastQuery = 0;
unsigned long lastPush = 0;
bool wifiConnected = false;
bool wifiWasConnected = false;

MenuState menuState = ST_IDLE;
int menuIndex = 0;
int menuCount = 4;

int editValue = 0;
int editMin = 0;
int editMax = 99;
char editLabel[16] = "";

bool screensaverActive = false;
unsigned long lastActivity = 0;
bool configDirty = false;
int screensaverPage = 0;

// ==================== 菜单文本 ====================
const char* txtMain[]     = {"Query", "Settings", "Push", "About"};
const char* txtSet[]      = {"Building", "Room", "Default", "WiFi Setup", "OTA Update", "Back"};
const char* txtQry[]      = {"Current", "Frequency", "Hour", "Back"};
const char* txtPush[]     = {"Trigger", "Frequency", "Push Now", "Back"};
const char* txtPushFreq[] = {"Daily", "Weekly"};
const char* txtPushDay[]  = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

// ==================== 公共初始化 ====================
void initHardware() {
        // 板载LED（GPIO2，低电平点亮）- 闪一下表示启动
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);   // 亮
    delay(100);
    digitalWrite(PIN_LED, HIGH);  // 灭，省电

    // 按钮引脚
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    pinMode(PIN_ENC_BTN, INPUT_PULLUP);
    pinMode(PIN_BTN_OK, INPUT_PULLUP);
    pinMode(PIN_BTN_BACK, INPUT_PULLUP);

    // 编码器中断
    encoderSetup();

    // OLED 初始化
    Wire.begin(PIN_SDA, PIN_SCL);
    delay(250);
    display.begin(0x3C, true);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.display();
}

void initWiFiAndNTP() {
    loadConfig();
    Serial.printf("Room: %d-%d\n", config.building, config.room);

    connectWiFi();

        if (wifiConnected) {
        configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
        struct tm ti;
        getLocalTime(&ti, 3000);  // 等 NTP 同步完成
        digitalWrite(PIN_LED, HIGH);  // 连上WiFi，关LED
    }
}

// ==================== setup ====================
void setup() {
    Serial.begin(115200);

    // 检查唤醒原因（必须最先执行）
    wakeReason = checkWakeReason();

    initHardware();

    // ===== 按唤醒原因分支 =====
    switch (wakeReason) {

                                case WAKE_TIMER: {
            // 定时器唤醒：查询/推送，关 WiFi，然后睡
            Serial.println("v9.1 Timer wake - executing tasks...");

            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SH110X_WHITE);
            drawStrCenter(20, "Timer Wake");
            drawStrCenter(32, "Updating...");
            display.display();

            // 必须先加载配置，否则 pushEnabled 等参数都是默认值
            loadConfig();
            loadQueryCache();

            initWiFiAndNTP();

            if (wifiConnected) {
                executeScheduledTasks();
                shutdownWiFi();  // 查完关 WiFi 省电
            }

            Serial.println("[Sleep] Tasks done, sleeping again...");
            enterDeepSleep();
            break;
        }

                                        case WAKE_GPIO: {
            // GPIO 唤醒：先显示缓存，再快速取 NTP 时间
            Serial.println("v9.1 Button wake - cache + quick NTP");

            loadConfig();
            loadQueryCache();

            screensaverActive = true;
            screensaverPage = 0;
            lastActivity = millis();
            showScreensaver();

            // 快速连 WiFi 取时间（~5秒），取完立刻关
            quickNTPSync();
            showScreensaver();  // 用取到的时间刷新显示
            break;
        }

                                        default: {
            // 上电复位：加载缓存 + 快速取时间
            Serial.println("v9.1 Fresh boot - cache + quick NTP");

            loadConfig();
            loadQueryCache();

            screensaverActive = true;
            screensaverPage = 0;
            lastActivity = millis();
            showScreensaver();

            quickNTPSync();
            showScreensaver();

            lastPush = millis();
            showCurrentMenu();
            break;
        }
    }
}

// ==================== loop ====================
void loop() {
    int delta = readEncoder();
    bool encBtn = readBtn(PIN_ENC_BTN);
    bool ok = readBtn(PIN_BTN_OK);
    bool back = readBtn(PIN_BTN_BACK);

            // 检测 WiFi 刚连上：立即查询，查完关 WiFi
    if (wifiConnected && !wifiWasConnected) {
        Serial.println("[WiFi] Just connected, querying...");
        if (queryElectricity()) {
            queryOk = true;
            saveQueryCache();
            showCurrentMenu();
        }
        lastQuery = millis();
        // 查询完毕，关 WiFi 省电
        shutdownWiFi();
    }
    wifiWasConnected = wifiConnected;

        bool anyInput = (delta != 0 || encBtn || ok || back);

    // 有操作 → 刷新计时
    if (anyInput) {
        lastActivity = millis();
    }

    // ===== 30秒无操作 → Deep Sleep =====
    if (millis() - lastActivity > SCREENSAVER_TIMEOUT) {
        Serial.println("[Sleep] 30s idle, going to sleep...");
        enterDeepSleep();
    }

    // 屏保模式：旋钮/按钮操作
    if (screensaverActive) {
        if (delta != 0 || back || encBtn) {
            screensaverActive = false;
            menuState = ST_MAIN;
            menuIndex = 0;
            menuCount = 4;
            lastActivity = millis();
            showCurrentMenu();
        } else if (ok) {
            screensaverPage = (screensaverPage + 1) % 2;
            lastActivity = millis();
            showScreensaver();
        }
        delay(10);
        return;
    }

        // IDLE 界面
    if (menuState == ST_IDLE) {
        if (delta != 0 || encBtn) {
            menuState = ST_MAIN;
            menuIndex = 0;
            menuCount = 4;
            lastActivity = millis();
            showCurrentMenu();
        }
        delay(10);
        return;
    }

                                // OTA 模式：处理 Web 上传（保持活跃，不进 sleep）
    if (menuState == ST_SET_OTA) {
        handleOtaLoop();
        lastActivity = millis();  // OTA 期间禁止 sleep
        // 允许返回键退出 OTA
        if (back) {
            handleBack();
        }
        delay(2);
        return;
    }

                                // WiFi 配网已改为阻塞式，无需 loop 处理

    // 编辑模式：旋钮调值
        if (menuState == ST_SET_BLD || menuState == ST_SET_ROOM ||
        menuState == ST_PUSH_EN) {
        if (delta != 0) {
            editValue += delta;
            if (editValue < editMin) editValue = editMin;
            if (editValue > editMax) editValue = editMax;
            showCurrentMenu();
        }
        if (encBtn || ok) handleOk();
        if (back) handleBack();
        delay(10);
        return;
    }

    // 菜单模式：旋钮选择
    if (delta != 0) {
        menuIndex += delta;
        if (menuIndex < 0) menuIndex = menuCount - 1;
        if (menuIndex >= menuCount) menuIndex = 0;
        showCurrentMenu();
        lastActivity = millis();
    }

    if (ok) {
        screensaverActive = true;
        lastActivity = millis();
        showScreensaver();
        delay(10);
        return;
    }

                if (encBtn) {
        handleOk();
        // Push Now 等待数秒，残留按钮状态需清除
        readBtn(PIN_ENC_BTN);
        readBtn(PIN_BTN_OK);
        readBtn(PIN_BTN_BACK);
    }
    if (back) handleBack();

    delay(10);
}
