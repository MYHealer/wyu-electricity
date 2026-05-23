// ==================== encoder.h ====================
// 旋转编码器（软件中断 + 四倍频查表法）
#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include "config.h"

// 四倍频状态查表：AB 两相各变化一次都计数
const int8_t encoderLookup[16] = {
     0,  1, -1,  0,
    -1,  0,  0,  1,
     1,  0,  0, -1,
     0, -1,  1,  0
};

volatile uint8_t old_AB = 0;
volatile int32_t encoderPos = 0;
volatile bool encoderReady = false;

void IRAM_ATTR handleEncoder() {
    uint8_t current_AB = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
    uint8_t idx = (old_AB << 2) | current_AB;
    old_AB = current_AB;
    int8_t delta = encoderLookup[idx];
    if (delta != 0) {
        encoderPos += delta;
        encoderReady = true;
    }
}

void encoderSetup() {
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    old_AB = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), handleEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), handleEncoder, CHANGE);
}

int readEncoder() {
    static int8_t remain = 0;
    noInterrupts();
    int32_t pos = encoderPos;
    encoderPos = 0;
    encoderReady = false;
    interrupts();
    remain += pos;
    if (remain >= 4) { remain -= 4; return -1; }
    if (remain <= -4) { remain += 4; return 1; }
    return 0;
}

// ==================== 按钮 ====================
bool readBtn(uint8_t pin) {
    static uint8_t lastState[11] = {1,1,1,1,1,1,1,1,1,1,1};
    uint8_t state = digitalRead(pin);
    bool pressed = (lastState[pin] == 1 && state == 0);
    lastState[pin] = state;
    return pressed;
}

#endif // ENCODER_H
