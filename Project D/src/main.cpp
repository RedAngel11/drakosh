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
    // 0. Кнопка
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // 1. Сеть
    network.begin();

    // 2. Движки
    lightEngine.begin(PIN_LED_STRIP, NUM_LEDS);
    lightEngine.setEmotion(Emotion::CALM, true);
    
    audioEngine.begin();
    displayEngine.begin();
    servoEngine.begin();

    // 3. Связываем обработчик команд с движками
    cmdHandler.setEngines(&lightEngine, &audioEngine, &displayEngine, &servoEngine);
}

void loop() {
    // 1. Сетевой опрос
    String command = network.pollCommand();
    if (command != "none") {
        cmdHandler.execute(command);
    }

    // 2. Обработка кнопки BOOT
    int currentReading = digitalRead(PIN_BUTTON);
    if (currentReading != lastButtonReading) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > 50) {
        if (currentReading == LOW && lastButtonReading == HIGH) {
            audioEngine.playWavFile("/hello.wav"); 
            displayEngine.showState(EyeState::HAPPY);
        }
    }
    lastButtonReading = currentReading;

    // 3. Обработка микрофона
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

    // 4. Фоновые задачи движков
    lightEngine.update();
    displayEngine.update();
    
    delay(10);
}