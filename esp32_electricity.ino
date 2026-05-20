/*
 * 五邑大学宿舍电费查询终端 v6.0
 * ESP32-C3 SuperMini + SH1106 OLED(128x64 I2C) + 旋钮 + 按钮
 * WiFi 直连，无配网功能
 *
 * 接线：
 *   OLED SDA  → GPIO4    旋钮 A    → GPIO0
 *   OLED SCL  → GPIO5    旋钮 B    → GPIO1
 *   OLED VCC  → 3V3      旋钮 按下  → GPIO6
 *   OLED GND  → GND      CONFIRM   → GPIO3
 *                          BACK      → GPIO10
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include "esp_wifi.h"
#include "config_portal.h"

// ==================== 引脚 ====================
#define PIN_SDA       4
#define PIN_SCL       5
#define PIN_ENC_A     0
#define PIN_ENC_B     1
#define PIN_ENC_BTN   6
#define PIN_BTN_OK    3
#define PIN_BTN_BACK  10

// ==================== OLED ====================
#define OLED_WIDTH     128
#define OLED_HEIGHT    64
#define OLED_RESET     -1
Adafruit_SH1106G display = Adafruit_SH1106G(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// ==================== 参数 ====================
#define QUERY_INTERVAL  300000
#define API_URL         "http://202.192.240.231/scp-api/electricity-recharge/getCurrentRemaining_v2"

// ==================== 菜单状态 ====================
enum MenuState {
    ST_IDLE,
    ST_MAIN, ST_SET, ST_QRY, ST_PUSH,
        ST_SET_BLD, ST_SET_ROOM, ST_SET_DEF, ST_SET_WIFI,
            ST_PUSH_FREQ, ST_PUSH_HOUR, ST_PUSH_DAY, ST_PUSH_EN, ST_ABOUT,
                        ST_QRY_FREQ, ST_QRY_HOUR, ST_QRY_DAY
};

const char* txtMain[] = {"Query", "Settings", "Push", "About"};
const char* txtSet[]  = {"Building", "Room", "Default", "Back"};
const char* txtQry[]  = {"Current", "Frequency", "Hour", "Back"};
const char* txtPush[]    = {"Trigger", "Frequency", "Hour", "Push Now", "Back"};
const char* txtPushFreq[] = {"Daily", "Weekly"};
const char* txtPushDay[]  = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

// ==================== 全局状态 ====================
struct Config {
    int building;
    int room;
    bool pushEnabled;
    bool pushDaily;
    int pushHour;
    int pushDay;
    bool qryDaily;
    int qryHour;
    int qryDay;
    char sendkey[64];
};
Config config;

float balance = -1;
float usage = 0;
bool  queryOk = false;
char  queryMsg[32] = "";
unsigned long lastQuery = 0;
unsigned long lastPush = 0;
int lastPushHour = -1;  // 记录上次推送的小时，防止同一小时重复推送
bool wifiConnected = false;
bool wifiWasConnected = false;
unsigned long lastWifiCheck = 0;

MenuState menuState = ST_IDLE;
int menuIndex = 0;
int menuCount = 4;

int editValue = 0;
int editMin = 0;
int editMax = 99;
char editLabel[16] = "";

bool screensaverActive = false;
bool displayOff = false;       // 息屏状态
unsigned long lastActivity = 0;
#define SCREENSAVER_TIMEOUT 30000  // 30s无操作息屏
bool configDirty = false;
int screensaverPage = 0;  // 0=电费页面, 1=时间页面

Preferences prefs;

// ==================== WiFi ====================
unsigned long wifiLastReconnect = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 15000;

void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("[WiFi] AP connected");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("[WiFi] Disconnected");
            wifiConnected = false;
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("[WiFi] IP: %s  RSSI: %d\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            wifiConnected = true;
            break;
        default: break;
    }
}

void connectWiFi() {
    Serial.printf("[WiFi] Connecting to: %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.onEvent(WiFiEvent);

    // ESP32-C3 SuperMini：降低发射功率防止信号失真
    esp_wifi_set_max_tx_power(34);  // 8.5dBm

    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // 等待最多15秒
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }

        if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("\n[WiFi] OK! IP: %s\n", WiFi.localIP().toString().c_str());
        // NTP 时间同步
        configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
        Serial.println("[NTP] Syncing...");
    } else {
        wifiConnected = false;
        Serial.println("\n[WiFi] Failed! Will retry in loop...");
    }
    wifiLastReconnect = millis();
}

// 每15秒检查，断了就重连
void checkWiFi() {
    if (wifiConnected) return;
    if (millis() - wifiLastReconnect < WIFI_RECONNECT_INTERVAL) return;

    wifiLastReconnect = millis();
    Serial.println("[WiFi] Reconnecting...");

    WiFi.disconnect();
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // 等待最多10秒
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
        Serial.print(".");
    }

        if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("\n[WiFi] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
        configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
    } else {
        Serial.println("\n[WiFi] Reconnect failed, will retry...");
    }
}

// ==================== 配置读写 ====================
void loadConfig() {
    prefs.begin("elec", true);
        config.building = prefs.getInt("bld", DEFAULT_BUILDING);
    config.room = prefs.getInt("room", DEFAULT_ROOM);
    config.pushEnabled = prefs.getBool("push", false);
    config.pushDaily = prefs.getBool("pushD", true);
    config.pushHour = prefs.getInt("pushH", 8);
        config.pushDay = prefs.getInt("pushDy", 1);
    config.qryDaily = prefs.getBool("qryD", true);
    config.qryHour = prefs.getInt("qryH", 8);
    config.qryDay = prefs.getInt("qryDy", 1);
    strncpy(config.sendkey, prefs.getString("sendkey", SENDKEY).c_str(), sizeof(config.sendkey) - 1);
    // 如果 NVS 里是空的，用硬编码默认值
    if (strlen(config.sendkey) == 0) {
        strncpy(config.sendkey, SENDKEY, sizeof(config.sendkey) - 1);
    }
    prefs.end();
}

void saveConfig() {
    prefs.begin("elec", false);
    prefs.putInt("bld", config.building);
    prefs.putInt("room", config.room);
    prefs.putBool("push", config.pushEnabled);
    prefs.putBool("pushD", config.pushDaily);
    prefs.putInt("pushH", config.pushHour);
        prefs.putInt("pushDy", config.pushDay);
    prefs.putBool("qryD", config.qryDaily);
    prefs.putInt("qryH", config.qryHour);
    prefs.putInt("qryDy", config.qryDay);
    prefs.putString("sendkey", config.sendkey);
    prefs.end();
}

// ==================== 编码器 ====================
volatile int encDelta = 0;
void IRAM_ATTR encoderISR() {
    static uint8_t last = 0;
    uint8_t a = digitalRead(PIN_ENC_A);
    uint8_t b = digitalRead(PIN_ENC_B);
    uint8_t ab = (a << 1) | b;
    uint8_t combined = (last << 2) | ab;
    static const int8_t table[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    encDelta += table[combined];
    last = ab;
}

int readEncoder() {
    int d = encDelta;
    encDelta = 0;
    static int accumulator = 0;
    accumulator += d;
    if (accumulator >= 4) { accumulator = 0; return 1; }
    if (accumulator <= -4) { accumulator = 0; return -1; }
    return 0;
}

// ==================== 按钮 ====================
bool readBtn(uint8_t pin) {
    static uint8_t lastState[11] = {1,1,1,1,1,1,1,1,1,1,1};
    uint8_t state = digitalRead(pin);
    bool pressed = (lastState[pin] == 1 && state == 0);
    lastState[pin] = state;
    return pressed;
}

// ==================== OLED绘图辅助 ====================
void drawStr(int x, int y, const char* str) {
    display.setCursor(x, y);
    display.print(str);
}

int strWidth(const char* str) {
    return strlen(str) * 6;
}

void drawStrCenter(int y, const char* str) {
    int w = strWidth(str);
    drawStr((128 - w) / 2, y, str);
}

void drawTitle(const char* title) {
    display.fillRect(0, 0, 128, 16, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    display.setTextSize(1);
    drawStrCenter(5, title);
    display.setTextColor(SH110X_WHITE);
}

void drawMenuItem(int y, const char* text, bool selected) {
    if (selected) {
        display.fillRect(2, y - 8, 124, 10, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
        int w = strWidth(text);
        drawStr((128 - w) / 2, y - 7, text);
        display.setTextColor(SH110X_WHITE);
    } else {
        drawStr(8, y - 7, text);
    }
}

// 绘制滚动菜单：选中项始终居中，其他项上下滚动
void drawScrollMenu(const char* items[], int count) {
    const int centerY = 36;  // 选中项的固定Y位置（菜单区域中心）
    const int itemH = 10;    // 每项高度
    const int topLimit = 20; // 菜单区域上边界
    const int botLimit = 54; // 菜单区域下边界

    for (int i = 0; i < count; i++) {
        int y = centerY + (i - menuIndex) * itemH;
        if (y < topLimit - 4 || y > botLimit + 4) continue; // 超出可见区域，跳过
        drawMenuItem(y, items[i], i == menuIndex);
    }
}

void drawStatusBar() {
    if (wifiConnected) {
                drawStr(104, 57, "WiFi");
    } else {
        drawStr(96, 57, "NoWiFi");
    }
    if (menuState != ST_IDLE) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d-%d", config.building, config.room);
        drawStr(0, 57, buf);
    }
}

// ==================== 屏保 ====================
void showScreensaver() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    if (screensaverPage == 0) {
        // 电费页面
        // 第一行：楼栋-房间号（大字）
        char roomBuf[16];
        snprintf(roomBuf, sizeof(roomBuf), "%02d-%03d", config.building, config.room);
        display.setTextSize(2);
        int rw = strlen(roomBuf) * 12;
        display.setCursor((128 - rw) / 2, 0);
        display.print(roomBuf);

        // 分隔线
        display.drawLine(0, 18, 127, 18, SH110X_WHITE);

        // 第二行：Balance:
        display.setTextSize(1);
        display.setCursor(0, 22);
        display.print("Balance:");

        if (queryOk) {
            // 第三行：余额（大字）+ Yuan
            char balBuf[16];
            snprintf(balBuf, sizeof(balBuf), "%.2f", balance);
            display.setTextSize(2);
            display.setCursor(0, 32);
            display.print(balBuf);
            display.setTextSize(1);
            display.setCursor(display.getCursorX() + 2, 40);
            display.print("Yuan");

            // 第四行：Used: xxx kWh
            display.setCursor(0, 56);
            display.printf("Used: %.1f kWh", usage);
        } else {
            display.setTextSize(1);
            display.setCursor(0, 36);
            display.print("No Data");
        }
    } else {
        // 时间页面
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 100)) {
            // 日期
            char dateBuf[20];
            snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
            display.setTextSize(1);
            drawStrCenter(8, dateBuf);

            // 星期
            const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            drawStrCenter(20, weekdays[timeinfo.tm_wday]);

                                    // 时间（大字，不含秒）
            char timeBuf[16];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d",
                     timeinfo.tm_hour, timeinfo.tm_min);
            display.setTextSize(2);
            int tw = strlen(timeBuf) * 12;
            display.setCursor((128 - tw) / 2, 36);
            display.print(timeBuf);
        } else {
            display.setTextSize(1);
            drawStrCenter(20, "No NTP");
            drawStrCenter(32, "WiFi:");
            display.print(wifiConnected ? "OK" : "--");
        }
    }

    display.display();
}

// ==================== 电费查询 ====================
bool queryElectricity() {
    if (!wifiConnected) {
        strcpy(queryMsg, "No WiFi");
        return false;
    }

    HTTPClient http;
    http.begin(API_URL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("User-Agent", "Mozilla/5.0");
    http.addHeader("Referer", "http://202.192.240.231/recharge.html");

    // 表单格式：userTypeID=1&building=46&room=416
    char payload[128];
    snprintf(payload, sizeof(payload),
             "userTypeID=1&building=%d&room=%d",
             config.building, config.room);

    Serial.printf("[API] Query: %s\n", payload);
    int code = http.POST((uint8_t*)payload, strlen(payload));

    if (code != 200) {
        snprintf(queryMsg, sizeof(queryMsg), "HTTP %d", code);
        http.end();
        return false;
    }

    String resp = http.getString();
    http.end();
    Serial.printf("[API] Resp: %s\n", resp.c_str());

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, resp)) {
        strcpy(queryMsg, "JSON Error");
        return false;
    }

        // 返回字段在 data 对象内：resamp=余额，usedamp=累计用电
    if (doc.containsKey("data")) {
        balance = doc["data"]["resamp"].as<float>();
        usage = doc["data"]["usedamp"].as<float>();
    } else {
        strcpy(queryMsg, "Parse Error");
        return false;
    }

    snprintf(queryMsg, sizeof(queryMsg), "%.2f", balance);
    Serial.printf("[API] Balance: %.2f  Usage: %.1f\n", balance, usage);
    return true;
}

// ==================== Server酱推送 ====================
bool pushNotification() {
    if (!wifiConnected) return false;
    if (strlen(config.sendkey) == 0) return false;

    HTTPClient http;
    String url = "https://sctapi.ftqq.com/" + String(config.sendkey) + ".send";
    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    char title[64];
    snprintf(title, sizeof(title), "电费%02d-%03d", config.building, config.room);
    char body[128];
        snprintf(body, sizeof(body), "title=%s&desp=余额: %.2f 元\n用量: %.1f 度",
             title, balance, usage);

    int code = http.POST((uint8_t*)body, strlen(body));
    http.end();

    Serial.printf("[Push] Code: %d\n", code);
    return code == 200;
}

// ==================== 显示菜单 ====================
void showCurrentMenu() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    switch (menuState) {
        case ST_IDLE:
            showScreensaver();
            return;

                case ST_MAIN:
            drawTitle("Menu");
            drawScrollMenu(txtMain, menuCount);
            break;

        case ST_SET:
            drawTitle("Settings");
            drawScrollMenu(txtSet, menuCount);
            break;

        case ST_QRY:
            drawTitle("Query");
            drawScrollMenu(txtQry, menuCount);
            break;

        case ST_PUSH:
            drawTitle("Push");
            drawScrollMenu(txtPush, menuCount);
            break;

                                case ST_SET_BLD:
        case ST_SET_ROOM:
        case ST_PUSH_HOUR:
        case ST_QRY_HOUR:
            drawTitle(editLabel);
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "< %d >", editValue);
                drawStrCenter(30, buf);
                            }
            break;
        case ST_PUSH_EN:
            drawTitle(editLabel);
            drawStrCenter(30, editValue ? "< ON >" : "< OFF >");
            break;

        case ST_ABOUT:
            drawTitle("About");
            drawStrCenter(22, "Elec Terminal v6.0");
            drawStrCenter(34, "ESP32-C3 + SH1106");
            drawStrCenter(46, "WYU HCJ");
            break;

        case ST_SET_DEF:
            drawTitle("Default");
            drawStrCenter(22, "Reset to 46-416?");
            drawStrCenter(38, "OK=Yes  Back=No");
            break;

                        case ST_PUSH_FREQ:
            drawTitle("Frequency");
            drawScrollMenu(txtPushFreq, 2);
            break;

        case ST_PUSH_DAY:
            drawTitle("Push Day");
            drawScrollMenu(txtPushDay, 7);
            break;

        case ST_QRY_FREQ:
            drawTitle("Frequency");
            drawScrollMenu(txtPushFreq, 2);
            break;

        case ST_QRY_DAY:
            drawTitle("Query Day");
            drawScrollMenu(txtPushDay, 7);
            break;

        case ST_SET_WIFI:
            drawTitle("WiFi");
            drawStrCenter(22, WIFI_SSID);
            if (wifiConnected) {
                drawStrCenter(38, "Connected");
            } else {
                drawStrCenter(38, "Disconnected");
            }
            drawStrCenter(50, "Back to exit");
            break;
    }

    drawStatusBar();
    display.display();
}

// ==================== 菜单操作 ====================
void handleOk() {
    lastActivity = millis();

        switch (menuState) {
        case ST_IDLE:
            // 切换屏保页面
            screensaverPage = (screensaverPage + 1) % 2;
            break;

        case ST_MAIN:
            switch (menuIndex) {
                                                                case 0: // Query
                    menuState = ST_QRY;
                    menuIndex = 0;
                    menuCount = 4;
                    break;
                case 1: // Settings
                    menuState = ST_SET;
                    menuIndex = 0;
                    menuCount = 4;
                    break;
                case 2: // Push
                    menuState = ST_PUSH;
                    menuIndex = 0;
                    menuCount = 5;
                    break;
                case 3: // About
                    menuState = ST_ABOUT;
                    break;
            }
            break;

        case ST_SET:
            switch (menuIndex) {
                case 0: // Building
                    menuState = ST_SET_BLD;
                    editValue = config.building;
                    editMin = 1; editMax = 99;
                    strcpy(editLabel, "Building");
                    break;
                case 1: // Room
                    menuState = ST_SET_ROOM;
                    editValue = config.room;
                    editMin = 1; editMax = 999;
                    strcpy(editLabel, "Room");
                    break;
                case 2: // Default
                    menuState = ST_SET_DEF;
                    break;
                case 3: // Back
                    menuState = ST_MAIN;
                    menuIndex = 1;
                    menuCount = 4;
                    break;
            }
            break;

                                case ST_QRY:
            switch (menuIndex) {
                case 0: // Current
                    if (queryElectricity()) queryOk = true;
                    menuState = ST_MAIN;
                    menuIndex = 0;
                    menuCount = 4;
                    break;
                case 1: // Frequency
                    menuState = ST_QRY_FREQ;
                    menuIndex = config.qryDaily ? 0 : 1;
                    menuCount = 2;
                    break;
                                case 2: // Hour
                                        strcpy(editLabel, "Hour");
                    editValue = config.qryHour;
                    editMin = 0;
                    editMax = 23;
                    menuState = ST_QRY_HOUR;
                    break;
                case 3: // Back
                    menuState = ST_MAIN;
                    menuIndex = 0;
                    menuCount = 4;
                    break;
            }
            break;

                case ST_PUSH:
            switch (menuIndex) {
                                case 0: // Enable
                    menuState = ST_PUSH_EN;
                    editValue = config.pushEnabled ? 1 : 0;
                    editMin = 0; editMax = 1;
                                        strcpy(editLabel, "Trigger");
                    break;
                case 1: // Frequency
                    menuState = ST_PUSH_FREQ;
                    menuIndex = config.pushDaily ? 0 : 1;
                    break;
                case 2: // Hour
                    menuState = ST_PUSH_HOUR;
                    editValue = config.pushHour;
                    editMin = 0; editMax = 23;
                    strcpy(editLabel, "Hour");
                    break;
                case 3: // Push Now
                    if (queryOk) {
                        pushNotification();
                        strcpy(queryMsg, "Pushed!");
                    } else {
                        strcpy(queryMsg, "No Data");
                    }
                    break;
                case 4: // Back
                    menuState = ST_MAIN;
                    menuIndex = 2;
                    menuCount = 4;
                    break;
            }
            break;

        case ST_SET_BLD:
            config.building = editValue;
            saveConfig();
            menuState = ST_SET;
            menuIndex = 0;
            menuCount = 4;
            break;

        case ST_SET_ROOM:
            config.room = editValue;
            saveConfig();
            menuState = ST_SET;
            menuIndex = 1;
            menuCount = 4;
            break;

        case ST_SET_DEF:
            config.building = 46;
            config.room = 416;
            saveConfig();
            menuState = ST_SET;
            menuIndex = 2;
            menuCount = 4;
            break;

        case ST_SET_WIFI:
            menuState = ST_SET;
            menuIndex = 3;
            menuCount = 4;
            break;

                        case ST_PUSH_FREQ:
            config.pushDaily = (menuIndex == 0);
            saveConfig();
            menuState = ST_PUSH;
            menuIndex = 1;
            menuCount = 5;
            break;

        case ST_PUSH_EN:
            config.pushEnabled = (editValue == 1);
            saveConfig();
            menuState = ST_PUSH;
            menuIndex = 0;
            menuCount = 5;
            break;

        case ST_PUSH_HOUR:
            config.pushHour = editValue;
            saveConfig();
            menuState = ST_PUSH;
            menuIndex = 2;
            menuCount = 5;
            break;

                case ST_PUSH_DAY:
            config.pushDay = menuIndex;
            saveConfig();
            menuState = ST_PUSH;
            menuIndex = 2;
            menuCount = 5;
            break;

                case ST_QRY_FREQ:
            config.qryDaily = (menuIndex == 0);
            saveConfig();
            menuState = ST_QRY;
            menuIndex = 1;
            menuCount = 4;
            break;

        case ST_QRY_HOUR:
            config.qryHour = editValue;
            saveConfig();
            menuState = ST_QRY;
            menuIndex = 2;
            menuCount = 4;
            break;

        case ST_QRY_DAY:
            config.qryDay = menuIndex;
            saveConfig();
            menuState = ST_QRY;
            menuIndex = 1;
            menuCount = 4;
            break;

        case ST_ABOUT:
            menuState = ST_MAIN;
            menuIndex = 3;
            menuCount = 4;
            break;
    }

    showCurrentMenu();
}

void handleBack() {
    lastActivity = millis();

    switch (menuState) {
        case ST_IDLE:
            break;
                case ST_MAIN:
            menuState = ST_IDLE;
            screensaverActive = true;
            showScreensaver();
            break;
        case ST_SET:
            menuState = ST_MAIN;
            menuIndex = 1;
            menuCount = 4;
            break;
        case ST_QRY:
            menuState = ST_MAIN;
            menuIndex = 0;
            menuCount = 4;
            break;
        case ST_PUSH:
            menuState = ST_MAIN;
            menuIndex = 2;
            menuCount = 4;
            break;
        case ST_SET_BLD:
            menuState = ST_SET;
            menuIndex = 0;
            menuCount = 4;
            break;
        case ST_SET_ROOM:
            menuState = ST_SET;
            menuIndex = 1;
            menuCount = 4;
            break;
        case ST_SET_DEF:
            menuState = ST_SET;
            menuIndex = 2;
            menuCount = 4;
            break;
        case ST_SET_WIFI:
            menuState = ST_SET;
            menuIndex = 3;
            menuCount = 4;
            break;
                        case ST_PUSH_FREQ:
            menuState = ST_PUSH;
            menuIndex = 1;
            menuCount = 5;
            break;
        case ST_PUSH_EN:
            menuState = ST_PUSH;
            menuIndex = 0;
            menuCount = 5;
            break;
        case ST_PUSH_HOUR:
            menuState = ST_PUSH;
            menuIndex = 2;
            menuCount = 5;
            break;
                case ST_PUSH_DAY:
            menuState = ST_PUSH;
            menuIndex = 2;
            menuCount = 5;
            break;
                        case ST_QRY_FREQ:
            menuState = ST_QRY;
            menuIndex = 1;
            menuCount = 4;
            break;
        case ST_QRY_HOUR:
            menuState = ST_QRY;
            menuIndex = 2;
            menuCount = 4;
            break;
        case ST_QRY_DAY:
            menuState = ST_QRY;
            menuIndex = 1;
            menuCount = 4;
            break;
        case ST_ABOUT:
            menuState = ST_MAIN;
            menuIndex = 3;
            menuCount = 4;
            break;
    }

    showCurrentMenu();
}

// ==================== setup ====================
void setup() {
    Serial.begin(115200);
    Serial.println("Elec Terminal v6.0");

    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    pinMode(PIN_ENC_BTN, INPUT_PULLUP);
    pinMode(PIN_BTN_OK, INPUT_PULLUP);
    pinMode(PIN_BTN_BACK, INPUT_PULLUP);

    attachInterrupt(PIN_ENC_A, encoderISR, CHANGE);

    // OLED初始化
    Wire.begin(PIN_SDA, PIN_SCL);
    delay(250);
    display.begin(0x3C, true);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    drawStrCenter(28, "Starting...");
    display.display();

    loadConfig();
    Serial.printf("Room: %d-%d\n", config.building, config.room);

    // 直接连接 WiFi
    connectWiFi();

    // NTP时间同步
    if (wifiConnected) {
        configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
        queryElectricity();
    }
    lastPush = millis();

    showCurrentMenu();
}

// ==================== loop ====================
void loop() {
    int delta = readEncoder();
    bool encBtn = readBtn(PIN_ENC_BTN);
    bool ok = readBtn(PIN_BTN_OK);
    bool back = readBtn(PIN_BTN_BACK);

        // 检测WiFi刚连上：立即查询
    if (wifiConnected && !wifiWasConnected) {
        Serial.println("[WiFi] Just connected, querying...");
        if (queryElectricity()) {
            queryOk = true;
            showCurrentMenu();
        }
        lastQuery = millis();
    }
    wifiWasConnected = wifiConnected;

    // 任何操作唤醒息屏 → 进入屏保
        bool anyInput = (delta != 0 || encBtn || ok || back);

        // 定时查询（daily/weekly，每小时只触发一次）
    if (wifiConnected) {
        struct tm ti;
        if (getLocalTime(&ti, 100)) {
            bool shouldQry = false;
            if (config.qryDaily) {
                shouldQry = (ti.tm_hour == config.qryHour);
            } else {
                int wdayMap[] = {1, 2, 3, 4, 5, 6, 0};
                shouldQry = (ti.tm_wday == wdayMap[config.qryDay] && ti.tm_hour == config.qryHour);
            }
            if (shouldQry && millis() - lastQuery > 300000) {
                if (queryElectricity()) {
                    queryOk = true;
                    showCurrentMenu();
                    Serial.printf("[AutoQry] OK (daily=%d day=%d hour=%d)\n",
                                  config.qryDaily, config.qryDay, config.qryHour);
                }
                lastQuery = millis();
            }
                        // 定时推送
            if (config.pushEnabled) {
                bool shouldPush = false;
                if (config.pushDaily) {
                    shouldPush = (ti.tm_hour == config.pushHour);
                } else {
                    int wdayMap[] = {1, 2, 3, 4, 5, 6, 0};
                    shouldPush = (ti.tm_wday == wdayMap[config.pushDay] && ti.tm_hour == config.pushHour);
                }
                                if (shouldPush && ti.tm_hour != lastPushHour) {
                    queryElectricity();  // 推送前先查最新数据
                    if (pushNotification()) {
                        lastPushHour = ti.tm_hour;
                        lastPush = millis();
                        Serial.printf("[AutoPush] Sent (daily=%d day=%d hour=%d)\n",
                                      config.pushDaily, config.pushDay, config.pushHour);
                    }
                }
            }
        }
    }

    if (displayOff) {
        if (anyInput) {
            displayOff = false;
            screensaverActive = true;
            screensaverPage = 0;
            lastActivity = millis();
            showScreensaver();
        }
        delay(10);
        return;
    }

                                                // 屏保模式
    if (screensaverActive) {
        if (delta != 0 || back) {
            // 旋钮旋转/Back键 → 退出屏保，进入菜单
            screensaverActive = false;
            menuState = ST_MAIN;
            menuIndex = 0;
            menuCount = 4;
            lastActivity = millis();
            showCurrentMenu();
        } else if (encBtn) {
            // 旋钮按下 → 退出屏保，进入菜单
            screensaverActive = false;
            menuState = ST_MAIN;
            menuIndex = 0;
            menuCount = 4;
            lastActivity = millis();
            showCurrentMenu();
        } else if (ok) {
            // CONFIRM键 → 切换屏保页面
            screensaverPage = (screensaverPage + 1) % 2;
            lastActivity = millis();
            showScreensaver();
        }
        delay(10);
        return;
    }

            // 闲置界面（IDLE）
    if (menuState == ST_IDLE) {
        if (delta != 0 || encBtn) {
            menuState = ST_MAIN;
            menuIndex = 0;
            menuCount = 4;
            lastActivity = millis();
            showCurrentMenu();
        }
                checkWiFi();
        delay(10);
        return;
    }

            // 编辑模式：旋钮调值
    if (menuState == ST_SET_BLD || menuState == ST_SET_ROOM ||
        menuState == ST_PUSH_HOUR || menuState == ST_PUSH_EN ||
        menuState == ST_QRY_HOUR) {
        if (delta != 0) {
            editValue += delta;
            if (editValue < editMin) editValue = editMin;
            if (editValue > editMax) editValue = editMax;
            showCurrentMenu();
        }
        if (encBtn || ok) {
            handleOk();
        }
        if (back) {
            handleBack();
        }
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

    // CONFIRM键 → 回到屏保
    if (ok) {
        screensaverActive = true;
        lastActivity = millis();
        showScreensaver();
        delay(10);
        return;
    }

    // 旋钮按下 = 菜单确认
    if (encBtn) {
        handleOk();
    }

    // 返回键
    if (back) {
        handleBack();
    }

    // 闲置超时 → 息屏
    if (millis() - lastActivity > SCREENSAVER_TIMEOUT) {
        displayOff = true;
        screensaverActive = false;
        display.clearDisplay();
        display.display();
    }

    delay(10);
}
