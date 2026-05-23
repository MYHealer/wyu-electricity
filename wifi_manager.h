// ==================== wifi_manager.h ====================
// WiFi 连接 + WiFiManager 配网（开机阻塞，菜单也阻塞）
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WiFiManager.h>
#include "esp_wifi.h"
#include "globals.h"
#include "config_store.h"

WiFiManager wm;
unsigned long wifiLastReconnect = 0;

const unsigned long WIFI_RECONNECT_INTERVAL = 15000;
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;

// 自定义参数：Server酱 SendKey
WiFiManagerParameter sendkeyParam("sendkey", "Server酱 SendKey", "", 64);
bool wmParamAdded = false;

// ==================== 关闭 WiFi（省电） ====================
void shutdownWiFi() {
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
    Serial.println("[WiFi] Quick NTP sync...");
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);  // 降低发射功率，减少峰值电流防 brownout
    delay(200);  // 等电源稳定
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
    WiFi.setTxPower(WIFI_POWER_8_5dBm);  // 降低发射功率防 brownout
    delay(200);  // 等电源稳定
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
void connectWiFi() {
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

// ==================== 菜单触发配网（阻塞） ====================
void startWifiPortal() {
    Serial.println("[WiFi] Menu triggered portal...");

    // 确保 WiFi 射频启动
    esp_wifi_start();
    delay(200);

    WiFi.mode(WIFI_STA);
    WiFi.onEvent(WiFiEvent);

        wm.setConfigPortalTimeout(600);
    wm.setAPCallback(configModeCallback);
    wm.setSaveConfigCallback(saveConfigCallback);
    preparePortal();
    bool portalOk = wm.startConfigPortal("ESP32-Elec", "12345678");

    if (portalOk) {
        wifiConnected = true;
        digitalWrite(PIN_LED, HIGH);
        Serial.printf("[WiFi] Portal connected! IP: %s\n", WiFi.localIP().toString().c_str());
        syncNTP();
    } else {
        wifiConnected = false;
        Serial.println("[WiFi] Portal timeout, back to menu...");
    }
    wifiLastReconnect = millis();
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
