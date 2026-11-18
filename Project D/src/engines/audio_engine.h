#pragma once
/* Звук: LEDC -> 1к/100 -> 47мкФ -> LIN PAM8403. Таймер 16 кГц + кольцевой буфер.
   Микрофон INMP441 (I2S 32 бита) + детектор ритма фразы «привет, Дракоша». */
#include <Arduino.h>
#include <string.h>
#include <driver/i2s.h>
#include "../config.h"

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define AUD_CORE3 1
#else
  #define AUD_CORE3 0
#endif
#if __has_include("../wav_data.h")
  #include "../wav_data.h"
  #define HAVE_EMBEDDED_WAV 1
#else
  #define HAVE_EMBEDDED_WAV 0
#endif

static const uint32_t AUD_SR = 16000;
static const uint32_t AUD_FRAME_MS = 16;          // 256 сэмплов

// ================= ШИМ =================
struct AudCarrier { uint32_t f; uint8_t bits; };
static const AudCarrier AUD_C[] = {
  {312500,8},{312500,7},{300000,7},{280000,7},{200000,8},{156250,8}
};
static uint32_t audCarrier = 0;
static uint8_t  audShift = 0;
static int32_t  audMid = 128, audMax = 255;
static hw_timer_t* audTmr = nullptr;

static inline void audWrite(uint32_t d) {
#if AUD_CORE3
  ledcWrite(PIN_SPK, d);
#else
  ledcWrite(0, d);
#endif
}

// ================= КОЛЬЦЕВОЙ БУФЕР =================
static const uint32_t RBM = 1023;
static int16_t audRb[RBM + 1];
static volatile uint32_t audHead = 0, audTail = 0;
static volatile bool     audFlush = false;
static volatile bool     audPlaying = false, audWavDone = false;
static volatile int32_t  audEnv = 0;
static volatile uint32_t audUnder = 0, audDrop = 0;
static uint32_t audLastDuty = 0xFFFFFFFF;
static uint32_t audMuteUntil = 0;

static inline uint32_t audFill() { return audHead - audTail; }
static inline bool audPush(int16_t s) {
  if (audFill() > RBM) { audDrop++; return false; }
  audRb[audHead & RBM] = s; audHead++; return true;
}

void IRAM_ATTR audTickISR() {
  if (audFlush) { audTail = audHead; audFlush = false; audLastDuty = 0xFFFFFFFF; audEnv = 0; }
  int32_t s = 0; uint32_t d;
  if (audHead != audTail) {
    s = audRb[audTail & RBM]; audTail++;
    int32_t v = audMid + (s >> audShift);
    if (v < 0) v = 0; else if (v > audMax) v = audMax;
    d = (uint32_t)v;
  } else { if (audPlaying) audUnder++; d = (uint32_t)audMid; }
  int32_t a = (s < 0) ? -s : s;                   // огибающая: атака мгновенная
  int32_t e = audEnv;
  if (a > e) e = a;
  else { int32_t dec = (e >> 8) + 1; e = (e > dec) ? (e - dec) : 0; }
  audEnv = e;
  if (d != audLastDuty) { audLastDuty = d; audWrite(d); }
}

static inline int16_t audClip(int32_t x) {
  const int32_t K = 90, L = 127;
  int32_t sg = (x < 0) ? -1 : 1;
  if (x < 0) x = -x;
  if (x > K) { int32_t o = x - K; x = K + (o * (L - K)) / (o + (L - K)); }
  return (int16_t)(sg * x);
}

// ================= WAV ИЗ МАССИВА =================
struct AudWav { uint32_t rate = 0, dataBytes = 0, dataOff = 0; uint16_t ch = 0, bits = 0; };
static AudWav aw;
static const uint8_t* wsrc = nullptr;
static uint32_t wsize = 0, wpos = 0, wLeft = 0, wStep = 0, wFrac = 0;
static int32_t  wPrev = 0, wCur = 0;
static int      wFrame = 2;
static uint16_t wGain = 256;

static bool audParse() {
  if (wsize < 44 || memcmp(wsrc, "RIFF", 4) || memcmp(wsrc + 8, "WAVE", 4)) return false;
  uint32_t p = 12; bool gotFmt = false;
  while (p + 8 <= wsize) {
    uint32_t sz = wsrc[p+4] | (wsrc[p+5] << 8) | (wsrc[p+6] << 16) | ((uint32_t)wsrc[p+7] << 24);
    if (!memcmp(wsrc + p, "fmt ", 4) && sz >= 16 && p + 24 <= wsize) {
      const uint8_t* g = wsrc + p + 8;
      uint16_t tag = g[0] | (g[1] << 8);
      aw.ch   = g[2] | (g[3] << 8);
      aw.rate = g[4] | (g[5] << 8) | (g[6] << 16) | ((uint32_t)g[7] << 24);
      aw.bits = g[14] | (g[15] << 8);
      if (tag != 1 || (aw.bits != 8 && aw.bits != 16) || aw.ch < 1 || aw.ch > 2) return false;
      gotFmt = true;
    } else if (!memcmp(wsrc + p, "data", 4)) {
      aw.dataOff = p + 8;
      uint32_t rest = wsize - aw.dataOff;
      aw.dataBytes = (sz < rest) ? sz : rest;
      return gotFmt;
    }
    p += 8 + sz + (sz & 1);
  }
  return false;
}

static bool audFrame(int32_t& v) {
  if (wLeft < (uint32_t)wFrame) return false;
  const uint8_t* b = wsrc + wpos;
  wpos += wFrame; wLeft -= wFrame;
  if (aw.bits == 16) {
    v = ((int16_t)(b[0] | (b[1] << 8))) >> 8;
    if (aw.ch == 2) v = (v + (((int16_t)(b[2] | (b[3] << 8))) >> 8)) / 2;
  } else {
    v = (int32_t)b[0] - 128;
    if (aw.ch == 2) v = (v + ((int32_t)b[2] - 128)) / 2;
  }
  return true;
}

static bool audRewind() {
  wpos = aw.dataOff; wLeft = aw.dataBytes; wFrac = 0;
  if (!audFrame(wCur)) return false;
  wPrev = wCur;
  return true;
}

void audioStop() { audPlaying = false; audWavDone = true; audFlush = true; }

bool audioStartWav() {
#if HAVE_EMBEDDED_WAV
  audioStop();
  wsrc = WAV_DATA; wsize = WAV_LEN;
  if (!audParse()) return false;
  wFrame = (aw.bits / 8) * aw.ch;
  wStep  = (uint32_t)(((uint64_t)aw.rate << 16) / AUD_SR);
  if (!audRewind()) return false;
  int32_t pk = 0, v;                              // запись тихая -> нормализуем пик
  while (audFrame(v)) { int32_t a = (v < 0) ? -v : v; if (a > pk) pk = a; }
  if (!audRewind()) return false;
  uint32_t g = (pk > 0) ? ((uint32_t)AUDIO_TARGET_PEAK * 256u / (uint32_t)pk) : 256u;
  if (g < 256u) g = 256u;
  if (g > 8192u) g = 8192u;
  wGain = (uint16_t)((g * AUDIO_VOLUME) / 100u);
  audWavDone = false; audFlush = true; audPlaying = true;
  return true;
#else
  return false;
#endif
}

static bool audFeed() {
  if (audFill() > 768) { vTaskDelay(1); return true; }
  for (int i = 0; i < 128; i++) {
    while (wFrac >= 65536) {
      wPrev = wCur;
      if (!audFrame(wCur)) return false;
      wFrac -= 65536;
    }
    int32_t v = wPrev + (((wCur - wPrev) * (int32_t)wFrac) >> 16);
    v = (v * (int32_t)wGain) >> 8;
    audPush(audClip(v));
    wFrac += wStep;
  }
  return true;
}

static void audPlayTask(void*) {
  for (;;) {
    if (audPlaying && !audWavDone) {
      if (!audFeed()) { audWavDone = true; vTaskDelay(pdMS_TO_TICKS(10)); }
    } else {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

// ================= ДЕТЕКТОР ФРАЗЫ =================
struct VtSyl { uint16_t dur; uint16_t gap; uint8_t amp; };

class VoiceTrigger {
public:
  void reset() { _st = false; _in = false; _n = 0; _sm = 0; _qm = 0; _um = 0; _gap = 0; _amp = 0; }

  bool feed(uint8_t lvl) {
    int32_t nz = _nq >> 6;
    int32_t th = nz * 3;
    if (th < VT_ON_MIN) th = VT_ON_MIN;
    if (th > 90) th = 90;
    int32_t off = th * 2 / 3;
    if (off < nz + 5) off = nz + 5;
    if (!_st) _nq += (((int32_t)lvl << 6) - _nq) >> 4;   // пол шума: только вне фразы
    if (_cal > 0) { _cal -= AUD_FRAME_MS; return false; }

    if (!_st) {
      if (lvl >= th) {
        _st = true; _in = true; _sm = AUD_FRAME_MS; _amp = lvl;
        _gap = 0; _um = AUD_FRAME_MS; _qm = 0; _n = 0;
      }
      return false;
    }
    _um += AUD_FRAME_MS;
    if (_in) {
      if (lvl >= off) {
        _sm += AUD_FRAME_MS;
        if (lvl > _amp) _amp = lvl;
        if (_sm > VT_SYL_MAX_MS) reset();               // не слоговая структура
        return false;
      }
      _in = false; _qm = 0;
      if (_sm >= VT_SYL_MIN_MS) {
        if (_n < 8) { _s[_n].dur = _sm; _s[_n].gap = _gap; _s[_n].amp = (uint8_t)_amp; _n++; }
        else reset();
      }
      return false;
    }
    _qm += AUD_FRAME_MS;
    if (lvl >= th) {
      _in = true; _gap = _qm; _sm = AUD_FRAME_MS; _amp = lvl; _qm = 0;
      return false;
    }
    if (_qm >= VT_END_MS) {
      bool ok = _match();
      _lastN = _n;
      for (uint8_t i = 0; i < _n; i++) _last[i] = _s[i];
      reset();
      return ok;
    }
    if (_um > VT_SPEECH_MAX_MS + VT_END_MS) reset();
    return false;
  }

  uint8_t noise() const { return (uint8_t)(_nq >> 6); }
  uint8_t threshold() const {
    int32_t t = (_nq >> 6) * 3;
    if (t < VT_ON_MIN) t = VT_ON_MIN;
    if (t > 90) t = 90;
    return (uint8_t)t;
  }
  uint8_t lastN() const { return _lastN; }
  VtSyl   last(uint8_t i) const { return _last[i < 8 ? i : 0]; }

private:
  int32_t _nq = 4 << 6;                 // пол шума в Q6
  int32_t _cal = VT_CAL_MS;
  bool _st = false, _in = false;
  uint8_t _n = 0, _lastN = 0;
  VtSyl _s[8], _last[8];
  int32_t _sm = 0, _qm = 0, _um = 0, _gap = 0, _amp = 0;

  bool _match() {
    int32_t speech = _um - _qm;
    if (_n < VT_MIN_SYL || _n > VT_MAX_SYL) return false;
    if (speech < VT_SPEECH_MIN_MS || speech > VT_SPEECH_MAX_MS) return false;
#if VOICE_TRIG_MODE == 0
    return true;
#else
    uint8_t groups = 1, k = 0;
    for (uint8_t i = 1; i < _n; i++) {
      if (_s[i].gap >= VT_GAP_WORD_MS) { groups++; if (!k) k = i; }
    }
    if (groups != 2) return false;                    // «привет» | «Дракоша»
    uint8_t g1 = k, g2 = _n - k;
    return (g1 >= 1 && g1 <= 2 && g2 >= 2 && g2 <= 3);
#endif
  }
};

static VoiceTrigger vt;
static volatile bool vtFlag = false;
static volatile uint32_t vtCount = 0;
static uint32_t vtCooldownUntil = 0;

// ================= МИКРОФОН =================
static bool audMicOk = false;
static volatile uint8_t audMicLvl = 0;

static bool audMicInit() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = (int)AUD_SR,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8, .dma_buf_len = 256,
    .use_apll = false, .tx_desc_auto_clear = false, .fixed_mclk = 0
  };
  i2s_pin_config_t p = {
    .bck_io_num = PIN_MIC_SCK, .ws_io_num = PIN_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = PIN_MIC_SD
  };
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &p) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

static void audMicTask(void*) {
  static int32_t raw[256];
  int32_t dc = 0;
  uint32_t nonzero = 0, checks = 0;
  for (;;) {
    size_t n = 0;
    if (i2s_read(I2S_NUM_0, raw, sizeof(raw), &n, portMAX_DELAY) != ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(10)); continue;
    }
    int cnt = (int)(n / 4);
    int32_t pk = 0;
    for (int i = 0; i < cnt; i++) {
      int32_t s = raw[i] >> 8;                  // 24 бита знаковые
      dc += (s - dc) >> 10;                     // ФВЧ ~2.5 Гц
      int32_t v = (s - dc) >> 8;                // шкала ±32768
      if (v != 0) nonzero++;
      int32_t a = (v < 0) ? -v : v;
      if (a > pk) pk = a;
    }
    int32_t lvl = (pk * VT_GAIN) >> 8;
    if (lvl > 255) lvl = 255;
    audMicLvl = (uint8_t)lvl;

    if (checks < 125) {                         // ~2 с: живой ли микрофон
      checks++;
      if (checks == 125 && nonzero < 50) { audMicOk = false; }
    }
#if VOICE_TRIG_ENABLE
    if (audPlaying || millis() < audMuteUntil || millis() < vtCooldownUntil) {
      vt.reset();                               // не слушаем сами себя
      continue;
    }
    if (audMicOk && vt.feed((uint8_t)lvl)) {
      vtFlag = true; vtCount++;
      vtCooldownUntil = millis() + VT_COOLDOWN_MS;
    }
#endif
  }
}

// ================= API =================
bool audioBegin() {
  pinMode(PIN_SPK, OUTPUT);
  digitalWrite(PIN_SPK, LOW);
  for (uint8_t i = 0; i < sizeof(AUD_C) / sizeof(AUD_C[0]); i++) {
#if AUD_CORE3
    bool ok = ledcAttach(PIN_SPK, AUD_C[i].f, AUD_C[i].bits);
    uint32_t f = ok ? AUD_C[i].f : 0u;
#else
    uint32_t f = ledcSetup(0, AUD_C[i].f, AUD_C[i].bits);
    if (f) ledcAttachPin(PIN_SPK, 0);
#endif
    if (f >= 150000u) {
      audCarrier = f;
      audMid  = (1 << AUD_C[i].bits) / 2;
      audMax  = (1 << AUD_C[i].bits) - 1;
      audShift = 8 - AUD_C[i].bits;
      break;
    }
  }
  audWrite((uint32_t)audMid);
  if (!audCarrier) return false;

#if AUD_CORE3
  audTmr = timerBegin(2000000);
  timerAttachInterrupt(audTmr, &audTickISR);
  timerAlarm(audTmr, 125, true, 0);
#else
  audTmr = timerBegin(0, 40, true);
  timerAttachInterrupt(audTmr, &audTickISR, true);
  timerAlarmWrite(audTmr, 125, true);
  timerAlarmEnable(audTmr);
#endif
  xTaskCreatePinnedToCore(audPlayTask, "aud", 4096, NULL, 3, NULL, 1);

#if USE_MIC
  audMicOk = audMicInit();
  if (audMicOk) xTaskCreatePinnedToCore(audMicTask, "mic", 4096, NULL, 2, NULL, 1);
#endif
  return true;
}

void audioUpdate() {
  static bool wasPlaying = false;
  if (audPlaying && audWavDone && audFill() == 0) audPlaying = false;
  if (wasPlaying && !audPlaying) audMuteUntil = millis() + 500;
  wasPlaying = audPlaying;
  if (audPlaying) vtFlag = false;
}

bool    audioBusy()      { return audPlaying; }
uint8_t audioLevel()     { int32_t e = audEnv; if (e > 127) e = 127; return (uint8_t)e; }
uint8_t audioMicLevel()  { return audMicLvl; }
bool    audioMicOk()     { return audMicOk; }
uint32_t audioUnderrun() { return audUnder; }

class AudioEngine {
public:
  bool    begin()      { return audioBegin(); }
  void    update()     { audioUpdate(); }
  bool    playHello()  { return audioStartWav(); }
  void    stop()       { audioStop(); }
  bool    busy()       { return audioBusy(); }
  uint8_t level()      { return audioLevel(); }
  uint8_t mic()        { return audioMicLevel(); }
  bool    micReady()   { return audioMicOk(); }
  uint8_t micNoise()   { return vt.noise(); }
  uint8_t micTh()      { return vt.threshold(); }
  uint32_t underrun()  { return audioUnderrun(); }
  bool    voiceHeard() { bool f = vtFlag; vtFlag = false; return f; }
  uint32_t voiceCount() { return vtCount; }
  uint8_t vtLastN()    { return vt.lastN(); }
  VtSyl   vtLast(uint8_t i) { return vt.last(i); }
};