#pragma once

#include <Arduino.h>
#include "engines/light_engine.h"
#include "engines/audio_engine.h"
#include "engines/display_engine.h"
#include "engines/servo_engine.h"

class CommandHandler {
public:
    void setEngines(LightEngine* light, AudioEngine* audio, DisplayEngine* display, ServoEngine* servo) {
        _light = light;
        _audio = audio;
        _display = display;
        _servo = servo;
    }

        void execute(String cmd) {
        if (cmd == "none" || cmd == "") return;

        // Команда может прийти с параметром: "change_color:red"
        String name = cmd;
        String param = "";
        int idx = cmd.indexOf(':');
        if (idx > 0) {
            name = cmd.substring(0, idx);
            param = cmd.substring(idx + 1);
            param.trim();
        }

        if (name == "greet") {
            _audio->playWavFile("/hello.wav");
            _light->setEmotion(Emotion::JOY);
            _display->showState(EyeState::HAPPY);
        }
        else if (name == "bye") {
            _audio->playWavFile("/bye.wav");
            _light->setEmotion(Emotion::SLEEP);
            _display->showState(EyeState::SLEEP);
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
        else if (name == "change_color") {
            _light->setColorByName(param);   // неизвестное имя просто игнорируем
        }
        else if (name == "set_brightness") {
            _light->setBrightness((uint8_t)param.toInt());
        }
    }

private:
    LightEngine* _light;
    AudioEngine* _audio;
    DisplayEngine* _display;
    ServoEngine* _servo;
};