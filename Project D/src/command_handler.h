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

        if (cmd == "greet") {
            _audio->playWavFile("/hello.wav");
            _light->setEmotion(Emotion::JOY);
            _display->showState(EyeState::HAPPY);
        } 
        else if (cmd == "bye") {
            _audio->playWavFile("/bye.wav");
            _light->setEmotion(Emotion::SLEEP);
            _display->showState(EyeState::SLEEP);
        } 
        else if (cmd == "sing") {
            _audio->playWavFile("/sing.wav");
            _display->showState(EyeState::HAPPY);
        }
        else if (cmd == "dance") {
            _servo->dance();
            _light->setEmotion(Emotion::JOY);
        } 
        else if (cmd == "blink") {
            _display->blink();
        }
        else if (cmd == "rainbow") {
            _light->setEmotion(Emotion::JOY); 
        }
    }

private:
    LightEngine* _light;
    AudioEngine* _audio;
    DisplayEngine* _display;
    ServoEngine* _servo;
};