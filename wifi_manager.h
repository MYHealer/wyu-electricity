// ==================== wifi_manager.h ====================
// WiFi 连接 + WiFiManager 配网（开机阻塞，菜单非阻塞）
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WiFiManager.h>
#include "esp_wifi.h"
#include "globals.h"
#include "config_store.h"
#include "display.h"

WiFiManager wm;
unsigned long wifiLastReconnect = 0;
bool portalActive = false;  // 配网期间为 true，禁用自动睡眠

const unsigned long WIFI_RECONNECT_INTERVAL = 15000;
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;

// 自定义参数：Server酱 SendKey
WiFiManagerParameter sendkeyParam("sendkey", "Server酱 SendKey", "", 64);
bool wmParamAdded = false;

// ==================== 关闭 WiFi（省电） ====================
void shutdownWiFi() {
    if (portalActive) return;  // ★ 配网期间不关 WiFi
    Serial.println("[WiFi] Shutting down...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
    wifiConnected = false;
    wifiWasConnected = false;
    digitalWrite(PIN_LED, LOW);
    Serial.println("[WiFi] Off, saving power");
}

// ==================== 同步 NTP ====================
void syncNTP() {
    configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
    Serial.println("[NTP] Syncing...");
    struct tm ti;
    if (getLocalTime(&ti, 3000)) {
        Serial.printf("[NTP] OK: %d-%02d-%02d %02d:%02d:%02d\n",
            ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
            ti.tm_hour, ti.tm_min, ti.tm_sec);
    } else {
        Serial.println("[NTP] Sync timeout");
    }
}

// ==================== 快速取 NTP 时间（不保持连接） ====================
void quickNTPSync() {
    if (portalActive) return;  // ★ 配网期间跳过
    Serial.println("[WiFi] Quick NTP sync...");
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    delay(200);
    WiFi.begin();

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(50);
    }

    if (WiFi.status() == WL_CONNECTED) {
        configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000)) {
            Serial.printf("[WiFi] NTP OK: %d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
    } else {
        Serial.println("[WiFi] NTP sync failed");
    }

    shutdownWiFi();
}

// ==================== 非阻塞连接尝试 ====================
bool tryConnectWiFi(unsigned long timeoutMs) {
    Serial.println("[WiFi] Attempting connection...");
    WiFi.disconnect();
    delay(100);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    delay(200);
    WiFi.begin();

    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("[WiFi] Connect timeout");
    WiFi.disconnect();
    return false;
}

// ==================== WiFi 事件回调 ====================
void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("[WiFi] AP connected");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("[WiFi] Disconnected");
            wifiConnected = false;
            digitalWrite(PIN_LED, LOW);
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("[WiFi] IP: %s  RSSI: %d\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            wifiConnected = true;
            break;
        default: break;
    }
}

// ==================== 配网回调 ====================
void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("[WiFi] Entered config mode");
    Serial.printf("[WiFi] ConfigAP SSID: %s\n", myWiFiManager->getConfigPortalSSID().c_str());
    Serial.printf("[WiFi] ConfigAP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // ★ 在 OLED 上显示配网信息
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    // 标题
    display.fillRect(0, 0, 128, 16, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    drawStrCenter(5, "WiFi Setup");
    display.setTextColor(SH110X_WHITE);

    // WiFi 名称
    display.setCursor(0, 20);
    display.print("SSID:");
    display.setCursor(0, 30);
    display.print(myWiFiManager->getConfigPortalSSID().c_str());

    // 密码
    display.setCursor(0, 42);
    display.print("Pass: 12345678");

    // IP 地址
    display.setCursor(0, 54);
    display.print("IP:");
    display.print(WiFi.softAPIP().toString().c_str());

    display.display();
}

// ==================== 保存回调 ====================
void saveConfigCallback() {
    strncpy(config.sendkey, sendkeyParam.getValue(), sizeof(config.sendkey) - 1);
    config.sendkey[sizeof(config.sendkey) - 1] = '\0';
    saveConfig();
    Serial.printf("[WiFi] Saved sendkey: %s\n", config.sendkey);
}

// ==================== 准备 Portal 参数 ====================
void preparePortal() {
    if (!wmParamAdded) {
        wm.addParameter(&sendkeyParam);
        wmParamAdded = true;
    }
    sendkeyParam.setValue(config.sendkey, 64);
}

// ==================== 连接 WiFi（开机阻塞） ====================
void connectWiFiOnBoot() {
    Serial.println("[WiFi] Starting...");

    WiFi.mode(WIFI_STA);
    WiFi.onEvent(WiFiEvent);
    esp_wifi_set_max_tx_power(34);

    Serial.println("[WiFi] Trying saved WiFi...");
    if (tryConnectWiFi(WIFI_CONNECT_TIMEOUT)) {
        wifiConnected = true;
        digitalWrite(PIN_LED, HIGH);
        syncNTP();
        wifiLastReconnect = millis();
        return;
    }

    // 连接失败：静默离线，用户通过菜单 WiFi Setup 手动配网
    wifiConnected = false;
    Serial.println("[WiFi] No saved WiFi, running offline. Use menu to setup.");
    wifiLastReconnect = millis();
}

// ==================== 菜单触发配网（非阻塞） ====================
void startWifiPortal() {
    if (portalActive) {
        Serial.println("[WiFi] Portal already active, skipping...");
        return;
    }

        Serial.println("[WiFi] Menu triggered portal...");
    portalActive = true;

    // 确保 WiFi 射频启动
    esp_wifi_start();
    delay(200);

    // ★ 断开已保存的 WiFi，防止自动连接干扰配网
    WiFi.disconnect(true);
    delay(100);
    WiFi.onEvent(WiFiEvent);

        wm.setConfigPortalTimeout(600);
    wm.setConfigPortalBlocking(false);  // ★ 非阻塞模式
    wm.setBreakAfterConfig(true);       // ★ 连上WiFi后不自动退出，等用户按键
    wm.setAPCallback(configModeCallback);
    wm.setSaveConfigCallback(saveConfigCallback);
    preparePortal();
    wm.startConfigPortal("ESP32-Elec", "12345678");
    // 非阻塞模式下立即返回，配网在后台运行

    // ★ 等待 AP 完全启动
    delay(500);

    // ★ 直接显示配网信息（不依赖回调）
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    // 标题
    display.fillRect(0, 0, 128, 16, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    drawStrCenter(5, "WiFi Setup");
    display.setTextColor(SH110X_WHITE);

    // WiFi 名称
    display.setCursor(0, 20);
    display.print("SSID:");
    display.setCursor(0, 30);
    display.print("ESP32-Elec");

    // 密码
    display.setCursor(0, 42);
    display.print("Pass: 12345678");

    // IP 地址
    display.setCursor(0, 54);
    display.print("IP:");
    display.print(WiFi.softAPIP().toString().c_str());

    display.display();
}

// ==================== 检查配网状态（loop 中调用） ====================
void checkPortalStatus() {
    if (!portalActive) return;

    // 非阻塞模式下，需要手动处理 webserver
    wm.process();

    // ★ 配网期间不自动退出，只在用户按键或超时时退出
    // 保留 WiFi 连接状态更新，但不改变 portalActive
    if (WiFi.status() == WL_CONNECTED && !wifiConnected) {
        Serial.println("[WiFi] Portal connected!");
        wifiConnected = true;
        wifiWasConnected = true;
        digitalWrite(PIN_LED, HIGH);
        Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
        syncNTP();
        wifiLastReconnect = millis();
        // ★ 不设置 portalActive = false，让用户手动按键退出
    }
}

// ==================== 停止配网（按钮触发） ====================
void stopWifiPortal() {
    if (!portalActive) return;
    Serial.println("[WiFi] Portal interrupted by user");
    wm.stopConfigPortal();
    portalActive = false;
    wifiLastReconnect = millis();
    // 恢复显示在 .ino 中处理
}

// ==================== 重连 WiFi（非阻塞） ====================
void checkWiFi() {
    if (wifiConnected) return;
    if (millis() - wifiLastReconnect < WIFI_RECONNECT_INTERVAL) return;

    wifiLastReconnect = millis();
    Serial.println("[WiFi] Reconnecting...");

    if (tryConnectWiFi(WIFI_CONNECT_TIMEOUT)) {
        wifiConnected = true;
        digitalWrite(PIN_LED, HIGH);
        Serial.printf("[WiFi] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
        syncNTP();
    } else {
        Serial.println("[WiFi] Reconnect failed, will retry...");
    }
}

#endif // WIFI_MANAGER_H
