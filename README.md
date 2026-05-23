# 五邑大学宿舍电费查询终端

基于 ESP32-C3 的宿舍电费查询终端，支持 OLED 显示、旋钮操作、Deep Sleep 省电、Server酱推送。

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32--C3-orange.svg)

## 功能

- **电费查询** — 对接五邑大学电费 API，实时显示余额和用电量
- **按需联网** — 默认不连 WiFi，仅查询/推送时短暂连网，完成后自动断开，省电
- **定时查询** — 每日/每周自动查询，支持选择星期几
- **Server酱推送** — 查询结果自动推送到微信
- **Deep Sleep** — 30 秒无操作自动休眠，GPIO3 唤醒键唤醒，休眠电流 ~9μA
- **Web 配网** — 旋钮选择 WiFi Setup，手机扫码配网
- **OTA 更新** — 通过 Web 界面上传固件，无需 USB 线
- **旋钮菜单** — 旋转选择 + 按下确认，操作简洁

## 硬件

### 主要硬件

1. **ESP32-C3** 主控板
2. **OLED 显示屏 + EC11 旋转编码器组合模块**（IIC 接口）

> 📷 硬件图片待补充

### 接线

| 功能 | GPIO | 说明 |
|------|------|------|
| OLED SDA | 4 | I2C 数据线 |
| OLED SCL | 5 | I2C 时钟线 |
| 旋钮 A | 0 | 编码器 A 相 |
| 旋钮 B | 1 | 编码器 B 相 |
| 旋钮确认 | 6 | 编码器按键 |
| 唤醒/屏保键 | 3 | 唤醒 + 切换屏保 |
| 返回键 | 10 | 返回上一级菜单 |
| 板载 LED | 2 | WiFi 指示灯（低电平点亮） |

### 接线示意

```
ESP32-C3
    ┌──────────────┐
    │         GP0  │──── 旋钮 A
    │         GP1  │──── 旋钮 B
    │         GP2  │──── 板载 LED
    │         GP3  │──── 唤醒键 (接GND)
    │         GP4  │──── OLED SDA
    │         GP5  │──── OLED SCL
    │         GP6  │──── 旋钮按键 (接GND)
    │         GP10 │──── 返回键 (接GND)
    │         GND  │──── 公共地
    │         3V3  │──── OLED VCC
    └──────────────┘
```

## 首次使用教程

### 第一步：烧录固件

1. 克隆本仓库到本地：
   ```bash
   git clone https://github.com/MYHealer/wyu-electricity.git
   ```
2. 使用 Arduino IDE 打开项目：
   - 安装 [Arduino IDE](https://www.arduino.cc/en/software)
   - 安装 ESP32 开发板支持（Board Manager → esp32 by Espressif）
   - 安装依赖库：`Adafruit_GFX`、`Adafruit_SH110X`、`ArduinoJson`
3. 选择开发板：`ESP32C3 Dev Module`
4. 连接 ESP32-C3 到电脑 USB
5. 点击"上传"烧录固件
6. 烧录完成后自动重启

> 💡 **提示**：烧录完成后，后续更新可通过 OTA 无线升级，无需 USB 线。

### 第二步：首次开机

烧录完成后，设备会自动重启：

1. **屏幕亮起** — 显示屏保页面（缓存余额 + 时间）
2. **LED 闪烁** — 短暂连接 WiFi 获取时间，完成后自动断开
3. **离线模式** — 首次没有 WiFi 凭据，设备跳过连接，直接显示缓存页面

> ⚠️ 首次开机**不会自动弹出配网页面**，需要手动进入。

### 第三步：配网（连接 WiFi）

1. **唤醒设备** — 按 `GP3` 唤醒键
2. **进入菜单** — 按 `GP6` 旋钮确认键进入主菜单
3. **选择 Settings** — 旋转旋钮选择 `Settings`，按下确认
4. **选择 WiFi Setup** — 旋转旋钮选择 `WiFi Setup`，按下确认
5. **连接热点** — 设备会开启 WiFi 热点：
   - **热点名称**：`ESP32-Elec`
   - **热点密码**：`12345678`
6. **打开配网页面** — 手机连接热点后，浏览器会自动弹出配网页面
   - 如果没有自动弹出，手动访问 `192.168.4.1`
7. **填写信息** — 在网页中填写：
   - WiFi 名称（SSID）
   - WiFi 密码
   - Server酱 SendKey（可选，用于微信推送）
8. **保存** — 点击保存，设备自动重启并连接 WiFi

### 第四步：验证连接

配网成功后：

1. **LED 闪烁** — WiFi 连接中（仅在查询/推送时短暂连接，完成后自动断开）
2. **屏保显示余额** — 30 秒无操作后进入屏保，显示缓存余额和时间

> 💡 **省电设计**：设备默认不保持 WiFi 连接，仅在查询电费或推送时短暂连网，完成后立即断开。日常使用靠缓存数据运行，无需持续联网。

### 配网失败？

如果配网失败（如密码错误），设备会自动重启并进入离线模式。重新进入 WiFi Setup 即可再次配网。

### OTA 无线升级

首次烧录后，后续固件更新无需 USB 线，通过 WiFi 无线升级：

1. 设备需已连接 WiFi
2. 进入菜单 → Settings → OTA Update
3. 设备会开启 OTA 服务页面
4. 浏览器访问设备 IP 地址，上传 `.bin` 固件文件
5. 等待上传完成，设备自动重启

## 菜单操作

### 按键说明

| 按键 | 功能 |
|------|------|
| 旋钮旋转 | 上下选择菜单项 |
| 旋钮按下 (GP6) | 确认/进入 |
| 返回键 (GP10) | 返回上一级 |
| 唤醒键 (GP3) | 唤醒设备/切换屏保 |

### 菜单结构

```
主菜单
├── Query Now          — 立即查询电费
├── Settings
│   ├── Building       — 设置楼栋号（默认46）
│   ├── Room           — 设置房间号（默认416）
│   ├── Default        — 恢复默认设置
│   ├── WiFi Setup     — 配置 WiFi 连接
│   └── OTA Update     — 固件升级
├── Query Timer
│   ├── Enable         — 开启/关闭定时查询
│   ├── Frequency      — 每日/每周
│   └── Day            — 每周几（仅每周模式）
├── Push Settings
│   ├── Enable         — 开启/关闭推送
│   ├── Frequency      — 每日/每周
│   └── Day            — 每周几（仅每周模式）
└── About              — 设备信息
```

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
- WiFiManager

## 常见问题

### Q: 设备一直显示 "Offline"？

A: 设备没有保存 WiFi 凭据。进入菜单 → Settings → WiFi Setup 进行配网。

### Q: 配网页面打不开？

A: 确保手机连接的是 `ESP32-Elec` 热点（密码 `12345678`）。如果浏览器没有自动弹出，手动访问 `192.168.4.1`。

### Q: 查询显示 "Error"？

A: 检查楼栋号和房间号是否正确（菜单 → Settings → Building/Room）。确保设备在校园网环境下。

### Q: 如何更新固件？

A: 进入菜单 → Settings → OTA Update，通过浏览器上传新的 `.bin` 文件。

### Q: 休眠后如何唤醒？

A: 按 `GP3` 唤醒键。

## License

MIT License
