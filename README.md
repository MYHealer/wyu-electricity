# 五邑大学宿舍电费查询终端

ESP32-C3 SuperMini + SH1106 OLED(128x64) + 旋钮 + 按钮，直连校园网查询宿舍电费余额。

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

Arduino IDE，选择 ESP32-C3 开发板，安装所需库后编译上传。

## License

GPL-3.0
