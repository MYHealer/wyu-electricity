// ==================== api.h ====================
// 电费查询 + Server酱推送
#ifndef API_H
#define API_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "globals.h"

// ==================== 电费查询 ====================
bool queryElectricity() {
    Serial.println("[API] >>> ENTER queryElectricity");
    if (!wifiConnected) {
        strcpy(queryMsg, "No WiFi");
        return false;
    }

    HTTPClient http;
    http.begin(API_URL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("User-Agent", "Mozilla/5.0");
    http.addHeader("Referer", "http://202.192.240.231/recharge.html");

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
        Serial.printf("[API] Resp len=%d\n", resp.length());

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, resp)) {
        strcpy(queryMsg, "JSON Error");
        return false;
    }

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
    Serial.println("[API] >>> ENTER pushNotification");
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

        Serial.printf("[Push] Sending to %s\n", config.sendkey);
    int code = http.POST((uint8_t*)body, strlen(body));
    http.end();

    Serial.printf("[Push] Code: %d\n", code);
    return code == 200;
}

#endif // API_H
