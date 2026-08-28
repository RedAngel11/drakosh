#pragma once
#include <Arduino.h>
#include "../drivers/servo_controller.h"
#include "../light_engine.h"

// ===== ВАЖНО: enum должен быть ОБЪЯВЛЕН ДО класса Brain =====
enum class BrainMode { 
    CALM, 
    JOY, 
    SUPPORT, 
    ALARM, 
    SLEEP, 
    OFF 
};

class Brain {
public:
    void begin(ServoController* servo, LightEngine* light) {
        _servo = servo;
        _light = light;
        _lastActivity = millis();
        Serial.println("🧠 Мозг Дракошки активирован");
    }

    void update() {
        // Автономное поведение
        unsigned long idleTime = millis() - _lastActivity;
        
        // Если долго ничего не происходило - переходим в спокойный режим
        if (idleTime > 60000 && _currentMode != BrainMode::SLEEP) {
            setMode(BrainMode::SLEEP);
        }
    }

    void setMode(BrainMode mode) {
        if (_currentMode == mode) return;
        
        _currentMode = mode;
        _lastActivity = millis();
        
        switch (mode) {
            case BrainMode::JOY:
                Serial.println("😊 Режим: радость");
                if (_light) _light->setEmotion(Emotion::JOY);
                if (_servo) _servo->waveWings();
                break;
                
            case BrainMode::CALM:
                Serial.println(" Режим: спокойствие");
                if (_light) _light->setEmotion(Emotion::CALM);
                break;
                
            case BrainMode::SUPPORT:
                Serial.println("💚 Режим: поддержка");
                if (_light) _light->setEmotion(Emotion::SUPPORT);
                break;
                
            case BrainMode::ALARM:
                Serial.println("⚠️ Режим: тревога/дедлайн");
                if (_light) _light->setEmotion(Emotion::ALARM);
                break;
                
            case BrainMode::SLEEP:
                Serial.println("😴 Режим: сон");
                if (_light) _light->setEmotion(Emotion::SLEEP);
                break;
                
            case BrainMode::OFF:
                Serial.println("⏹️ Режим: выключен");
                if (_light) _light->setEmotion(Emotion::OFF);
                break;
        }
    }

    void activityDetected() {
        _lastActivity = millis();
        if (_currentMode == BrainMode::SLEEP) {
            setMode(BrainMode::CALM);
        }
    }

    BrainMode getCurrentMode() const { return _currentMode; }

private:
    ServoController* _servo = nullptr;
    LightEngine* _light = nullptr;
    BrainMode _currentMode = BrainMode::CALM;
    unsigned long _lastActivity = 0;
};