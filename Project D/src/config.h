#pragma once

// ===== СВЕТ =====
#define PIN_LED_MOUTH 2   // Пин для рта (РОЗОВЫЙ)
#define PIN_LED_BELLY 18  // Пин для живота
#define NUM_LEDS_MOUTH 6  // Используем 6 из 8
#define NUM_LEDS_BELLY 10 // Используем 10 из 52
#define BRIGHTNESS 127

// ===== УПРАВЛЕНИЕ =====
#define PIN_BUTTON 0

// ===== PCA9685 =====
#define PCA9685_ADDRESS 0x40
#define SERVO_COUNT 5

// Константы углов для MG90S (примерные значения, калибруются на практике)
#define SERVO_0   130
#define SERVO_90  375
#define SERVO_180 620

// ===== I2S ДИНАМИК (MAX98357A) =====
#define I2S_OUT_BCK 15
#define I2S_OUT_WS 16
#define I2S_OUT_DATA 17

// ===== I2S МИКРОФОН (INMP441) =====
#define I2S_IN_BCK 39
#define I2S_IN_WS 38
#define I2S_IN_DATA 37

// ===== ПОРОГ ГОЛОСА =====
#define VOICE_THRESHOLD 300

// ===== СЕТЬ =====
// WiFi credentials убраны — теперь через captive portal
#define RELAY_PULL_URL "https://helloesp32.ksushat75.workers.dev/box/pull"
#define RELAY_STATUS_URL "https://helloesp32.ksushat75.workers.dev/box/status"

// ===== ДИСПЛЕИ =====
#define DISPLAY_BL_PIN    6   // подсветка (теперь через ШИМ)
#define DISPLAY_LEFT_CS  10
#define DISPLAY_RIGHT_CS  9
#define DISPLAY_DC        8
#define DISPLAY_RST       7