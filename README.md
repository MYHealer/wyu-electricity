# 五邑大学宿舍电费查询终端

基于 ESP32-C3 SuperMini 的宿舍电费查询终端，支持 OLED 显示、旋钮操作、Deep Sleep 省电、Server酱推送。

## 硬件

- ESP32-C3 SuperMini
- SH1106 OLED 128x64 (I2C)
- 旋转编码器（带按键）
- 确认键 + 返回键

### 接线

| 功能 | GPIO |
|------|------|
| OLED SDA | 4 |
| OLED SCL | 5 |
| 旋钮 A | 0 |
| 旋钮 B | 1 |
| 旋钮按下 | 6 |
| 确认键 (CON) | 3 |
| 返回键 | 10 |
| 板载 LED | 2 |

## 功能

- **电费查询** — 对接五邑大学电费 API，实时显示余额和用电量
- **定时查询** — 每日/每周自动查询，支持选择星期几
- **Server酱推送** — 查询结果自动推送到微信
- **Deep Sleep** — 30 秒无操作自动休眠，GPIO3 按键唤醒，休眠电流 ~9μA
- **Web 配网** — 首次使用或 WiFi 变更时，手机扫码配网
- **OTA 更新** — 通过 Web 界面上传固件，无需 USB 线
- **旋钮菜单** — 旋转选择 + 按下确认，操作简洁

## 软件架构

```
esp32_electricity.ino  — 主程序（setup/loop）
├── config.h           — 引脚定义、常量、数据结构
├── globals.h          — 全局变量声明
├── config_store.h     — NVS 配置读写
├── sleep_manager.h    — Deep Sleep 管理、唤醒源、定时任务
├── wifi_manager.h     — WiFi 连接 + Web 配网
├── encoder.h          — 旋转编码器 + 按键中断
├── display.h          — OLED 绘图辅助 + 屏保
├── api.h              — 电费查询 + Server酱推送
├── ota_manager.h      — OTA 固件更新
└── menu.h             — 菜单状态机
```

## Deep Sleep 优化

休眠前执行：
1. OLED 硬件关屏（SH1106 `0xAE` 指令）
2. WiFi 断开并关闭
3. Wire.end() 释放 I2C 总线
4. 所有 GPIO 拉低（GPIO3 保留作为唤醒源）

实测休眠电流：**~9μA**

## 依赖

- Arduino ESP32 Board Package
- Adafruit_GFX
- Adafruit_SH110X
- ArduinoJson
- WiFi (ESP32 内置)
- HTTPClient (ESP32 内置)
- Preferences (ESP32 内置)
