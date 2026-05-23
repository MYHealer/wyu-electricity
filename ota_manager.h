// ==================== ota_manager.h
// Web OTA 固件更新：ESP32 自建 AP，手机/电脑直连上传
#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "globals.h"

WebServer otaServer(80);

// ==================== OTA 页面 HTML ====================
const char* otaPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 OTA Update</title>
<style>
body{font-family:Arial,sans-serif;max-width:400px;margin:40px auto;padding:20px;background:#1a1a2e;color:#e0e0e0}
h2{text-align:center;color:#00d4ff}
.box{background:#16213e;border-radius:12px;padding:30px;box-shadow:0 4px 20px rgba(0,0,0,0.3)}
input[type=file]{width:100%;padding:10px;margin:15px 0;background:#0f3460;border:1px solid #00d4ff;border-radius:8px;color:#fff;font-size:14px}
input[type=file]::-webkit-file-upload-button{background:#00d4ff;color:#1a1a2e;border:none;padding:6px 12px;border-radius:6px;cursor:pointer}
button{width:100%;padding:12px;background:#00d4ff;color:#1a1a2e;border:none;border-radius:8px;font-size:16px;font-weight:bold;cursor:pointer;margin-top:10px}
button:hover{background:#00b8d4}
button:disabled{background:#555;cursor:not-allowed}
#bar{width:100%;height:20px;background:#0f3460;border-radius:10px;margin-top:15px;overflow:hidden;display:none}
#bar div{height:100%;background:linear-gradient(90deg,#00d4ff,#00ff88);width:0%;transition:width 0.3s;border-radius:10px}
#msg{text-align:center;margin-top:15px;font-size:14px;min-height:20px}
.ok{color:#00ff88}.err{color:#ff4444}
</style>
</head>
<body>
<h2>OTA Update</h2>
<div class="box">
<input type="file" id="fw" accept=".bin">
<button id="btn" onclick="doUpload()">Upload</button>
<div id="bar"><div id="prog"></div></div>
<div id="msg"></div>
</div>
<script>
function doUpload(){
var f=document.getElementById('fw').files[0];
if(!f){alert('Select .bin file first');return;}
var btn=document.getElementById('btn');
var bar=document.getElementById('bar');
var prog=document.getElementById('prog');
var msg=document.getElementById('msg');
btn.disabled=true;bar.style.display='block';
msg.className='';msg.textContent='Uploading...';
var fd=new FormData();fd.append('firmware',f);
var xhr=new XMLHttpRequest();
xhr.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);prog.style.width=p+'%';msg.textContent='Uploading... '+p+'%';}};
xhr.onload=function(){if(xhr.status==200){msg.className='ok';msg.textContent='Done! Rebooting...';}else{msg.className='err';msg.textContent='Error: '+xhr.responseText;btn.disabled=false;}};
xhr.onerror=function(){msg.className='err';msg.textContent='Network error';btn.disabled=false;};
xhr.open('POST','/update');xhr.send(fd);
}
</script>
</body>
</html>
)rawliteral";

// ==================== 处理函数 ====================
void handleOtaRoot() {
    otaServer.send(200, "text/html", otaPage);
}

void handleOtaUpdate() {
    HTTPUpload& upload = otaServer.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] Update start: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("[OTA] Update success: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

void handleOtaEnd() {
    otaServer.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    if (!Update.hasError()) {
        Serial.println("[OTA] Rebooting in 1s...");
        delay(1000);
        ESP.restart();
    }
}

// ==================== 启动 OTA（AP 模式） ====================
void startOtaServer() {
    // 断开已有 WiFi，自建 AP
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_ELEC", "12345678");
    delay(200);

    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[OTA] AP started: ESP32_ELEC  IP: %s\n", apIP.toString().c_str());

    otaServer.on("/", HTTP_GET, handleOtaRoot);
    otaServer.on("/update", HTTP_POST, handleOtaEnd, handleOtaUpdate);
    otaServer.begin();
    Serial.printf("[OTA] Web server on port 80\n");
}

// ==================== 处理 OTA 请求（loop 中调用） ====================
void handleOtaLoop() {
    otaServer.handleClient();
}

#endif
