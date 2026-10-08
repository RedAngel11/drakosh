#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include "config.h"

enum class EyeState { CALM, HAPPY, SLEEP, BLINK };

class DisplayEngine {
public:
    void begin() {
        pinMode(6, OUTPUT); // TFT_BL (подсветка)
        digitalWrite(6, HIGH);
        
        left.begin();
        right.begin();
        left.setRotation(0);
        right.setRotation(0);
        
        showState(EyeState::CALM);
    }

    void showState(EyeState state) {
        _currentState = state;
        _isBlinking = false;
        _blinkStartTime = 0;
        _drawEyes(state);
    }

    void blink() {
        _isBlinking = true;
        _blinkStartTime = millis();
        _drawEyes(EyeState::BLINK);
    }

    void update() {
        if (_isBlinking && (millis() - _blinkStartTime > 150)) {
            _isBlinking = false;
            _drawEyes(_currentState);
        }
    }

private:
    Adafruit_GC9A01A left = Adafruit_GC9A01A(10, 8, 7);
    Adafruit_GC9A01A right = Adafruit_GC9A01A(9, 8, 7);
    
    EyeState _currentState = EyeState::CALM;
    bool _isBlinking = false;
    unsigned long _blinkStartTime = 0;

    void _drawEyes(EyeState state) {
        uint16_t bgColor = GC9A01A_BLACK;
        uint16_t eyeColor = GC9A01A_GREEN; 
        uint16_t pupilColor = GC9A01A_BLACK;

        if (state == EyeState::SLEEP) eyeColor = GC9A01A_BLUE;
        else if (state == EyeState::HAPPY) eyeColor = GC9A01A_YELLOW;

        left.fillScreen(bgColor);
        right.fillScreen(bgColor);

        if (state == EyeState::BLINK) {
            left.drawLine(60, 120, 180, 120, eyeColor);
            right.drawLine(60, 120, 180, 120, eyeColor);
            return;
        }

        left.fillCircle(120, 120, 80, eyeColor);
        right.fillCircle(120, 120, 80, eyeColor);

        // Змеиный зрачок (вертикальный эллипс)
        left.fillEllipse(120, 120, 15, 60, pupilColor);
        right.fillEllipse(120, 120, 15, 60, pupilColor);

        // Блик
        left.fillCircle(140, 90, 10, GC9A01A_WHITE);
        right.fillCircle(140, 90, 10, GC9A01A_WHITE);

        _drawEyelashes(left);
        _drawEyelashes(right);
    }

    void _drawEyelashes(Adafruit_GC9A01A &display) {
        uint16_t color = GC9A01A_WHITE;
        display.drawLine(60, 60, 40, 40, color);
        display.drawLine(120, 45, 120, 25, color);
        display.drawLine(180, 60, 200, 40, color);
    }
};