# 五邑大学宿舍电费查询终端

ESP32-C3 SuperMini + SH1106 OLED(128x64) + 旋钮 + 按钮，直连校园网查询宿舍电费余额。

## 文件说明

| 文件 | 说明 |
|------|------|
| `esp32_electricity.ino` | 主程序文件 |
| `config_portal.h` | WiFi 配置门户模块（AP 热点配网） |

**⚠️ 重要**：`config_portal.h` 必须和 `esp32_electricity.ino` 放在**同一个文件夹**下，否则编译会报错找不到头文件。

## 硬件

- ESP32-C3 SuperMini
- SH1106 OLED 128x64 I2C
- 旋转编码器 + 2个按钮

## 接线

| 功能 | GPIO |
|------|------|
| OLED SDA | 4 |
| OLED SCL | 5 |
| 旋钮 A | 0 |
| 旋钮 B | 1 |
| 旋钮按下 | 6 |
| CONFIRM | 3 |
| BACK | 10 |

## 功能

- 实时查询电费余额
- 旋钮+按钮操作菜单
- WiFi 配置（AP 热点）
- 低电量/余额预警推送
- 定时自动查询

## 烧录

1. 下载本仓库所有文件，确保 `config_portal.h` 和 `esp32_electricity.ino` 在同一文件夹
2. Arduino IDE 打开 `esp32_electricity.ino`
3. 选择开发板：ESP32C3 Dev Module
4. 安装所需库：Adafruit GFX Library、Adafruit SH110X、ArduinoJson
5. 编译上传

## License

GPL-3.0
