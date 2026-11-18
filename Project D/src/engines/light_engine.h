#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "../config.h"

enum class Emotion { CALM, JOY, SUPPORT, ALARM, SLEEP, OFF };
struct RGB { uint8_t r, g, b; };

inline RGB emotionColor(Emotion e) {
  switch (e) {
    case Emotion::CALM:    return RGB{ 20,  80, 160};
    case Emotion::JOY:     return RGB{  0, 255,  40};
    case Emotion::SUPPORT: return RGB{ 80, 200, 100};
    case Emotion::ALARM:   return RGB{230,  40,  40};
    case Emotion::SLEEP:   return RGB{ 60,  30, 110};
    default:               return RGB{  0,   0,   0};
  }
}

class LightEngine {
public:
  void begin() {
    _belly = new Adafruit_NeoPixel(NUM_LEDS_BELLY, PIN_LED_BELLY, NEO_GRB + NEO_KHZ800);
    _mouth = new Adafruit_NeoPixel(NUM_LEDS_MOUTH, PIN_LED_MOUTH, NEO_GRB + NEO_KHZ800);
    _belly->begin(); _mouth->begin();
    _belly->setBrightness(BELLY_BRIGHTNESS);
    _mouth->setBrightness(MOUTH_BRIGHT_MIN);
    _mouthBr = MOUTH_BRIGHT_MIN; _mouthNow = MOUTH_BRIGHT_MIN;
    setEmotion(Emotion::CALM, true);
    _showMouth(true);
  }

  void setEmotion(Emotion e, bool instant = false) {
    _from = _cur; _target = emotionColor(e); _t0 = millis(); _rainbow = false;
    _fading = !instant;
    if (instant) { _cur = _target; _showBelly(true); }
  }

  bool setColorByName(const String& n) {
    RGB c;
    if      (n == "red")    c = {255,   0,   0};
    else if (n == "green")  c = {  0, 255,  40};
    else if (n == "blue")   c = {  0,   0, 255};
    else if (n == "yellow") c = {255, 255,   0};
    else if (n == "purple") c = {128,   0, 128};
    else if (n == "white")  c = {200, 200, 200};
    else if (n == "orange") c = {255, 128,   0};
    else if (n == "cyan")   c = {  0, 255, 255};
    else return false;
    _rainbow = false; _from = _cur; _target = c; _t0 = millis(); _fading = true;
    return true;
  }

  void setBellyBrightness(uint8_t v) { _belly->setBrightness(v); _showBelly(true); }
  void setMouthBrightness(uint8_t v) { _mouthBr = v; }
  void setRainbow(bool on) { _rainbow = on; _hue = 0; }

  // мгновенная заливка живота (для миганий-диагностики)
  void bellyFill(uint8_t r, uint8_t g, uint8_t b) {
    _rainbow = false; _fading = false;
    _cur = RGB{r, g, b};
    _showBelly(true);
  }

  // шкала уровня микрофона: зелёный/жёлтый столбик, синий диод = порог
  void vu(uint8_t lvl, uint8_t th) {
    _rainbow = false; _fading = false;
    const uint16_t FS = 96;
    uint16_t n = _belly->numPixels();
    uint16_t lit = (uint16_t)(((uint32_t)lvl * n) / FS);
    uint16_t ti  = (uint16_t)(((uint32_t)th  * n) / FS);
    if (lit > n) lit = n;
    if (ti >= n) ti = n - 1;
    for (uint16_t i = 0; i < n; i++) {
      uint32_t c = 0;
      if (i < lit) c = (i >= ti) ? _belly->Color(255, 170, 0) : _belly->Color(0, 255, 40);
      if (i == ti) c = _belly->Color(40, 40, 255);
      _belly->setPixelColor(i, c);
    }
    _belly->show();
    _bKey = 0xFFFFFFFF;
  }

  void update() {
    uint32_t now = millis();
    if (now - _last < 20) return;
    _last = now;

    if (_rainbow) {
      for (uint16_t i = 0; i < _belly->numPixels(); i++)
        _belly->setPixelColor(i, _wheel((uint8_t)((_hue + i * 255 / _belly->numPixels()) & 255)));
      _belly->show(); _hue += 2; _bKey = 0xFFFFFFFF;
    } else if (_fading) {
      float t = (float)(now - _t0) / 600.0f;
      if (t >= 1.0f) { t = 1.0f; _fading = false; }
      _cur.r = (uint8_t)(_from.r + (int16_t)(_target.r - _from.r) * t);
      _cur.g = (uint8_t)(_from.g + (int16_t)(_target.g - _from.g) * t);
      _cur.b = (uint8_t)(_from.b + (int16_t)(_target.b - _from.b) * t);
      _showBelly();
    }

    int16_t d = (int16_t)_mouthBr - _mouthNow;
    if (d > 0) _mouthNow += (d + 2) / 3;
    else if (d < 0) _mouthNow -= ((-d) + 2) / 3;
    _showMouth();
  }

private:
  Adafruit_NeoPixel* _belly = nullptr;
  Adafruit_NeoPixel* _mouth = nullptr;
  RGB _cur{0,0,0}, _from{0,0,0}, _target{0,0,0};
  uint32_t _t0 = 0, _last = 0, _bKey = 0xFFFFFFFF, _mKey = 0xFFFFFFFF;
  bool _fading = false, _rainbow = false;
  uint8_t _hue = 0, _mouthBr = MOUTH_BRIGHT_MIN;
  int16_t _mouthNow = MOUTH_BRIGHT_MIN;

  void _showBelly(bool force = false) {
    uint32_t k = ((uint32_t)_belly->getBrightness() << 24) |
                 ((uint32_t)_cur.r << 16) | ((uint32_t)_cur.g << 8) | _cur.b;
    if (!force && k == _bKey) return;
    _bKey = k;
    uint32_t c = _belly->Color(_cur.r, _cur.g, _cur.b);
    for (uint16_t i = 0; i < _belly->numPixels(); i++) _belly->setPixelColor(i, c);
    _belly->show();
  }

  void _showMouth(bool force = false) {
    uint32_t k = (uint32_t)_mouthNow;
    if (!force && k == _mKey) return;
    _mKey = k;
    _mouth->setBrightness((uint8_t)_mouthNow);
    uint32_t c = _mouth->Color(255, 105, 180);
    for (uint16_t i = 0; i < _mouth->numPixels(); i++) _mouth->setPixelColor(i, c);
    _mouth->show();
  }

  uint32_t _wheel(uint8_t pos) {
    pos = 255 - pos;
    if (pos < 85)  return _belly->Color(255 - pos * 3, 0, pos * 3);
    if (pos < 170) { pos -= 85; return _belly->Color(0, pos * 3, 255 - pos * 3); }
    pos -= 170; return _belly->Color(pos * 3, 255 - pos * 3, 0);
  }
};