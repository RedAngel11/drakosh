#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

enum class Emotion { CALM, JOY, SUPPORT, ALARM, SLEEP, OFF, VOICE_DETECTED };
struct RGB { uint8_t r, g, b; };

inline RGB emotionColor(Emotion e) {
    switch (e) {
        case Emotion::CALM:           return RGB{ 20,  80, 160};
        case Emotion::JOY:            return RGB{255, 150,   0};
        case Emotion::SUPPORT:        return RGB{ 80, 200, 100};
        case Emotion::ALARM:          return RGB{230,  40,  40};
        case Emotion::SLEEP:          return RGB{ 60,  30, 110};
        case Emotion::OFF:            return RGB{  0,   0,   0};
        case Emotion::VOICE_DETECTED: return RGB{255, 255, 255};
    }
    return RGB{0,0,0};
}

class LightEngine {
public:
    void begin() {
        _mouth = new Adafruit_NeoPixel(NUM_LEDS_MOUTH, PIN_LED_MOUTH, NEO_GRB + NEO_KHZ800);
        _belly = new Adafruit_NeoPixel(NUM_LEDS_BELLY, PIN_LED_BELLY, NEO_GRB + NEO_KHZ800);
        _mouth->begin(); _mouth->setBrightness(BRIGHTNESS);
        _belly->begin(); _belly->setBrightness(BRIGHTNESS);
        setEmotion(Emotion::CALM, true);
    }

    void setEmotion(Emotion e, bool instant = false) {
        _from = _cur;
        _target = emotionColor(e);
        _startMs = millis();
        if (instant) { _cur = _target; _apply(); _fading = false; }
        else { _fading = true; }
        _currentEmotion = e;
    }

    // Розовый рот на N миллисекунд (не блокирующее)
    void setMouthPink(unsigned long durationMs) {
        _mouthOverride = true;
        _mouthOverrideEnd = millis() + durationMs;
        _applyMouthColor(255, 105, 180); // розовый
    }

    // Цвет по имени (для живота + рта, кроме override)
    bool setColorByName(const String& name) {
        if      (name == "red")    _setAll(255, 0, 0);
        else if (name == "blue")   _setAll(0, 0, 255);
        else if (name == "green")  _setAll(0, 255, 0);
        else if (name == "yellow") _setAll(255, 255, 0);
        else if (name == "purple") _setAll(128, 0, 128);
        else if (name == "white")  _setAll(255, 255, 255);
        else if (name == "orange") _setAll(255, 128, 0);
        else if (name == "cyan")   _setAll(0, 255, 255);
        else return false;
        return true;
    }

    void setBrightness(uint8_t value) {
        if (_mouth) _mouth->setBrightness(value);
        if (_belly) _belly->setBrightness(value);
        _apply();
    }

    // Радуга - переливающийся эффект на животе
    void setRainbowMode(bool on) {
        _rainbow = on;
        _rainbowCycle = 0;
    }

    void update() {
        // 1. Проверка таймера override рта (розовый при приветствии)
        if (_mouthOverride && millis() > _mouthOverrideEnd) {
            _mouthOverride = false;
        }

        // 2. Плавный переход цвета живота (если не радуга и не override)
        if (_fading && !_rainbow) {
            float t = (millis() - _startMs) / 800.0f;
            if (t >= 1.0f) { t = 1.0f; _fading = false; }
            _cur.r = _from.r + (_target.r - _from.r) * t;
            _cur.g = _from.g + (_target.g - _from.g) * t;
            _cur.b = _from.b + (_target.b - _from.b) * t;
            _applyBellyColor(_cur.r, _cur.g, _cur.b);
        }

        // 3. Радуга (на животе, переливается)
        if (_rainbow) {
            for (uint16_t i = 0; i < _belly->numPixels(); i++) {
                uint8_t hue = (_rainbowCycle + i * 255 / _belly->numPixels()) & 255;
                uint32_t c = _wheel(hue);
                _belly->setPixelColor(i, c);
            }
            _belly->show();
            _rainbowCycle++;
        }

        // 4. Рот - либо override (розовый), либо цвет эмоции
        if (!_mouthOverride && !_rainbow) {
            _applyMouthColor(_cur.r, _cur.g, _cur.b);
        }
    }

private:
    Adafruit_NeoPixel* _mouth = nullptr;
    Adafruit_NeoPixel* _belly = nullptr;
    RGB _cur{0,0,0}, _from{0,0,0}, _target{0,0,0};
    unsigned long _startMs = 0;
    bool _fading = false;
    Emotion _currentEmotion = Emotion::CALM;

    bool _mouthOverride = false;
    unsigned long _mouthOverrideEnd = 0;

    bool _rainbow = false;
    uint16_t _rainbowCycle = 0;

    void _setAll(uint8_t r, uint8_t g, uint8_t b) {
        _applyMouthColor(r, g, b);
        _applyBellyColor(r, g, b);
        _cur = RGB{r, g, b};
        _fading = false;
    }

    void _apply() {
        _applyMouthColor(_cur.r, _cur.g, _cur.b);
        _applyBellyColor(_cur.r, _cur.g, _cur.b);
    }

    void _applyMouthColor(uint8_t r, uint8_t g, uint8_t b) {
        if (!_mouth) return;
        uint32_t c = _mouth->Color(r, g, b);
        for (uint16_t i = 0; i < _mouth->numPixels(); i++) _mouth->setPixelColor(i, c);
        _mouth->show();
    }

    void _applyBellyColor(uint8_t r, uint8_t g, uint8_t b) {
        if (!_belly) return;
        uint32_t c = _belly->Color(r, g, b);
        for (uint16_t i = 0; i < _belly->numPixels(); i++) _belly->setPixelColor(i, c);
        _belly->show();
    }

    uint32_t _wheel(byte pos) {
        pos = 255 - pos;
        if (pos < 85)  return _belly->Color(255 - pos*3, 0, pos*3);
        if (pos < 170) { pos -= 85; return _belly->Color(0, pos*3, 255 - pos*3); }
        pos -= 170; return _belly->Color(pos*3, 255 - pos*3, 0);
    }
};