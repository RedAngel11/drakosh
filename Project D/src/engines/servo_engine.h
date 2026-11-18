#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "../config.h"

#define SERVO_USED 2

class ServoEngine {
public:
  bool begin() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);
    _ok = _probe();
    if (!_ok) return false;
    _pwm.begin();
    _pwm.setOscillatorFrequency(SERVO_OSC);
    _pwm.setPWMFreq(SERVO_FREQ);
    _ch[0] = SERVO_CH_A; _ch[1] = SERVO_CH_B;
    for (uint8_t i = 0; i < SERVO_USED; i++) {
      _deg[i] = _from[i] = _to[i] = SERVO_NEUTRAL_DEG;
      _mov[i] = false; _dur[i] = SERVO_MOVE_MS; _t0[i] = millis();
      _pwm.setPWM(_ch[i], 0, ticks(_deg[i]));
      delay(150);
    }
    return true;
  }
  bool ok() const { return _ok; }

  static uint16_t ticks(float deg) {
    if (deg < 0) deg = 0;
    if (deg > 180) deg = 180;
    float t = (deg <= 90.0f)
      ? SERVO_0  + (SERVO_90  - SERVO_0)  * deg / 90.0f
      : SERVO_90 + (SERVO_180 - SERVO_90) * (deg - 90.0f) / 90.0f;
    if (t < 80) t = 80;
    if (t > 700) t = 700;
    return (uint16_t)(t + 0.5f);
  }

  void moveTo(uint8_t i, float deg, uint16_t ms, uint16_t delayMs = 0) {
    if (i >= SERVO_USED) return;
    if (deg < SERVO_MIN_DEG) deg = SERVO_MIN_DEG;
    if (deg > SERVO_MAX_DEG) deg = SERVO_MAX_DEG;
    _from[i] = _deg[i]; _to[i] = deg;
    _dur[i] = ms ? ms : 1;
    _t0[i] = millis() + delayMs;
    _mov[i] = true;
  }
  void moveBoth(float deg, uint16_t ms) {
    moveTo(0, deg, ms, 0);
    moveTo(1, deg, ms, SERVO_STAGGER_MS);
  }
  void neutral(uint16_t ms = SERVO_MOVE_MS) { moveBoth(SERVO_NEUTRAL_DEG, ms); }
  void startWave() { _wStep = 0; _wT = millis(); _waving = true; _waveStep(); }
  bool busy() { return _mov[0] || _mov[1] || _waving; }
  void release() { for (uint8_t i = 0; i < SERVO_USED; i++) _pwm.setPWM(_ch[i], 0, 4096); }

  void update() {
    uint32_t now = millis();
    if (_waving && now - _wT >= 450) {
      _wT = now; _wStep++;
      if (_wStep >= 4) { _waving = false; neutral(300); }
      else _waveStep();
    }
    if (now - _last < 20) return;
    _last = now;
    for (uint8_t i = 0; i < SERVO_USED; i++) {
      if (!_mov[i]) continue;
      if ((int32_t)(now - _t0[i]) < 0) continue;
      float t = (float)(now - _t0[i]) / (float)_dur[i];
      if (t > 1.0f) t = 1.0f;
      float s = t * t * (3.0f - 2.0f * t);
      _deg[i] = _from[i] + (_to[i] - _from[i]) * s;
      _pwm.setPWM(_ch[i], 0, ticks(_deg[i]));
      if (t >= 1.0f) _mov[i] = false;
    }
  }

private:
  Adafruit_PWMServoDriver _pwm = Adafruit_PWMServoDriver(PCA9685_ADDRESS);
  bool _ok = false, _waving = false;
  bool _mov[SERVO_USED] = {false, false};
  uint8_t _ch[SERVO_USED] = {0, 1}, _wStep = 0;
  float _deg[SERVO_USED] = {90, 90}, _from[SERVO_USED] = {90, 90}, _to[SERVO_USED] = {90, 90};
  uint16_t _dur[SERVO_USED] = {350, 350};
  uint32_t _t0[SERVO_USED] = {0, 0}, _last = 0, _wT = 0;

  bool _probe() {
    Wire.beginTransmission(PCA9685_ADDRESS);
    return Wire.endTransmission() == 0;
  }
  void _waveStep() {
    switch (_wStep) {
      case 0: moveTo(0, 135, 300); moveTo(1,  45, 300); break;
      case 1: moveTo(0,  45, 300); moveTo(1, 135, 300); break;
      case 2: moveTo(0, 135, 300); moveTo(1,  45, 300); break;
      default: neutral(300); break;
    }
  }
};