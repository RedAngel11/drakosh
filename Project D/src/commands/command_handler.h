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

    // Обработка команд, пришедших от Telegram (через Relay)
    void handleTelegramCommand(const String& cmd) {
        if (cmd.length() == 0 || cmd == "empty") return;
        
        Serial.println("📨 Команда из TG: " + cmd);
        if (_brain) _brain->activityDetected();

        if (cmd.indexOf("light_joy") >= 0 || cmd.indexOf("joy") >= 0) {
            if (_brain) _brain->setMode(BrainMode::JOY);
        }
        else if (cmd.indexOf("light_off") >= 0 || cmd.indexOf("off") >= 0) {
            if (_brain) _brain->setMode(BrainMode::OFF);
        }
        else if (cmd.indexOf("light_calm") >= 0 || cmd.indexOf("calm") >= 0) {
            if (_brain) _brain->setMode(BrainMode::CALM);
        }
        else if (cmd.indexOf("light_support") >= 0 || cmd.indexOf("support") >= 0) {
            if (_brain) _brain->setMode(BrainMode::SUPPORT);
        }
        else if (cmd.indexOf("light_alarm") >= 0 || cmd.indexOf("alarm") >= 0) {
            if (_brain) _brain->setMode(BrainMode::ALARM);
        }
        else if (cmd.indexOf("light_sleep") >= 0 || cmd.indexOf("sleep") >= 0) {
            if (_brain) _brain->setMode(BrainMode::SLEEP);
        }
        else if (cmd.indexOf("wave") >= 0) {
            if (_servo) _servo->waveWings();
        }
        else if (cmd.indexOf("center") >= 0) {
            if (_servo) _servo->setAllServosToCenter();
        }
        else {
            Serial.println("⚠️ Неизвестная команда: " + cmd);
        }
    }

    // Обработка команд из Serial Monitor (для отладки)
    void handleSerialCommand() {
        if (!Serial.available()) return;
        String s = Serial.readStringUntil('\n');
        s.trim();
        if (s.length() == 0) return;

        // Маппинг русских слов из Serial в те же команды
        if (s.indexOf("радость") >= 0) handleTelegramCommand("light_joy");
        else if (s.indexOf("спокой") >= 0) handleTelegramCommand("light_calm");
        else if (s.indexOf("поддерж") >= 0) handleTelegramCommand("light_support");
        else if (s.indexOf("дедлайн") >= 0 || s.indexOf("тревога") >= 0) handleTelegramCommand("light_alarm");
        else if (s.indexOf("сон") >= 0) handleTelegramCommand("light_sleep");
        else if (s.indexOf("выкл") >= 0) handleTelegramCommand("light_off");
        else if (s.indexOf("маш") >= 0) handleTelegramCommand("wave");
        else if (s.indexOf("центр") >= 0) handleTelegramCommand("center");
        else if (s.indexOf("test") >= 0) {
            Serial.println("🔍 Тест: Wi-Fi " + String(WiFi.status() == WL_CONNECTED ? "OK" : "FAIL"));
        }
    }

private:
    Brain* _brain = nullptr;
    ServoController* _servo = nullptr;
};