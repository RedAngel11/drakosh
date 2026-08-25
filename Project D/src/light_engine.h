#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

enum class Emotion { 
    CALM, 
    JOY, 
    SUPPORT, 
    ALARM, 
    SLEEP, 
    OFF 
};

struct RGB { 
    uint8_t r, g, b; 
};

inline RGB emotionColor(Emotion e) {
    switch (e) {
        case Emotion::CALM:    return RGB{ 20,  80, 160};
        case Emotion::JOY:     return RGB{255, 150,   0};
        case Emotion::SUPPORT: return RGB{ 80, 200, 100};
        case Emotion::ALARM:   return RGB{230,  40,  40};
        case Emotion::SLEEP:   return RGB{ 60,  30, 110};
        case Emotion::OFF:     return RGB{  0,   0,   0};
    }
    return RGB{0, 0, 0};
}

class LightEngine {
public:
    void begin(uint8_t pin, uint16_t count = 1) {
        _strip = new Adafruit_NeoPixel(count, pin, NEO_GRB + NEO_KHZ800);
        _strip->begin();
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
    }

    void update() {
        if (!_fading || !_strip) return;
        
        float t = (millis() - _startMs) / 1200.0;
        if (t >= 1.0) { 
            t = 1.0; 
            _fading = false; 
        }
        
        _cur.r = _from.r + (_target.r - _from.r) * t;
        _cur.g = _from.g + (_target.g - _from.g) * t;
        _cur.b = _from.b + (_target.b - _from.b) * t;
        _apply();
    }

    Emotion getCurrentEmotion() const { return _currentEmotion; }

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