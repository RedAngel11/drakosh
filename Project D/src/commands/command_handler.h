#pragma once
#include <Arduino.h>
#include "../brain/brain.h"
#include "../drivers/servo_controller.h"

class CommandHandler {
public:
    void begin(Brain* brain, ServoController* servo) {
        _brain = brain;
        _servo = servo;
    }

    void handleCommand(const String& cmd) {
        Serial.println("📨 Команда: " + cmd);
        
        if (_brain) _brain->activityDetected();

        // Команды от Telegram
        if (cmd.indexOf("light_on") >= 0 || cmd.indexOf("joy") >= 0) {
            if (_brain) _brain->setMode(BrainMode::JOY);
        }
        else if (cmd.indexOf("light_off") >= 0 || cmd.indexOf("off") >= 0) {
            if (_brain) _brain->setMode(BrainMode::OFF);
        }
        else if (cmd.indexOf("calm") >= 0) {
            if (_brain) _brain->setMode(BrainMode::CALM);
        }
        else if (cmd.indexOf("support") >= 0) {
            if (_brain) _brain->setMode(BrainMode::SUPPORT);
        }
        else if (cmd.indexOf("alarm") >= 0 || cmd.indexOf("deadline") >= 0) {
            if (_brain) _brain->setMode(BrainMode::ALARM);
        }
        else if (cmd.indexOf("sleep") >= 0) {
            if (_brain) _brain->setMode(BrainMode::SLEEP);
        }
        // Команды для серв
        else if (cmd.indexOf("wave") >= 0) {
            if (_servo) _servo->waveWings();
        }
        else if (cmd.indexOf("center") >= 0) {
            if (_servo) _servo->setAllServosToCenter();
        }
        
        Serial.println("✅ Команда выполнена");
    }

    void handleSerialCommand() {
        if (!Serial.available()) return;
        
        String s = Serial.readStringUntil('\n');
        s.trim();
        
        if (s.length() == 0) return;

        if (_brain) _brain->activityDetected();

        if (s.indexOf("радость") >= 0 || s.indexOf("joy") >= 0) {
            if (_brain) _brain->setMode(BrainMode::JOY);
        }
        else if (s.indexOf("спокой") >= 0 || s.indexOf("calm") >= 0) {
            if (_brain) _brain->setMode(BrainMode::CALM);
        }
        else if (s.indexOf("поддерж") >= 0 || s.indexOf("support") >= 0) {
            if (_brain) _brain->setMode(BrainMode::SUPPORT);
        }
        else if (s.indexOf("дедлайн") >= 0 || s.indexOf("тревога") >= 0 || s.indexOf("alarm") >= 0) {
            if (_brain) _brain->setMode(BrainMode::ALARM);
        }
        else if (s.indexOf("сон") >= 0 || s.indexOf("sleep") >= 0) {
            if (_brain) _brain->setMode(BrainMode::SLEEP);
        }
        else if (s.indexOf("выкл") >= 0 || s.indexOf("off") >= 0) {
            if (_brain) _brain->setMode(BrainMode::OFF);
        }
        else if (s.indexOf("маш") >= 0 || s.indexOf("wave") >= 0) {
            if (_servo) _servo->waveWings();
        }
        else if (s.indexOf("центр") >= 0 || s.indexOf("center") >= 0) {
            if (_servo) _servo->setAllServosToCenter();
        }
        else if (s.indexOf("test") >= 0) {
            Serial.println("🔍 Тест системы:");
            Serial.println("  Wi-Fi: " + String(WiFi.status() == WL_CONNECTED ? "✅" : "❌"));
            Serial.println("  IP: " + WiFi.localIP().toString());
            Serial.println("  Режим: " + String(_brain ? (int)_brain->getCurrentMode() : -1));
        }
    }

private:
    Brain* _brain = nullptr;
    ServoController* _servo = nullptr;
};