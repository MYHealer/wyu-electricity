// ==================== display.h ====================
// OLED 绘图辅助
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_SH110X.h>
#include <time.h>
#include "globals.h"

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
    const int centerY = 36;
    const int itemH = 10;
    const int topLimit = 20;
    const int botLimit = 54;

    for (int i = 0; i < count; i++) {
        int y = centerY + (i - menuIndex) * itemH;
        if (y < topLimit - 4 || y > botLimit + 4) continue;
        drawMenuItem(y, items[i], i == menuIndex);
    }
}

void drawStatusBar() {
    if (wifiConnected) {
        drawStr(104, 57, "WiFi");
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
        char roomBuf[16];
        snprintf(roomBuf, sizeof(roomBuf), "%02d-%03d", config.building, config.room);
        display.setTextSize(2);
        int rw = strlen(roomBuf) * 12;
        display.setCursor((128 - rw) / 2, 0);
        display.print(roomBuf);

        display.drawLine(0, 18, 127, 18, SH110X_WHITE);

        display.setTextSize(1);
        display.setCursor(0, 22);
        display.print("Balance:");

        if (queryOk) {
            char balBuf[16];
            snprintf(balBuf, sizeof(balBuf), "%.2f", balance);
            display.setTextSize(2);
            display.setCursor(0, 32);
            display.print(balBuf);
            display.setTextSize(1);
            display.setCursor(display.getCursorX() + 2, 40);
            display.print("Yuan");

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
        if (getCurrentTime(&timeinfo)) {
            char dateBuf[20];
            snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
            display.setTextSize(1);
            drawStrCenter(8, dateBuf);

            const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            drawStrCenter(20, weekdays[timeinfo.tm_wday]);

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

#endif // DISPLAY_H
