#pragma once
#include <Arduino.h>
#include "config.h"
#include "engines/light_engine.h"
#include "engines/audio_engine.h"
#include "engines/display_engine.h"
#include "engines/servo_engine.h"
class CommandHandler {
public:
  void setEngines(LightEngine* light, AudioEngine* audio, DisplayEngine* display, ServoEngine* servo) {
    _light = light; _audio = audio; _display = display; _servo = servo;
  }
  bool busy() const { return _st != ST_IDLE; }
  void setVoice(bool on) { _voiceOn = on; }
  bool voiceOn() const { return _voiceOn; }

  void execute(String cmd) {
    cmd.trim(); cmd.toLowerCase();
    if (cmd.isEmpty() || cmd == "none") return;
    String name = cmd, param = "";
    int i = cmd.indexOf(':');
    if (i > 0) { name = cmd.substring(0, i); param = cmd.substring(i + 1); param.trim(); }
    if (name != "rainbow") _light->setRainbow(false);

    if      (name == "greet" || name == "hello" || name == "g") startGreet();
    else if (name == "dance")   { _light->setEmotion(Emotion::JOY); _servo->startWave(); }
    else if (name == "blink")   _display->blink();
    else if (name == "rainbow") _light->setRainbow(true);
    else if (name == "change_color")   _light->setColorByName(param);
    else if (name == "set_brightness") _light->setBellyBrightness((uint8_t)constrain(param.toInt(), 0, 200));
    else if (name == "light_joy")      _light->setEmotion(Emotion::JOY);
    else if (name == "light_calm")     _light->setEmotion(Emotion::CALM);
    else if (name == "light_support")  _light->setEmotion(Emotion::SUPPORT);
    else if (name == "light_alarm")    _light->setEmotion(Emotion::ALARM);
    else if (name == "light_sleep" || name == "bye") {
      _light->setEmotion(Emotion::SLEEP);
      _display->showState(EyeState::SLEEP, 3600000UL);
    }
    else if (name == "light_off") _light->setEmotion(Emotion::OFF);
    else if (name == "center" || name == "wave_wings") _servo->neutral();
    else if (name == "voice_on")  _voiceOn = true;
    else if (name == "voice_off") _voiceOn = false;
    else if (name == "stop") { _audio->stop(); _servo->neutral(); _st = ST_IDLE; }
  }

  void update() {
#if VOICE_TRIG_ENABLE
    bool heard = _audio->voiceHeard();
    if (heard && _voiceOn && _st == ST_IDLE && !_servo->busy()) startGreet();
#endif
    _scenario();
    _mouth();
  }

  void startGreet() {
    if (_st != ST_IDLE || _servo->busy()) return;
    _light->setEmotion(Emotion::JOY);
    _display->showState(EyeState::HAPPY, 6000);
    _servo->moveBoth(SERVO_NEUTRAL_DEG + GREET_DELTA_DEG, SERVO_MOVE_MS);
    _st = ST_MOVE; _t = _t0 = millis();
  }

private:
  enum State { ST_IDLE, ST_MOVE, ST_SPEAK, ST_SETTLE, ST_RETURN };
  LightEngine* _light = nullptr;
  AudioEngine* _audio = nullptr;
  DisplayEngine* _display = nullptr;
  ServoEngine* _servo = nullptr;
  State _st = ST_IDLE;
  uint32_t _t = 0, _t0 = 0;
  bool _voiceOn = true;

  void _scenario() {
    uint32_t now = millis();
    switch (_st) {
      case ST_MOVE:
        if (now - _t >= GREET_MOVE_WAIT_MS) {
          if (_audio->playHello()) { _st = ST_SPEAK; _t = now; }
          else { _servo->neutral(); _st = ST_RETURN; _t = now; }
        }
        break;
      case ST_SPEAK:
        if (!_audio->busy()) { _st = ST_SETTLE; _t = now; }
        break;
      case ST_SETTLE:
        if (now - _t >= GREET_TAIL_MS) { _servo->neutral(); _st = ST_RETURN; _t = now; }
        break;
      case ST_RETURN:
        if (!_servo->busy()) { _light->setEmotion(Emotion::CALM); _st = ST_IDLE; }
        break;
      default: break;
    }
    if (_st != ST_IDLE && millis() - _t0 > GREET_TIMEOUT_MS) {
      _audio->stop(); _servo->neutral();
      _light->setEmotion(Emotion::CALM);
      _st = ST_IDLE;
    }
  }

  void _mouth() {
    static uint32_t t = 0;
    if (millis() - t < 30) return;
    t = millis();
    uint8_t b = MOUTH_BRIGHT_MIN;
    if (_st == ST_SPEAK) {
      uint8_t e = _audio->level();
      if (e > 80) e = 80;
      b = (uint8_t)(MOUTH_BRIGHT_MIN + (uint16_t)e * (MOUTH_BRIGHT_MAX - MOUTH_BRIGHT_MIN) / 80);
    }
    _light->setMouthBrightness(b);
  }
};