#pragma once
// ===== ДРАКОШКА · пины нашей платы =====

// --- PCA9685 ---
#define I2C_SDA 4
#define I2C_SCL 5
#define PCA9685_ADDRESS 0x40
#define SERVO_FREQ 50
#define SERVO_OSC  27000000
#define SERVO_0   130
#define SERVO_90  375
#define SERVO_180 620
#define SERVO_CH_A 0
#define SERVO_CH_B 1
#define SERVO_NEUTRAL_DEG 90
#define GREET_DELTA_DEG   45
#define SERVO_MOVE_MS    350
#define SERVO_STAGGER_MS 100
#define SERVO_MIN_DEG 10
#define SERVO_MAX_DEG 170

// --- ЛЕНТЫ ---
#define PIN_LED_MOUTH   2
#define PIN_LED_BELLY  18
#define NUM_LEDS_MOUTH  6
#define NUM_LEDS_BELLY 15
#define BELLY_BRIGHTNESS 45
#define MOUTH_BRIGHT_MIN 35
#define MOUTH_BRIGHT_MAX 170
#define RGB_LED_PIN 48
#define STATUS_LED_PIN RGB_LED_PIN

// --- МИКРОФОН INMP441 ---
#define MIC_SWAP_SCK_SD 0        // 0: SCK=39/SD=37 (плата); 1: SCK=37/SD=39 (коммит 12.11)
#if MIC_SWAP_SCK_SD
  #define PIN_MIC_SCK 37
  #define PIN_MIC_SD  39
#else
  #define PIN_MIC_SCK 39
  #define PIN_MIC_SD  37
#endif
#define PIN_MIC_WS 38
#define USE_MIC 1

// --- ЗВУК PAM8403 ---
#define PIN_SPK 21
#define AUDIO_VOLUME 80
#define AUDIO_TARGET_PEAK 70

// --- ГОЛОСОВОЙ ТРИГГЕР «привет, Дракоша» ---
#define VOICE_TRIG_ENABLE 1
#define VOICE_TRIG_MODE   1      // 1 = ритм 2+3 слога с паузой; 0 = любая речь 3..6 слогов
#define VT_GAIN          64      // усиление уровня: lvl = peak * GAIN / 256
#define VT_ON_MIN        20      // минимальный порог слога (шкала 0..255)
#define VT_SYL_MIN_MS    40
#define VT_SYL_MAX_MS    280
#define VT_GAP_WORD_MS   180     // пауза между «привет» и «Дракоша»
#define VT_END_MS        420     // столько тишины = конец фразы
#define VT_SPEECH_MIN_MS 500
#define VT_SPEECH_MAX_MS 2000
#define VT_MIN_SYL 3
#define VT_MAX_SYL 6
#define VT_CAL_MS   600          // калибровка шума после старта
#define VT_COOLDOWN_MS 6000

// --- ДИСПЛЕИ (SPI: MOSI 11, SCK 12) ---
#define DISPLAY_LEFT_CS  10
#define DISPLAY_RIGHT_CS  9
#define DISPLAY_DC        8
#define DISPLAY_RST       7
#define DISPLAY_BL_PIN   -1      // BL не разведён на GPIO (подсветка на 3V3)
#define EYES_SHARED_CS    1      // 1 = оба CS на GPIO10; 0 = раздельные CS 9 и 10

// --- СЦЕНАРИЙ ---
#define GREET_MOVE_WAIT_MS 450
#define GREET_TAIL_MS      150
#define GREET_TIMEOUT_MS 15000
#define PIN_BUTTON 0

// --- СЕТЬ ---
#define RELAY_PULL_URL   "https://helloesp32.ksushat75.workers.dev/box/pull"
#define RELAY_STATUS_URL "https://helloesp32.ksushat75.workers.dev/box/status"
#define USE_WIFI_MANAGER 1
#define POLL_INTERVAL_MS 800
// #define WIFI_SSID "сеть"
// #define WIFI_PASS "пароль"