/* ДРАКОШКА · свет + глаза + серво + звук + микрофон + Telegram-реле
   Отладка без Serial: самодиагностика лентой живота + VU-режим по кнопке.
   Кнопка BOOT: короткое нажатие = сценарий «привет»
                удержание 1.5...5 с = VU-режим микрофона на 15 с
                удержание 5 с и больше = вкл/выкл голосовой канал */
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "engines/light_engine.h"
#include "engines/audio_engine.h"
#include "engines/display_engine.h"
#include "engines/servo_engine.h"
#include "command_handler.h"
#include "network_client.h"

static LightEngine     light;
static AudioEngine     audio;
static DisplayEngine   eyes;
static ServoEngine     servo;
static CommandHandler  handler;
static NetworkClient   net;
static Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

static bool errServo = false, errAudio = false;
static uint32_t vuUntil = 0, vuCount0 = 0;
static bool voiceBeforeVu = true;

static void statusSet(uint8_t r, uint8_t g, uint8_t b) {
  static uint32_t key = 0xFFFFFFFF;
  uint32_t k = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  if (k == key) return;
  key = k;
  statusLed.setPixelColor(0, statusLed.Color(r, g, b));
  statusLed.show();
}

static void blinkBelly(uint8_t r, uint8_t g, uint8_t b, uint8_t n,
                       uint16_t onMs = 150, uint16_t offMs = 150) {
  for (uint8_t i = 0; i < n; i++) {
    light.bellyFill(r, g, b); delay(onMs);
    light.bellyFill(0, 0, 0); delay(offMs);
  }
}

static void enterVu() {
  voiceBeforeVu = handler.voiceOn();
  handler.setVoice(false);                 // в VU фраза не запускает сценарий
  vuCount0 = audio.voiceCount();
  vuUntil = millis() + 15000;
  if (!audio.micReady()) blinkBelly(255, 0, 0, 3, 120, 120);   // микрофон мёртв
}

static void vuUpdate() {
  if (!vuUntil) return;
  if (millis() >= vuUntil) {
    vuUntil = 0;
    handler.setVoice(voiceBeforeVu);
    light.setEmotion(Emotion::CALM, true);
    return;
  }
  static uint32_t t = 0;
  if (millis() - t >= 60) {
    t = millis();
    light.vu(audio.mic(), audio.micTh());
  }
  if (audio.voiceCount() != vuCount0) {    // фраза распознана
    vuCount0 = audio.voiceCount();
    blinkBelly(255, 255, 255, 3, 90, 90);
  }
}

static void buttonUpdate() {
  static bool down = false;
  static uint32_t tDown = 0, tPoll = 0;
  if (millis() - tPoll < 25) return;
  tPoll = millis();
  bool pressed = (digitalRead(PIN_BUTTON) == LOW);
  if (pressed && !down) { down = true; tDown = millis(); return; }
  if (!pressed && down) {
    down = false;
    uint32_t dur = millis() - tDown;
    if (dur >= 5000) {
      bool on = !handler.voiceOn();
      handler.setVoice(on);
      blinkBelly(on ? 0 : 255, on ? 255 : 0, 0, 3, 120, 120);
      light.setEmotion(Emotion::CALM, true);
    } else if (dur >= 1500) {
      enterVu();
    } else if (dur >= 40) {
      handler.startGreet();
    }
  }
}

static void statusUpdate() {
  static uint32_t t = 0;
  if (millis() - t < 120) return;
  t = millis();
  if (vuUntil)                              statusSet(20, 20, 0);    // жёлтый: VU
  else if (handler.busy())                  statusSet(0, 25, 10);    // зелёный: сценарий
  else if (errServo || errAudio)            statusSet(25, 12, 0);    // оранжевый: узел не ответил
  else if (!net.connected())                statusSet(25, 0, 0);     // красный: нет Wi-Fi
  else if (!audio.micReady())               statusSet(20, 0, 20);    // сиреневый: микрофон молчит
  else                                      statusSet(0, 0, 12);     // синий: ждём команду
}

void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  statusLed.begin();
  statusLed.setBrightness(40);
  statusSet(20, 20, 20);

  light.begin();
  eyes.begin();
  errServo = !servo.begin();
  net.begin();
  errAudio = !audio.begin();
  handler.setEngines(&light, &audio, &eyes, &servo);

  // === самодиагностика лентой: 1 мигание = серво, 2 = звук, 3 = Wi-Fi ===
  bool wf = net.connected();
  blinkBelly(errServo ? 255 : 0, errServo ? 0 : 255, 0, 1); delay(400);
  blinkBelly(errAudio ? 255 : 0, errAudio ? 0 : 255, 0, 2); delay(400);
  blinkBelly(wf ? 0 : 255, wf ? 255 : 0, 0, 3); delay(400);
  light.setEmotion(Emotion::CALM, true);
}

void loop() {
  audio.update();
  servo.update();
  eyes.update();
  light.update();
  handler.update();
  vuUpdate();
  statusUpdate();
  buttonUpdate();
  if (!vuUntil && !handler.busy() && !servo.busy()) {
    String c = net.pollCommand();
    if (c != "none" && c.length()) handler.execute(c);
  }
}