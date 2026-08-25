#pragma once

// ===== I2C =====
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

// ===== I2S Микрофон (INMP441) =====
#define PIN_MIC_SCK 5
#define PIN_MIC_WS  6
#define PIN_MIC_SD  7

// ===== I2S Динамик (MAX98357A) =====
#define PIN_SPK_BCLK 15
#define PIN_SPK_LRC  16
#define PIN_SPK_DIN  17

// ===== LED лента живота =====
#define PIN_LED_STRIP 4
#define LED_COUNT 1

// ===== PCA9685 =====
#define PCA9685_ADDRESS 0x40
#define PCA9685_FREQ 50

// ===== Сервоприводы MG90S =====
// Каналы PCA9685 (0-15)
#define SERVO_CHANNEL_LEFT_WING  0
#define SERVO_CHANNEL_RIGHT_WING 1
#define SERVO_CHANNEL_LEFT_LEG   2
#define SERVO_CHANNEL_RIGHT_LEG  3
#define SERVO_CHANNEL_HEAD_YAW   4
#define SERVO_CHANNEL_HEAD_PITCH 5
#define SERVO_CHANNEL_TAIL       6

// Параметры серв MG90S
#define SERVO_MIN_PULSE 102    // 0.5 мс (0°)
#define SERVO_MAX_PULSE 512    // 2.5 мс (180°)
#define SERVO_CENTER_PULSE 306 // 1.5 мс (90°)

// ===== Telegram Relay =====
#define RELAY_HOST "helloesp32.ksushat75.workers.dev"
#define RELAY_PORT 443

// ===== WiFi Manager =====
#define WIFI_AP_SSID "Drakoshka_Setup"
#define WIFI_AP_PASS "drakoshka123"
#define WIFI_CONFIG_TIMEOUT 180

// ===== Timings =====
#define RECONNECT_INTERVAL 5000
#define BOX_POLL_INTERVAL 1500
#define SERVO_MOVE_DELAY 15