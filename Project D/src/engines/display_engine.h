#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include "config.h"

enum class EyeState { CALM, HAPPY, SLEEP, BLINK };
enum class AutoPhase { STRAIGHT_A, BLINKING, STRAIGHT_B, GLANCE };

#define PHASE_STRAIGHT_MS 20000UL  // 20 c смотрит ровно
#define PHASE_BLINK_MS     5000UL  // 5 c на одно большое моргание
#define PHASE_GLANCE_MS    5000UL  // 5 c смотрит в сторону

// Тайминги большого моргания внутри фазы BLINKING
#define BLINK_HALF1_END_MS 1000UL  // до 1 c  — призакрытые
#define BLINK_CLOSED_END_MS 4000UL // 1–4 c  — закрытые
                                   // 4–5 c  — снова призакрытые

class DisplayEngine {
public:
    void begin() {
        analogWrite(DISPLAY_BL_PIN, _backlight);
        _left.begin();
        _right.begin();
        _left.setRotation(0);
        _right.setRotation(0);
        _phaseStart = millis();
        showState(EyeState::CALM);
    }

    void setBacklight(uint8_t value) {
        _backlight = value;
        analogWrite(DISPLAY_BL_PIN, value);
    }

    // Команда извне (greet/bye) — пауза автоцикла
    void showState(EyeState state, unsigned long pauseMs = 6000) {
        _state = state;
        _autoPausedUntil = millis() + pauseMs;
        _needPhaseReset = true;
        _drawEyes(0, 0, (_state == EyeState::SLEEP) ? 2 : 0);
    }

    // Быстрое моргание по команде "blink"
    void blink() {
        _cmdBlinkUntil = millis() + 150;
        _drawEyes(0, 0, 2);
    }

    void update() {
        unsigned long now = millis();

        // 1. Командное моргание — приоритет
        if (_cmdBlinkUntil) {
            if (now >= _cmdBlinkUntil) {
                _cmdBlinkUntil = 0;
                _drawEyes(_pupilDx, _pupilDy, 0);
            }
            return;
        }

        // 2. Пауза после внешней команды
        if (now < _autoPausedUntil) return;

        // 3. Аккуратный перезапуск цикла после паузы
        if (_needPhaseReset) {
            _needPhaseReset = false;
            _gotoPhase(AutoPhase::STRAIGHT_A);
            return;
        }

        // 4. Автономный цикл
        unsigned long elapsed = now - _phaseStart;

        switch (_phase) {
        case AutoPhase::STRAIGHT_A:
            if (elapsed >= PHASE_STRAIGHT_MS) _gotoPhase(AutoPhase::BLINKING);
            break;

        case AutoPhase::BLINKING: {
            // Одно большое моргание: призакрыт -> закрыт -> призакрыт
            uint8_t lid;
            if (elapsed < BLINK_HALF1_END_MS)  lid = 1;      // 0–1 c
            else if (elapsed < BLINK_CLOSED_END_MS) lid = 2; // 1–4 c
            else lid = 1;                                    // 4–5 c
            if (lid != _blinkStep) {
                _blinkStep = lid;
                _drawEyes(0, 0, lid);
            }
            if (elapsed >= PHASE_BLINK_MS) _gotoPhase(AutoPhase::STRAIGHT_B);
            break;
        }

        case AutoPhase::STRAIGHT_B:
            if (elapsed >= PHASE_STRAIGHT_MS) _gotoPhase(AutoPhase::GLANCE);
            break;

        case AutoPhase::GLANCE:
            if (elapsed >= PHASE_GLANCE_MS) _gotoPhase(AutoPhase::STRAIGHT_A);
            break;
        }
    }

private:
    Adafruit_GC9A01A _left  = Adafruit_GC9A01A(DISPLAY_LEFT_CS,  DISPLAY_DC, DISPLAY_RST);
    Adafruit_GC9A01A _right = Adafruit_GC9A01A(DISPLAY_RIGHT_CS, DISPLAY_DC, DISPLAY_RST);

    EyeState  _state = EyeState::CALM;
    AutoPhase _phase = AutoPhase::STRAIGHT_A;
    unsigned long _phaseStart = 0;

    unsigned long _autoPausedUntil = 0;
    bool _needPhaseReset = false;
    unsigned long _cmdBlinkUntil = 0;

    uint8_t _blinkStep = 0;          // текущий кадр моргания (1 или 2)
    int8_t  _pupilDx = 0, _pupilDy = 0;
    uint8_t _backlight = 160;        // ~63% яркости подсветки

    void _gotoPhase(AutoPhase p) {
        _phase = p;
        _phaseStart = millis();

        if (p == AutoPhase::BLINKING) {
            _blinkStep = 1;
            _drawEyes(0, 0, 1);      // сразу призакрытые
            return;
        }
        if (p == AutoPhase::GLANCE) {
            // Одна случайная сторона на оба глаза
            const int8_t dx[6] = { -30, 30,   0,   0, -22, 22 };
            const int8_t dy[6] = {   0,  0, -12,  12, -10, -10 };
            uint8_t i = random(6);
            _pupilDx = dx[i];
            _pupilDy = dy[i];
        } else {
            _pupilDx = 0;
            _pupilDy = 0;
        }
        _drawEyes(_pupilDx, _pupilDy, 0);
    }

    // lid: 0 — открыт, 1 — призакрыт, 2 — закрыт
    void _drawEyes(int8_t dx, int8_t dy, uint8_t lid) {
        uint16_t eyeColor = GC9A01A_GREEN;
        if (_state == EyeState::SLEEP) eyeColor = GC9A01A_BLUE;
        else if (_state == EyeState::HAPPY) eyeColor = GC9A01A_YELLOW;

        _left.fillScreen(GC9A01A_BLACK);
        _right.fillScreen(GC9A01A_BLACK);

        // Закрытые глаза — плотная линия
        if (lid == 2 || _state == EyeState::BLINK) {
            for (uint8_t s = 0; s < 2; s++) {
                Adafruit_GC9A01A& d = (s == 0) ? _left : _right;
                d.drawLine(60, 119, 180, 119, eyeColor);
                d.drawLine(60, 120, 180, 120, eyeColor);
                d.drawLine(60, 121, 180, 121, eyeColor);
            }
            return;
        }

        for (uint8_t s = 0; s < 2; s++) {
            Adafruit_GC9A01A& d = (s == 0) ? _left : _right;
            d.fillCircle(120, 120, 80, eyeColor);                      // глаз
            d.fillEllipse(120 + dx, 120 + dy, 15, 60, GC9A01A_BLACK);  // змеиный зрачок
            d.fillCircle(140 + dx / 2, 90 + dy / 2, 10, GC9A01A_WHITE);// блик

            if (lid == 1) {
                // Призакрытые: чёрное веко сверху + линия века + реснички по краю века
                d.fillRect(40, 40, 161, 80, GC9A01A_BLACK); // закрывает y=40..119
                d.drawLine(45, 120, 195, 120, eyeColor);    // край века
                d.drawLine(70, 120, 55, 105, GC9A01A_WHITE);
                d.drawLine(120, 120, 120, 100, GC9A01A_WHITE);
                d.drawLine(170, 120, 185, 105, GC9A01A_WHITE);
            } else {
                // Открытые: реснички сверху
                d.drawLine(60, 60, 40, 40, GC9A01A_WHITE);
                d.drawLine(120, 45, 120, 25, GC9A01A_WHITE);
                d.drawLine(180, 60, 200, 40, GC9A01A_WHITE);
            }
        }
    }
};