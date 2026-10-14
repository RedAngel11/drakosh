#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

enum class Emotion {
    CALM, JOY, SUPPORT, ALARM, SLEEP, OFF, VOICE_DETECTED
};

struct RGB { uint8_t r, g, b; };

inline RGB emotionColor(Emotion e) {
    switch (e) {
        case Emotion::CALM:          return RGB{ 20,  80, 160}; // Спокойный синий
        case Emotion::JOY:           return RGB{255, 150,   0};
        case Emotion::SUPPORT:       return RGB{ 80, 200, 100};
        case Emotion::ALARM:         return RGB{230,  40,  40};
        case Emotion::SLEEP:         return RGB{ 60,  30, 110};
        case Emotion::OFF:           return RGB{  0,   0,   0};
        case Emotion::VOICE_DETECTED:return RGB{255, 255, 255}; // Ярко-белый при голосе
    }
    return RGB{0, 0, 0};
}

class LightEngine {
public:
    void begin(uint8_t pin, uint16_t count) {
        _strip = new Adafruit_NeoPixel(count, pin, NEO_GRB + NEO_KHZ800);
        _strip->begin();
        _strip->setBrightness(BRIGHTNESS);
        setEmotion(Emotion::CALM, true);
    }

    void setEmotion(Emotion e, bool instant = false) {
        _from = _cur;
        _target = emotionColor(e);
        _startMs = millis();
        if (instant) { 
            _cur = _target; 
            _apply(); 
            _fading = false; 
        } else {
            _fading = true; 
        }
        _currentEmotion = e;
    }

    void update() {
        if (!_fading || !_strip) return;
        float t = (millis() - _startMs) / 800.0;
        if (t >= 1.0) { t = 1.0; _fading = false; }
        _cur.r = _from.r + (_target.r - _from.r) * t;
        _cur.g = _from.g + (_target.g - _from.g) * t;
        _cur.b = _from.b + (_target.b - _from.b) * t;
        _apply();
    }
        // Задать произвольный цвет с плавным переходом (переиспользуем готовый механизм фейда)
    void setColorRGB(uint8_t r, uint8_t g, uint8_t b) {
        _from = _cur;
        _target = RGB{r, g, b};
        _startMs = millis();
        _fading = true;
    }

    // Задать цвет по имени; false — если имя неизвестно
    bool setColorByName(const String& name) {
        if      (name == "red")    setColorRGB(255, 0, 0);
        else if (name == "blue")   setColorRGB(0, 0, 255);
        else if (name == "green")  setColorRGB(0, 255, 0);
        else if (name == "yellow") setColorRGB(255, 255, 0);
        else if (name == "purple") setColorRGB(128, 0, 128);
        else if (name == "white")  setColorRGB(255, 255, 255);
        else if (name == "orange") setColorRGB(255, 128, 0);
        else if (name == "cyan")   setColorRGB(0, 255, 255);
        else return false;
        return true;
    }

    void setBrightness(uint8_t value) {
        if (_strip) _strip->setBrightness(value);
    }

private:
    void _apply() {
        if (!_strip) return;
        uint32_t c = _strip->Color(_cur.r, _cur.g, _cur.b);
        for (uint16_t i = 0; i < _strip->numPixels(); i++) {
            _strip->setPixelColor(i, c);
        }
        _strip->show();
    }

    Adafruit_NeoPixel* _strip = nullptr;
    RGB _cur{0,0,0}, _from{0,0,0}, _target{0,0,0};
    unsigned long _startMs = 0;
    bool _fading = false;
    Emotion _currentEmotion = Emotion::CALM;
};