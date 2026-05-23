// ==================== menu.h ====================
// 菜单状态机：显示 + 操作逻辑
// v8.0：删除 Hour 选项，查询/推送统一定在 0 点
// v9.0：新增 OTA 无线更新
// v9.1：修复 Push Now 闪退（去掉 goto，加调试日志）
#ifndef MENU_H
#define MENU_H

#include "globals.h"
#include "display.h"
#include "config_store.h"
#include "api.h"
#include "wifi_manager.h"
#include "ota_manager.h"

// 前向声明（定义在 .ino / wifi_manager.h）
void initWiFiAndNTP();
void shutdownWiFi();
void quickNTPSync();

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

        case ST_SET_OTA:
            drawTitle("OTA Update");
            drawStrCenter(22, "AP: ESP32_ELEC");
            drawStrCenter(34, "PW: 12345678");
            drawStrCenter(46, "192.168.4.1");
            break;

        case ST_SET_WIFI:
            drawTitle("WiFi");
            drawStrCenter(22, "Open Portal?");
            drawStrCenter(38, "OK=Yes  Back=No");
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
            drawStrCenter(22, "Elec Terminal v9.1");
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
    }

    drawStatusBar();
    display.display();
}

// ==================== 菜单确认 ====================
void handleOk() {
    lastActivity = millis();

    switch (menuState) {
        case ST_IDLE:
            screensaverPage = (screensaverPage + 1) % 2;
            break;

        case ST_MAIN:
            switch (menuIndex) {
                case 0:
                    menuState = ST_QRY;
                    menuIndex = 0;
                    menuCount = 3;
                    break;
                case 1:
                    menuState = ST_SET;
                    menuIndex = 0;
                    menuCount = 6;
                    break;
                case 2:
                    menuState = ST_PUSH;
                    menuIndex = 0;
                    menuCount = 4;
                    break;
                case 3:
                    menuState = ST_ABOUT;
                    break;
            }
            break;

        case ST_SET:
            switch (menuIndex) {
                case 0:
                    menuState = ST_SET_BLD;
                    editValue = config.building;
                    editMin = 1; editMax = 99;
                    strcpy(editLabel, "Building");
                    break;
                case 1:
                    menuState = ST_SET_ROOM;
                    editValue = config.room;
                    editMin = 1; editMax = 999;
                    strcpy(editLabel, "Room");
                    break;
                case 2:
                    menuState = ST_SET_DEF;
                    break;
                case 3:
                    menuState = ST_SET_WIFI;
                    break;
                case 4:
                    lastActivity = millis();
                    menuState = ST_SET_OTA;
                    startOtaServer();
                    lastActivity = millis();
                    display.clearDisplay();
                    display.setTextSize(1);
                    display.setTextColor(SH110X_WHITE);
                    drawTitle("OTA Update");
                    drawStr(10, 20, "AP: ESP32_ELEC");
                    drawStr(10, 32, "PW: 12345678");
                    drawStr(10, 46, "192.168.4.1");
                    display.display();
                    break;
                case 5:
                    menuState = ST_MAIN;
                    menuIndex = 1;
                    menuCount = 4;
                    break;
            }
            break;

        case ST_QRY:
            switch (menuIndex) {
                case 0: // Current
                    {
                        bool wasOff = !wifiConnected;
                        if (wasOff) {
                            WiFi.disconnect();
                            delay(100);
                            WiFi.mode(WIFI_STA);
                            WiFi.begin();
                            unsigned long t0 = millis();
                            while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
                                delay(500);
                            }
                            if (WiFi.status() == WL_CONNECTED) {
                                wifiConnected = true;
                            }
                        }
                        if (wifiConnected) {
                            if (queryElectricity()) {
                                queryOk = true;
                                saveQueryCache();
                            }
                        } else {
                            strcpy(queryMsg, "No WiFi");
                        }
                        if (wasOff) {
                            shutdownWiFi();
                        }
                    }
                    menuState = ST_MAIN;
                    menuIndex = 0;
                    menuCount = 4;
                    break;
                case 1:
                    menuState = ST_QRY_FREQ;
                    menuIndex = config.qryDaily ? 0 : 1;
                    menuCount = 2;
                    break;
                case 2:
                    menuState = ST_MAIN;
                    menuIndex = 0;
                    menuCount = 4;
                    break;
            }
            break;

        case ST_PUSH:
            switch (menuIndex) {
                case 0:
                    menuState = ST_PUSH_EN;
                    editValue = config.pushEnabled ? 1 : 0;
                    editMin = 0; editMax = 1;
                    strcpy(editLabel, "Trigger");
                    break;
                case 1:
                    menuState = ST_PUSH_FREQ;
                    menuIndex = config.pushDaily ? 0 : 1;
                    break;
                case 2: // Push Now
                    {
                        Serial.println("[PushNow] Enter");
                        bool wasOff = !wifiConnected;
                        bool pushOk = false;

                        if (wasOff) {
                            display.clearDisplay();
                            display.setTextSize(1);
                            display.setTextColor(SH110X_WHITE);
                            drawTitle("Push Now");
                            drawStr(10, 24, "WiFi connecting...");
                            display.display();

                            WiFi.disconnect();
                            delay(100);
                            WiFi.mode(WIFI_STA);
                            WiFi.begin();

                            unsigned long t0 = millis();
                            while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
                                delay(500);
                                Serial.print(".");
                            }

                            if (WiFi.status() == WL_CONNECTED) {
                                wifiConnected = true;
                                Serial.printf("[PushNow] WiFi OK: %s\n", WiFi.localIP().toString().c_str());
                            }
                        }

                        if (wifiConnected) {
                            if (balance > 0) {
                                Serial.printf("[PushNow] Cache: bal=%.2f\n", balance);
                            } else {
                                display.clearDisplay();
                                display.setTextSize(1);
                                display.setTextColor(SH110X_WHITE);
                                drawTitle("Push Now");
                                drawStr(10, 24, "Querying...");
                                display.display();
                                Serial.println("[PushNow] No cache, querying...");
                                queryElectricity();
                            }

                            display.clearDisplay();
                            display.setTextSize(1);
                            display.setTextColor(SH110X_WHITE);
                            drawTitle("Push Now");
                            drawStr(10, 24, "Pushing...");
                            display.display();

                            Serial.println("[PushNow] Sending...");
                            pushOk = pushNotification();
                            Serial.printf("[PushNow] Result: %d\n", pushOk);
                        } else {
                            Serial.println("[PushNow] No WiFi");
                        }

                        if (wasOff) {
                            shutdownWiFi();
                        }

                                                lastActivity = millis();
                        display.clearDisplay();
                        display.setTextSize(1);
                        display.setTextColor(SH110X_WHITE);
                        drawTitle("Push Now");
                        if (pushOk) {
                            drawStrCenter(28, "Pushed!");
                        } else if (wifiConnected) {
                            drawStrCenter(28, "Fail");
                        } else {
                            drawStrCenter(28, "No WiFi");
                        }
                        display.display();
                        delay(2000);
                        menuState = ST_PUSH;
                        menuIndex = 2;
                        menuCount = 4;
                        showCurrentMenu();
                        readBtn(PIN_ENC_BTN);
                        readBtn(PIN_BTN_OK);
                        readBtn(PIN_BTN_BACK);
                    }
                    break;
                case 3:
                    menuState = ST_MAIN;
                    menuIndex = 2;
                    menuCount = 4;
                    break;
            }
            break;

        // ---- 设置子菜单 ----
        case ST_SET_BLD:
            config.building = editValue;
            saveConfig();
            menuState = ST_SET;
            menuIndex = 0;
            menuCount = 6;
            break;

        case ST_SET_ROOM:
            config.room = editValue;
            saveConfig();
            menuState = ST_SET;
            menuIndex = 1;
            menuCount = 6;
            break;

        case ST_SET_DEF:
            config.building = 46;
            config.room = 416;
            saveConfig();
            menuState = ST_SET;
            menuIndex = 2;
            menuCount = 6;
            break;

        case ST_SET_WIFI:
            lastActivity = millis();
            startWifiPortal();
            menuState = ST_SET;
            menuIndex = 3;
            menuCount = 6;
            showCurrentMenu();
            break;

        case ST_SET_OTA:
            break;

        // ---- 推送子菜单 ----
        case ST_PUSH_FREQ:
            config.pushDaily = (menuIndex == 0);
            saveConfig();
            menuState = ST_PUSH;
            menuIndex = 1;
            menuCount = 4;
            break;

        case ST_PUSH_EN:
            config.pushEnabled = (editValue == 1);
            saveConfig();
            menuState = ST_PUSH;
            menuIndex = 0;
            menuCount = 4;
            break;

        case ST_PUSH_DAY:
            config.pushDay = menuIndex;
            saveConfig();
            menuState = ST_PUSH;
            menuIndex = 1;
            menuCount = 4;
            break;

        // ---- 查询子菜单 ----
        case ST_QRY_FREQ:
            config.qryDaily = (menuIndex == 0);
            saveConfig();
            menuState = ST_QRY;
            menuIndex = 1;
            menuCount = 3;
            break;

        case ST_QRY_DAY:
            config.qryDay = menuIndex;
            saveConfig();
            menuState = ST_QRY;
            menuIndex = 1;
            menuCount = 3;
            break;

        case ST_ABOUT:
            menuState = ST_MAIN;
            menuIndex = 3;
            menuCount = 4;
            break;
    }

    showCurrentMenu();
}

// ==================== 菜单返回 ====================
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
            menuCount = 6;
            break;
        case ST_SET_ROOM:
            menuState = ST_SET;
            menuIndex = 1;
            menuCount = 6;
            break;
        case ST_SET_DEF:
            menuState = ST_SET;
            menuIndex = 2;
            menuCount = 6;
            break;
        case ST_SET_WIFI:
            menuState = ST_SET;
            menuIndex = 3;
            menuCount = 6;
            break;
        case ST_SET_OTA:
            otaServer.stop();
            menuState = ST_SET;
            menuIndex = 4;
            menuCount = 6;
            break;
        case ST_PUSH_FREQ:
            menuState = ST_PUSH;
            menuIndex = 1;
            menuCount = 4;
            break;
        case ST_PUSH_EN:
            menuState = ST_PUSH;
            menuIndex = 0;
            menuCount = 4;
            break;
        case ST_PUSH_DAY:
            menuState = ST_PUSH;
            menuIndex = 1;
            menuCount = 4;
            break;
        case ST_QRY_FREQ:
            menuState = ST_QRY;
            menuIndex = 1;
            menuCount = 3;
            break;
        case ST_QRY_DAY:
            menuState = ST_QRY;
            menuIndex = 1;
            menuCount = 3;
            break;
        case ST_ABOUT:
            menuState = ST_MAIN;
            menuIndex = 3;
            menuCount = 4;
            break;
    }

    showCurrentMenu();
}

#endif
