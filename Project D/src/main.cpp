#include <Arduino.h>
#include "config.h"
#include "network_client.h"
#include "command_handler.h"
#include "engines/light_engine.h"
#include "engines/audio_engine.h"
#include "engines/display_engine.h"
#include "engines/servo_engine.h"

NetworkClient network;
LightEngine lightEngine;
AudioEngine audioEngine;
DisplayEngine displayEngine;
ServoEngine servoEngine;
CommandHandler cmdHandler;

int lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;
bool isVoiceActive = false;
unsigned long voiceDebounceTime = 0;

void setup() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // 1. Сначала "оживляем лицо" — чтобы не было чёрного экрана
    displayEngine.begin();
    displayEngine.showState(EyeState::SLEEP);
    lightEngine.begin();
    lightEngine.setEmotion(Emotion::SLEEP, true);

    audioEngine.begin();
    servoEngine.begin();

    // 2. Подключение к сети (через captive portal при первом запуске)
    network.begin();

    cmdHandler.setEngines(&lightEngine, &audioEngine, &displayEngine, &servoEngine);

    // 3. Пробуждение
    if (WiFi.status() == WL_CONNECTED) {
        displayEngine.showState(EyeState::CALM);
        lightEngine.setEmotion(Emotion::CALM);
    }
}

void loop() {
    // 1. Опрос реле
    String command = network.pollCommand();
    if (command != "none") cmdHandler.execute(command);

    // 2. Кнопка BOOT
    int currentReading = digitalRead(PIN_BUTTON);
    if (currentReading != lastButtonReading) lastDebounceTime = millis();
    if ((millis() - lastDebounceTime) > 50) {
        if (currentReading == LOW && lastButtonReading == HIGH) {
            audioEngine.playWavFile("/hello.wav");
            lightEngine.setMouthPink(2000);
            displayEngine.showState(EyeState::HAPPY);
        }
    }
    lastButtonReading = currentReading;

    // 3. Микрофон
    if (audioEngine.checkVoiceActivity()) {
        if (!isVoiceActive) {
            lightEngine.setEmotion(Emotion::VOICE_DETECTED);
            displayEngine.showState(EyeState::HAPPY);
            isVoiceActive = true;
            voiceDebounceTime = millis();
        }
    } else {
        if (isVoiceActive && (millis() - voiceDebounceTime > 1000)) {
            lightEngine.setEmotion(Emotion::CALM);
            displayEngine.showState(EyeState::CALM);
            isVoiceActive = false;
        }
    }

    // 4. Обновления (свет, радуга, моргание)
    lightEngine.update();
    displayEngine.update();

    delay(10);
}