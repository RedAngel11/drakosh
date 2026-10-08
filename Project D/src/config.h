#pragma once

// ===== СВЕТ =====
#define PIN_LED_STRIP 18       // Освободили 17 для динамика!
#define NUM_LEDS 10            // Только 10 светодиодов
#define BRIGHTNESS 127         // 50% яркости (максимум 255)

// ===== УПРАВЛЕНИЕ =====
#define PIN_BUTTON 0           // Кнопка BOOT

// ===== PCA9685 =====
#define PCA9685_ADDRESS 0x40

// ===== I2S ДИНАМИК (MAX98357A) =====
#define I2S_OUT_BCK    15
#define I2S_OUT_WS     16
#define I2S_OUT_DATA   17

// ===== I2S МИКРОФОН (INMP441) =====
#define I2S_IN_BCK     39
#define I2S_IN_WS      38
#define I2S_IN_DATA    37

// ===== ПОРОГ ГОЛОСА =====
#define VOICE_THRESHOLD 300