#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

#include "config.h"
#include "light_engine.h"
#include "audio_engine.h"

// Глобальные объекты
LightEngine lightEngine;
AudioEngine audio;
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADDRESS);
Adafruit_GC9A01A displayLeft(10, 8, 7);  // CS, DC, RST
Adafruit_GC9A01A displayRight(9, 8, 7);

// Переменные для кнопки
int lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;

// Состояние микрофона
bool isVoiceActive = false;
unsigned long voiceDebounceTime = 0;

void setup() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(6, OUTPUT);        // TFT_BL
    digitalWrite(6, HIGH);

    // 1. Свет
    lightEngine.begin(PIN_LED_STRIP, NUM_LEDS);
    lightEngine.setEmotion(Emotion::CALM, true);

    // 2. Экраны
    displayLeft.begin();
    displayRight.begin();
    displayLeft.setRotation(0);
    displayRight.setRotation(0);
    displayLeft.fillScreen(GC9A01A_BLACK);
    displayRight.fillScreen(GC9A01A_BLACK);

    // 3. I2C и Серво (БЕЗ while(1)! Если нет - просто идем дальше)
    Wire.begin(47, 48);
    bool i2cFound = false;
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0 && address == PCA9685_ADDRESS) {
            i2cFound = true;
            break;
        }
    }
    if (i2cFound) {
        pwm.begin();
        pwm.setOscillatorFrequency(27000000);
        pwm.setPWMFreq(50);
    }

    // 4. Аудио
    audio.begin();
}

void loop() {
    // 1. Обработка кнопки BOOT -> проигрывание звука
    int currentReading = digitalRead(PIN_BUTTON);
    if (currentReading != lastButtonReading) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > 50) {
        if (currentReading == LOW && lastButtonReading == HIGH) {
            audio.playWavFile("/hello.wav"); 
        }
    }
    lastButtonReading = currentReading;

    // 2. Обработка микрофона -> реакция ленты
    if (audio.checkVoiceActivity()) {
        if (!isVoiceActive) {
            lightEngine.setEmotion(Emotion::VOICE_DETECTED);
            isVoiceActive = true;
            voiceDebounceTime = millis();
        }
    } else {
        // Если голос пропал, ждем 1 секунду и возвращаем спокойный цвет
        if (isVoiceActive && (millis() - voiceDebounceTime > 1000)) {
            lightEngine.setEmotion(Emotion::CALM);
            isVoiceActive = false;
        }
    }

    // 3. Обновление плавных переходов света
    lightEngine.update();
    delay(10);
}