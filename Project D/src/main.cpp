#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

#include "config.h"
#include "light_engine.h"
#include "audio_engine.h"

// Пины экранов и I2C
#define I2C_SDA 47
#define I2C_SCL 48
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS_LEFT   10
#define TFT_CS_RIGHT  9
#define TFT_DC        8
#define TFT_RST       7
#define TFT_BL        6

// Глобальные объекты (теперь NUM_LEDS и PCA9685_ADDRESS видны из config.h)
LightEngine lightEngine;
AudioEngine audio;
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADDRESS);
Adafruit_GC9A01A displayLeft(TFT_CS_LEFT, TFT_DC, TFT_RST);
Adafruit_GC9A01A displayRight(TFT_CS_RIGHT, TFT_DC, TFT_RST);

// Переменные для кнопки
bool lastButtonState = HIGH;
bool currentButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// Состояние микрофона
bool isVoiceActive = false;
unsigned long voiceDebounceTime = 0;

// =====================================================
// === ФУНКЦИИ УПРАВЛЕНИЯ ==============================
// =====================================================
void moveServoToAngle(int angle) {
    int pwmValue = map(angle, 0, 180, 150, 550);
    pwm.setPWM(0, 0, pwmValue);
}

void drawEye(Adafruit_GC9A01A &display, int pupilX, uint16_t irisColor) {
    int cx = 120, cy = 120; // SCREEN_W / 2
    display.fillScreen(GC9A01A_BLACK);
    display.fillCircle(cx, cy, 100, GC9A01A_WHITE); // EYE_RADIUS
    int irisX = cx + pupilX * 40;
    display.fillCircle(irisX, cy, 45, irisColor);   // PUPIL_RADIUS + 10
    display.fillCircle(irisX, cy, 35, GC9A01A_BLACK); // PUPIL_RADIUS
    display.fillCircle(irisX - 10, cy - 10, 8, GC9A01A_WHITE); // Блик
}

void updateEyes(int pupilX, uint16_t irisColor) {
    drawEye(displayLeft, pupilX, irisColor);
    drawEye(displayRight, pupilX, irisColor);
}

void applySystemState() {
    moveServoToAngle(90);
    lightEngine.setEmotion(Emotion::CALM);
    updateEyes(0, GC9A01A_GREEN);
}

// =====================================================
// === ПРОВЕРКИ ========================================
// =====================================================
bool checkButton() {
    int reading = digitalRead(PIN_BUTTON);
    if (reading != lastButtonState) lastDebounceTime = millis();
    if ((millis() - lastDebounceTime) > 50) {
        if (reading != currentButtonState) {
            currentButtonState = reading;
            if (currentButtonState == LOW) return true; // Нажатие (LOW, т.к. INPUT_PULLUP)
        }
    }
    lastButtonState = reading;
    return false;
}

bool checkI2CConnection() {
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0 && address == PCA9685_ADDRESS) return true;
    }
    return false;
}

// =====================================================
// === SETUP & LOOP ====================================
// =====================================================
void setup() {
    Serial.begin(115200);
    Serial.println("🚀 Запуск Дракошки...");

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // 1. Инициализация модулей
    lightEngine.begin(PIN_LED_STRIP, NUM_LEDS);
    lightEngine.setEmotion(Emotion::ALARM, true); // Тест ленты (красный)
    delay(500);

    displayLeft.begin();
    displayRight.begin();
    displayLeft.setRotation(0);
    displayRight.setRotation(0);

    Wire.begin(I2C_SDA, I2C_SCL);
    if (!checkI2CConnection()) {
        Serial.println("❌ PCA9685 не найден!");
        while (1) { delay(1000); }
    }

    pwm.begin();
    pwm.setOscillatorFrequency(27000000);
    pwm.setPWMFreq(PCA9685_FREQ);

    audio.begin();

    applySystemState();
    Serial.println("✅ Система готова. Нажми BOOT для звука, скажи что-нибудь для света.");
}

void loop() {
    // 1. Обработка кнопки BOOT -> проигрывание звука
    if (checkButton()) {
        Serial.println("🔊 Кнопка нажата! Проигрываю звук...");
        audio.playWavFile("/hello.wav"); 
        // Если файла нет, сработает фолбэк на playTone(440, 500) из audio_engine.h
    }

    // 2. Обработка микрофона -> реакция ленты
    if (audio.checkVoiceActivity()) {
        if (!isVoiceActive) {
            Serial.println("🎤 Голос обнаружен!");
            lightEngine.setEmotion(Emotion::VOICE_DETECTED);
            isVoiceActive = true;
            voiceDebounceTime = millis();
        }
    } else {
        // Если голос пропал, ждем 1 секунду и возвращаем спокойный цвет (защита от моргания)
        if (isVoiceActive && (millis() - voiceDebounceTime > 1000)) {
            lightEngine.setEmotion(Emotion::CALM);
            isVoiceActive = false;
        }
    }

    // 3. Обновление плавных переходов света и задержка
    lightEngine.update();
    delay(10);
}