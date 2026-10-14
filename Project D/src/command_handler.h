#pragma once
#include <Arduino.h>
#include "engines/light_engine.h"
#include "engines/audio_engine.h"
#include "engines/display_engine.h"
#include "engines/servo_engine.h"

class CommandHandler {
public:
    void setEngines(LightEngine* light, AudioEngine* audio, DisplayEngine* display, ServoEngine* servo) {
        _light = light; _audio = audio; _display = display; _servo = servo;
    }

    void execute(String cmd) {
        if (cmd == "none" || cmd == "") return;

        String name = cmd, param = "";
        int idx = cmd.indexOf(':');
        if (idx > 0) {
            name = cmd.substring(0, idx);
            param = cmd.substring(idx + 1);
            param.trim();
        }

        // Отключаем радугу при любой другой команде
        if (name != "rainbow") _light->setRainbowMode(false);

        if (name == "greet") {
            _audio->playWavFile("/hello.wav");
            _light->setMouthPink(2000);   // РОЗОВЫЙ рот на 2 секунды!
            _display->showState(EyeState::HAPPY);
        }
        else if (name == "bye" || name == "light_sleep") {
            _display->setBacklight(30);   // почти выключено, ~12%
            _display->showState(EyeState::SLEEP, 3600000UL); // автоцикл не мешает спать
            _light->setEmotion(Emotion::SLEEP);
}
        else if (name == "sing") {
            _audio->playWavFile("/sing.wav");
            _display->showState(EyeState::HAPPY);
        }
        else if (name == "dance") {
            _servo->dance();
            _light->setEmotion(Emotion::JOY);
        }
        else if (name == "blink") {
            _display->blink();
        }
        else if (name == "rainbow") {
            _light->setRainbowMode(true);
        }
        else if (name == "change_color") {
            _light->setColorByName(param);
        }
        else if (name == "set_brightness") {
            _light->setBrightness((uint8_t)param.toInt());
        }
        else if (name == "light_joy")     _light->setEmotion(Emotion::JOY);
        else if (name == "light_calm")    _light->setEmotion(Emotion::CALM);
        else if (name == "light_support") _light->setEmotion(Emotion::SUPPORT);
        else if (name == "light_alarm")   _light->setEmotion(Emotion::ALARM);
        else if (name == "light_sleep")   _light->setEmotion(Emotion::SLEEP);
        else if (name == "light_off")     _light->setEmotion(Emotion::OFF);
        else if (name == "wave_wings" || name == "center") {
            _servo->_resetAll();
        }
    }

private:
    LightEngine* _light;
    AudioEngine* _audio;
    DisplayEngine* _display;
    ServoEngine* _servo;
};