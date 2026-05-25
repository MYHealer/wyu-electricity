/*
  * 五邑大学宿舍电费查询终端 v9.3
 * ESP32-C3 + SH1106 OLED(128x64 I2C) + 旋钮 + 按钮
 * Deep Sleep 省电：30秒无操作休眠，GPIO3/定时器唤醒
 *
 * 接线：
 *   OLED SDA  → GPIO4    旋钮 A    → GPIO0
 *   OLED SCL  → GPIO5    旋钮 B    → GPIO1
 *   OLED VCC  → 3V3      旋钮 确认  → GPIO6
 *   OLED GND  → GND      唤醒/屏保 → GPIO3
 *                          BACK      → GPIO10
 *
 * 模块划分：
 *   config.h       — 引脚、常量、结构体定义
 *   globals.h      — 全局变量 extern 声明
 *   config_store.h — NVS 配置读写
 *   sleep_manager.h — Deep Sleep 管理
 *   wifi_manager.h — WiFi 连接 + Web 配网 + 在线更新
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
#include "sleep_manager.h"  // 先包含，定义 getCurrentTime/saveNTPTime
#include "display.h"
#include "api.h"
#include "wifi_manager.h"

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
const char* txtSet[]      = {"Building", "Room", "Default", "WiFi Setup", "Back"};
const char* txtQry[]      = {"Current", "Frequency", "Back"};
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

        connectWiFiOnBoot();

    if (wifiConnected) {
        configTime(8 * 3600, 0, "ntp.aliyun.com", "cn.ntp.org.cn", "pool.ntp.org");
        struct tm ti;
        if (getLocalTime(&ti, 5000)) {
            // 验证时间合理性（年份必须 >= 2025）
            if (ti.tm_year + 1900 >= 2025) {
                saveNTPTime();  // 保存到 RTC 内存，断网时可用
            } else {
                Serial.printf("[NTP] Invalid year: %d, sync failed\n", ti.tm_year + 1900);
            }
        }
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
            Serial.println("v9.2 Timer wake - executing tasks...");

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
            // GPIO 唤醒：直接用软件 RTC 显示时间，不等 NTP
            Serial.println("v9.2 Button wake - cache + local RTC");

            // ★ deep sleep 后 configTime 丢失，必须重新设置时区
            // 否则 localtime_r() 按 UTC 算，时间会少 8 小时
            configTime(8 * 3600, 0, "ntp.aliyun.com", "cn.ntp.org.cn", "pool.ntp.org");

            loadConfig();
            loadQueryCache();

            screensaverActive = true;
            screensaverPage = 0;
            lastActivity = millis();
            showScreensaver();  // 用软件 RTC 时间，秒亮
            break;
        }

                                        default: {
            // 上电复位：加载缓存 + 快速取时间
            Serial.println("v9.2 Fresh boot - cache + quick NTP");

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

                    // ★ 配网期间：检测按钮退出 + 检查配网状态
        if (portalActive) {
        if (back) {  // ★ 只有 BACK (IO10) 退出配网
            stopWifiPortal();  // 停止配网
            // 恢复显示
            if (menuState == ST_SET) {
                showCurrentMenu();
            } else {
                showScreensaver();
            }
        } else {
            checkPortalStatus();  // 检查配网是否自动完成
            if (!portalActive) {
                // 配网自动完成，恢复显示
                if (menuState == ST_SET) {
                    showCurrentMenu();
                } else {
                    showScreensaver();
                }
            }
        }
        delay(10);
        return;
    }

                        // 检测 WiFi 刚连上：立即查询，查完关 WiFi（配网期间跳过）
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
