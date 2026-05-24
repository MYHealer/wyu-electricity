// ==================== sleep_manager.h ====================
// Deep Sleep 管理：唤醒源配置、休眠/唤醒流程
//
// 唤醒源：
//   1. GPIO8（BOOT键）低电平唤醒（飞线连接旋钮按下脚）
//   2. 定时器唤醒（查询/推送时刻）
//
// 唤醒后芯片重启，用 RTC 内存保存状态
#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "esp_sleep.h"

// ==================== RTC 内存（重启不丢失） ====================
// 唤醒原因
#define WAKE_NONE       0   // 上电复位（首次启动）
#define WAKE_GPIO       1   // GPIO8 按钮唤醒
#define WAKE_TIMER      2   // 定时器唤醒（查询/推送）

RTC_DATA_ATTR int wakeReason = WAKE_NONE;
RTC_DATA_ATTR int wakeCount = 0;           // 累计唤醒次数
RTC_DATA_ATTR unsigned long lastSleepMs = 0; // 上次 sleep 时的 millis

// ==================== 计算下次事件的 sleep 时间（微秒） ====================
// 查询和推送都在每日/每周 0 点触发，取两者较早的时间
uint64_t calcNextEventSleepUs() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        // 没有时间，sleep 5分钟后重试
        return 300ULL * 1000000ULL;
    }

    unsigned long nowSec = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    int todayWday = timeinfo.tm_wday;  // 0=Sun

    // 计算到目标星期几的 0 点还有多少秒
    auto calcNextDaySec = [&](int targetWday) -> unsigned long {
        int daysAhead = (targetWday - todayWday + 7) % 7;
        if (daysAhead == 0 && nowSec > 0) {
            daysAhead = 7;  // 今天已过 0 点，等下周
        }
        return daysAhead * 86400ULL - nowSec;
    };

        // 1. 查询时间：qryDaily=每天，qryWeekly=每周指定日
    unsigned long nextQuerySec = 86400 * 7;  // 默认下周
    if (config.qryDaily) {
        nextQuerySec = (nowSec == 0) ? 0 : 86400 - nowSec;
    } else {
        nextQuerySec = calcNextDaySec(menuIdxToWday(config.qryDay));
    }

    // 2. 推送时间：pushDaily=每天，pushWeekly=每周指定日
    unsigned long nextPushSec = 86400 * 7;  // 默认下周
    if (config.pushEnabled) {
        if (config.pushDaily) {
            nextPushSec = (nowSec == 0) ? 0 : 86400 - nowSec;
        } else {
            nextPushSec = calcNextDaySec(menuIdxToWday(config.pushDay));
        }
    }

    // 取较短的时间
    uint64_t sleepSec = (nextQuerySec < nextPushSec) ? nextQuerySec : nextPushSec;

    // 至少 sleep 60 秒
    if (sleepSec < 60) sleepSec = 60;

    Serial.printf("[Sleep] Next event in %lu sec (query=%lu, push=%lu)\n",
        (unsigned long)sleepSec, (unsigned long)nextQuerySec, (unsigned long)nextPushSec);
    return sleepSec * 1000000ULL;
}

// ==================== 进入 Deep Sleep ====================
void enterDeepSleep() {
    Serial.println("[Sleep] Entering deep sleep...");
    Serial.flush();  // 等串口发完

            // 关 OLED — 硬件关屏（0xAE = Display OFF）
    display.clearDisplay();
    display.display();
    delay(50);
    Wire.beginTransmission(0x3C);
    Wire.write(0x00);  // Co=0, D/C#=0 → 命令
    Wire.write(0xAE);  // Display OFF
    Wire.endTransmission();
    delay(10);

    // 关 WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // 关 LED
    digitalWrite(PIN_LED, HIGH);  // 灭灯

    // ===== 关闭所有外设 GPIO，减少漏电 =====
    Wire.end();  // 释放 I2C 总线
    pinMode(PIN_SDA, OUTPUT);
    digitalWrite(PIN_SDA, LOW);
    pinMode(PIN_SCL, OUTPUT);
    digitalWrite(PIN_SCL, LOW);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);   // LED 低电平亮，HIGH=灭
            pinMode(PIN_ENC_A, OUTPUT);
    digitalWrite(PIN_ENC_A, LOW);
    pinMode(PIN_ENC_B, OUTPUT);
    digitalWrite(PIN_ENC_B, LOW);
    pinMode(PIN_ENC_BTN, OUTPUT);
    digitalWrite(PIN_ENC_BTN, LOW);
    pinMode(PIN_BTN_BACK, OUTPUT);
    digitalWrite(PIN_BTN_BACK, LOW);

    // 记录状态到 RTC 内存
    lastSleepMs = millis();
    wakeCount++;

    // 计算下次事件时间
    uint64_t sleepTimeUs = calcNextEventSleepUs();

    // 配置唤醒源：GPIO3（BTN_OK）+ 定时器
    // ESP32-C3 deep sleep GPIO 唤醒只支持 GPIO 0-5
    esp_deep_sleep_enable_gpio_wakeup(BIT(GPIO_NUM_3), ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_sleep_enable_timer_wakeup(sleepTimeUs);

    // 进入深睡眠
    esp_deep_sleep_start();
}

// ==================== 检查唤醒原因 ====================
// 返回：WAKE_NONE / WAKE_GPIO / WAKE_TIMER
int checkWakeReason() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    switch (cause) {
                case ESP_SLEEP_WAKEUP_GPIO:
            Serial.println("[Sleep] Woke by GPIO3 (BTN_OK) button");
            return WAKE_GPIO;

        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("[Sleep] Woke by timer");
            return WAKE_TIMER;

        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            Serial.println("[Sleep] Fresh boot (power-on/reset)");
            return WAKE_NONE;
    }
}

// ==================== 唤醒后执行定时任务 ====================
// 定时器唤醒 = 到了 0 点，执行查询 + 推送（如果该天推送）
void executeScheduledTasks() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 3000)) {
        Serial.println("[Sleep] NTP not ready, skip scheduled tasks");
        return;
    }

    int hour = timeinfo.tm_hour;
    int wday = timeinfo.tm_wday;  // 0=Sun

    // 查询任务：每次 0 点唤醒都执行
        Serial.println("[Sleep] Executing scheduled query...");
    bool queryResult = queryElectricity();
    if (queryResult) {
        saveQueryCache();  // 查询成功，更新缓存
    }
    delay(2000);  // 等连接释放

        // 推送任务：检查是否到了推送日
    // 只要配置开启且有有效余额（查询成功或缓存有效），就推送
    if (config.pushEnabled && balance > 0) {
        bool shouldPush = false;

                if (config.pushDaily) {
            shouldPush = true;
        } else {
            if (wday == menuIdxToWday(config.pushDay)) {
                shouldPush = true;
            }
        }

        if (shouldPush) {
            // 推送前检查 WiFi 是否还在
            if (!wifiConnected) {
                Serial.println("[Sleep] WiFi lost before push, reconnecting...");
                tryConnectWiFi(15000);
            }
            Serial.printf("[Sleep] Pushing! Day=%d Hour=%d Balance=%.2f\n", wday, hour, balance);
            bool pushResult = pushNotification();
            Serial.printf("[Sleep] Push result: %d\n", pushResult);
        }
    } else {
        Serial.printf("[Sleep] Push skipped: enabled=%d balance=%.2f queryOk=%d\n",
                      config.pushEnabled, balance, queryOk);
    }
}

#endif // SLEEP_MANAGER_H
