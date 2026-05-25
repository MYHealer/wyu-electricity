# 更新日志

## v9.2 (2026-05-25)

### 固件更新整合
- **移除独立 OTA 菜单项**，固件更新功能合并到 WiFi Setup 的 Web 服务器中
- 删除 `ota_manager.h`，OTA 逻辑统一由 `wifi_manager.h` 中的 HTTP 上传接口处理
- 更新流程：进入 WiFi Setup → 浏览器访问设备 IP → 切换到"固件更新"标签页 → 上传 .bin 文件
- 菜单层级简化：Settings 下从 5 项减为 4 项（Building / Room / Default / WiFi Setup）

### 文档更新
- README 中 OTA 相关描述同步更新，反映新的固件更新入口

---

## v9.1 (2026-05-23)

### Deep Sleep 电流优化
- **休眠电流从 5.6mA 降至 ~9μA**
- 新增 SH1106 OLED 硬件关屏指令（`0xAE` Display OFF），解决仅清屏但芯片仍工作导致的漏电
- 新增 `Wire.end()` 释放 I2C 总线，防止上拉电阻持续耗电
- 新增 sleep 前所有 GPIO 拉低（SDA/SCL/ENC_A/ENC_B/ENC_BTN/BTN_BACK），仅保留 GPIO3 作为唤醒源
- LED 灭灯逻辑统一（HIGH = 灭）

### 代码优化
- 清理 sleep_manager.h 中冗余的 `gpio_set_direction` 调用
- 唤醒源配置改用 `esp_deep_sleep_enable_gpio_wakeup`（ESP-IDF 新 API）

---

## v9.0

### OTA 更新
- 新增 Web OTA 固件更新功能，通过浏览器上传 .bin 文件即可更新
- 菜单新增 "OTA Update" 选项

### 定时任务
- 定时器唤醒自动执行查询 + 推送
- 支持每日/每周推送模式
- 支持每日/每周查询模式

### Web 配网
- 首次启动或 WiFi 连接失败时自动进入 AP 模式
- 手机扫码连接后在网页中配置 WiFi

### 菜单系统
- 旋钮 + 按键操作的完整菜单
- 支持楼栋号、房间号编辑
- 支持推送开关、频率设置
- 屏保显示余额和时间

---

## v8.0

### 初始版本
- ESP32-C3 + SH1106 OLED
- 旋转编码器菜单操作
- 五邑大学电费 API 对接
- Server酱微信推送
- NVS 配置持久化
- Deep Sleep 省电（30 秒无操作休眠）
