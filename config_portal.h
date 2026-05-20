// ==================== config_portal.h ====================
// WiFi 凭据和推送配置
#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

// WiFi 凭据 —— 填写你自己的校园网/热点名称和密码
// 示例: #define WIFI_SSID     "MyWiFi"
// 示例: #define WIFI_PASSWORD "mypassword123"
#define WIFI_SSID     "填写你的WiFi名称"
#define WIFI_PASSWORD "填写你的WiFi密码"

// 默认宿舍号（首次上电或NVS为空时使用）
// 示例: 46栋416室
#define DEFAULT_BUILDING  46
#define DEFAULT_ROOM      416

// Server酱推送密钥（用于低电量/余额预警推送）
// 获取方式: 访问 https://sct.ftqq.com/ 注册后复制 SendKey
// 示例: #define SENDKEY       "SCTxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define SENDKEY       "填写你的Server酱SendKey"

#endif // CONFIG_PORTAL_H
