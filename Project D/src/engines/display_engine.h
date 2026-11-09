#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <math.h>
#include "config.h"

enum class EyeState { CALM, HAPPY, SLEEP, BLINK };
enum class AutoPhase { STRAIGHT_A, BIGBLINK, STRAIGHT_B, GLANCE };

#define PHASE_STRAIGHT_MS 20000UL   // 20 c смотрит ровно
#define PHASE_BLINK_MS     5000UL   // 5 c большое моргание
#define PHASE_GLANCE_MS    5000UL   // 5 c смотрит в сторону

#define BLINK_HALF1_END_MS 1000UL   // 0–1 c призакрытые
#define BLINK_CLOSED_END_MS 4000UL  // 1–4 c закрытые, 4–5 c снова призакрытые

#define DISPLAY_SPI_HZ 24000000UL

class DisplayEngine {
public:
    void begin() {
        pinMode(DISPLAY_BL_PIN, OUTPUT);
        digitalWrite(DISPLAY_BL_PIN, HIGH);

        // Один общий сброс обеих панелей вручную
        pinMode(DISPLAY_RST, OUTPUT);
        digitalWrite(DISPLAY_RST, HIGH); delay(10);
        digitalWrite(DISPLAY_RST, LOW);  delay(10);
        digitalWrite(DISPLAY_RST, HIGH); delay(120);

        // Один объект: обе панели зеркалят его
        _panel.begin(DISPLAY_SPI_HZ);
        _panel.setRotation(0);

        _phaseStart = millis();
        showState(EyeState::CALM);
    }

    void setBacklight(bool on) {
        digitalWrite(DISPLAY_BL_PIN, on ? HIGH : LOW);
    }

    void showState(EyeState state, unsigned long pauseMs = 6000) {
        _state = state;
        _autoPausedUntil = millis() + pauseMs;
        _needPhaseReset = true;
        _cmdBlinkUntil = 0;
        _lid = (state == EyeState::SLEEP) ? 2 : 0;
        _pupilDx = 0; _pupilDy = 0;
        _drawEye();
    }

    void blink() {
        _cmdBlinkUntil = millis() + 150;
        _lid = 2;
        _drawEye();
    }

    bool isAwake() const {
        return (_state == EyeState::CALM || _state == EyeState::HAPPY);
    }

    void update() {
        unsigned long now = millis();

        if (_cmdBlinkUntil) {
            if (now >= _cmdBlinkUntil) {
                _cmdBlinkUntil = 0;
                _lid = 0;
                _drawEye();
            }
            return;
        }

        if (now < _autoPausedUntil) return;

        if (_needPhaseReset) {
            _needPhaseReset = false;
            _gotoPhase(AutoPhase::STRAIGHT_A);
            return;
        }

        unsigned long el = now - _phaseStart;

        switch (_phase) {
        case AutoPhase::STRAIGHT_A:
            if (el >= PHASE_STRAIGHT_MS) _gotoPhase(AutoPhase::BIGBLINK);
            break;

        case AutoPhase::BIGBLINK: {
            uint8_t want = (el < BLINK_HALF1_END_MS) ? 1
                         : (el < BLINK_CLOSED_END_MS) ? 2 : 1;
            if (want != _lid) {
                _lid = want;
                _drawEye();
            }
            if (el >= PHASE_BLINK_MS) _gotoPhase(AutoPhase::STRAIGHT_B);
            break;
        }

        case AutoPhase::STRAIGHT_B:
            if (el >= PHASE_STRAIGHT_MS) _gotoPhase(AutoPhase::GLANCE);
            break;

        case AutoPhase::GLANCE:
            if (el >= PHASE_GLANCE_MS) _gotoPhase(AutoPhase::STRAIGHT_A);
            break;
        }
    }

private:
    Adafruit_GC9A01A _panel = Adafruit_GC9A01A(DISPLAY_LEFT_CS, DISPLAY_DC, -1);

    EyeState  _state = EyeState::CALM;
    AutoPhase _phase = AutoPhase::STRAIGHT_A;
    unsigned long _phaseStart = 0;
    unsigned long _autoPausedUntil = 0;
    bool _needPhaseReset = false;
    unsigned long _cmdBlinkUntil = 0;

    uint8_t _lid = 0;               // 0 открыт, 1 призакрыт, 2 закрыт
    int8_t  _pupilDx = 0, _pupilDy = 0;

    void _gotoPhase(AutoPhase p) {
        _phase = p;
        _phaseStart = millis();

        if (p == AutoPhase::BIGBLINK) {
            _lid = 1;
        } else if (p == AutoPhase::GLANCE) {
            const int8_t dx[6] = { -30, 30,   0,   0, -22, 22 };
            const int8_t dy[6] = {   0,  0, -12,  12, -10, -10 };
            uint8_t i = random(6);
            _pupilDx = dx[i];
            _pupilDy = dy[i];
        } else {
            _lid = 0;
            _pupilDx = 0; _pupilDy = 0;
        }
        _drawEye();
    }

    // Гладкий вытянутый эллипс из послойных линий (без fillEllipse)
    void _fillEllipseSafe(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color) {
        for (int16_t y = -ry; y <= ry; y++) {
            float t = 1.0f - (float)(y * y) / ((float)ry * (float)ry);
            if (t < 0.0f) t = 0.0f;
            int16_t w = (int16_t)((float)rx * sqrtf(t));
            _panel.drawLine(cx - w, cy + y, cx + w, cy + y, color);
        }
    }

    void _drawEye() {
        uint16_t eyeColor = GC9A01A_GREEN;
        if (_state == EyeState::SLEEP) eyeColor = GC9A01A_BLUE;
        else if (_state == EyeState::HAPPY) eyeColor = GC9A01A_YELLOW;

        // Белый фон — склера
        _panel.fillScreen(GC9A01A_WHITE);

        // ПОЛНОСТЬЮ ЗАКРЫТЫЙ ГЛАЗ: сплошная чёрная форма + реснички
        if (_lid == 2 || _state == EyeState::BLINK) {
            // Толстая чёрная полоса вместо глаза
            _panel.fillRect(50, 110, 140, 20, GC9A01A_BLACK);
            // Чёрные реснички от полосы вверх
            _panel.drawLine(70, 110, 55, 90, GC9A01A_BLACK);
            _panel.drawLine(120, 110, 120, 85, GC9A01A_BLACK);
            _panel.drawLine(170, 110, 185, 90, GC9A01A_BLACK);
            return;
        }

        // Рисуем открытый глаз (радужка, зрачок, блик)
        _panel.fillCircle(120, 120, 80, eyeColor);
        _fillEllipseSafe(120 + _pupilDx, 120 + _pupilDy, 15, 60, GC9A01A_BLACK);
        _panel.fillCircle(127 + _pupilDx / 2, 94 + _pupilDy / 2, 8, GC9A01A_WHITE);

        // ПОЛУЗАКРЫТЫЙ ГЛАЗ: верхняя часть закрыта ЧЁРНЫМ веком
        if (_lid == 1) {
            // Чёрное веко сверху (закрывает склеру и верхнюю часть радужки)
            _panel.fillRect(40, 40, 161, 80, GC9A01A_BLACK);
            // Чёрная окантовка края века
            _panel.drawLine(40, 120, 200, 120, GC9A01A_BLACK);
            // Чёрные реснички от края века наружу
            _panel.drawLine(40, 120, 25, 100, GC9A01A_BLACK);
            _panel.drawLine(70, 120, 55, 100, GC9A01A_BLACK);
            _panel.drawLine(120, 120, 120, 95, GC9A01A_BLACK);
            _panel.drawLine(170, 120, 185, 100, GC9A01A_BLACK);
            _panel.drawLine(200, 120, 215, 100, GC9A01A_BLACK);
        }
        // ОТКРЫТЫЙ ГЛАЗ: ресничек нет, только радужка + зрачок + блик
    }
};