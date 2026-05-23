// ==================== config_store.h ====================
// NVS 配置读写
#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <Preferences.h>
#include "globals.h"

Preferences prefs;

void loadConfig() {
    prefs.begin("elec", true);  // 只读
    config.building    = prefs.getInt("bld", DEFAULT_BUILDING);
    config.room        = prefs.getInt("room", DEFAULT_ROOM);
    config.pushEnabled = prefs.getBool("push", false);
    config.pushDaily   = prefs.getBool("pushD", true);
        config.pushDay     = prefs.getInt("pushDy", 1);
    config.qryDaily    = prefs.getBool("qryD", true);
    config.qryDay      = prefs.getInt("qryDy", 1);
    strncpy(config.sendkey, prefs.getString("sendkey", SENDKEY).c_str(), sizeof(config.sendkey) - 1);
    if (strlen(config.sendkey) == 0) {
        strncpy(config.sendkey, SENDKEY, sizeof(config.sendkey) - 1);
    }
    prefs.end();
}

void saveConfig() {
    prefs.begin("elec", false);  // 读写
    prefs.putInt("bld", config.building);
    prefs.putInt("room", config.room);
    prefs.putBool("push", config.pushEnabled);
    prefs.putBool("pushD", config.pushDaily);
        prefs.putInt("pushDy", config.pushDay);
    prefs.putBool("qryD", config.qryDaily);
    prefs.putInt("qryDy", config.qryDay);
    prefs.putString("sendkey", config.sendkey);
    prefs.end();
}

// ==================== 缓存查询结果（唤醒后秒显示） ====================
void saveQueryCache() {
    prefs.begin("elec", false);
    prefs.putFloat("bal", balance);
    prefs.putFloat("use", usage);
    prefs.putBool("qok", queryOk);
    prefs.end();
}

void loadQueryCache() {
    prefs.begin("elec", true);
    balance = prefs.getFloat("bal", 0);
    usage   = prefs.getFloat("use", 0);
    queryOk = prefs.getBool("qok", false);
    prefs.end();
}

#endif // CONFIG_STORE_H
