/* ===================================================================
 *  ДРАКОШКА · звук на PAM8403, автосценарий «повторюшка»
 *  Сценарий: синус 440 Гц 5 с -> WAV -> слушает 3 с -> повторяет -> ...
 *  Выход : LEDC -> 1к/100 -> 47 мкФ -> LIN
 *  Такт  : таймер 16 кГц (ISR) + кольцевой буфер + кэш duty
 *  Вход  : INMP441, I2S 32 бита, АРУ только на приёме
 *  Отладка: только RGB-диод. Serial выключен.
 * =================================================================== */
#include <Arduino.h>
#include <driver/i2s.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <string.h>
#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define CORE3 1
#else
  #define CORE3 0
#endif

#define DEBUG_SERIAL 0     // 1 = печать в Serial (нужен CDC_ON_BOOT=0)
#define SINE_MS      5000  // длительность синуса
#define REC_MS       3000  // сколько секунд слушаем
#define DEMO_LOOP    0     // 1 = после 5 повторений сценарий начинается заново

#if DEBUG_SERIAL
  #define DBG(...) do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define DBG(...) do {} while (0)
#endif

#if __has_include("wav_data.h")
  #include "wav_data.h"
  #define HAVE_EMBEDDED_WAV 1
#else
  #define HAVE_EMBEDDED_WAV 0
#endif
static const bool kWavEmbedded = (HAVE_EMBEDDED_WAV != 0);

// ===== ПИНЫ =====
// 33..37 заняты Octal PSRAM; 38 = бортовый диод на плате v1.1
static const int PIN_SCK = 4, PIN_WS = 5, PIN_SD = 6;   // INMP441
static const int PIN_SPK = 21;                          // -> делитель -> LIN
static const int PWM_CH  = 0;                           // только core 2.x
#define LED_PIN_A 48
#define LED_PIN_B 38
#define WAV_PATH  "/hello.wav"

static const uint32_t SR = 16000;

// ===== НЕСУЩИЕ: на S3 LEDC тактируется от XTAL 40 МГц =====
struct CarrierOpt { uint32_t freq; uint8_t bits; };
static const CarrierOpt CARRIERS[] = {
  {312500, 8}, {312500, 7}, {300000, 7}, {280000, 7}, {200000, 8}, {156250, 8}
};
static const uint8_t N_CARRIERS = sizeof(CARRIERS) / sizeof(CARRIERS[0]);
static uint8_t  carrierIdx  = 0;
static uint32_t carrierReal = 0;
static uint8_t  pwmBits  = 8;
static int32_t  pwmMid   = 128;
static int32_t  pwmMax   = 255;
static uint8_t  pwmShift = 0;

// ===== ФАЗЫ =====
enum Phase { PH_INIT, PH_SINE, PH_WAV, PH_REC, PH_PLAY };
static volatile Phase phase = PH_INIT;
static volatile bool  wavDone  = false;
static volatile bool  playDone = false;
static uint32_t phaseStart = 0;
static uint8_t  lastErr = 0;
static uint8_t  cycleCount = 0;

// ===== БУФЕР ЗАПИСИ: 3 с = 48 КБ =====
static const uint32_t REC_SAMPLES = (SR * (uint32_t)REC_MS) / 1000u;
static int8_t recBuf[REC_SAMPLES];
static volatile uint32_t recPos = 0, playPos = 0;
static volatile bool     recFull = false;
static volatile int32_t  recPeak = 0;

// ===== АРУ: ограничена, чтобы не разгоняться в тишине =====
static const int16_t GAIN_TAB[5] = {1, 2, 3, 4, 6};
static const uint8_t GAIN_N = 5;
static uint8_t gainIdx = 0;
static volatile int micGain = 1;

// ===== КОЛЬЦЕВОЙ БУФЕР ВЫВОДА: 1024 сэмпла = 64 мс =====
static const uint32_t RB_MASK = 1023;
static int16_t rb[RB_MASK + 1];
static volatile uint32_t rbHead = 0, rbTail = 0;
static volatile bool     flushReq = false;
static volatile uint32_t underrun = 0, dropped = 0, clipCnt = 0;
static volatile int32_t  micPeak = 0;

static inline uint32_t rbFill() { return rbHead - rbTail; }
static inline bool rbPush(int16_t s) {
  if (rbFill() > RB_MASK) { dropped++; return false; }
  rb[rbHead & RB_MASK] = s; rbHead++; return true;
}

// ===== ВЫВОД =====
static inline void spk(uint32_t d) {
#if CORE3
  ledcWrite(PIN_SPK, d);
#else
  ledcWrite(PWM_CH, d);
#endif
}

static void spkInit() {
  pinMode(PIN_SPK, OUTPUT);
  digitalWrite(PIN_SPK, LOW);        // вход усилителя не висит в воздухе
  for (uint8_t i = 0; i < N_CARRIERS; i++) {
#if CORE3
    bool ok = ledcAttach(PIN_SPK, CARRIERS[i].freq, CARRIERS[i].bits);
    uint32_t f = ok ? CARRIERS[i].freq : 0u;
#else
    uint32_t f = ledcSetup(PWM_CH, CARRIERS[i].freq, CARRIERS[i].bits);
    if (f) ledcAttachPin(PIN_SPK, PWM_CH);
#endif
    DBG("[PWM] try %u/%u -> %u\n",
        (unsigned)CARRIERS[i].freq, (unsigned)CARRIERS[i].bits, (unsigned)f);
    if (f >= 150000u) {
      carrierIdx = i; carrierReal = f;
      pwmBits  = CARRIERS[i].bits;
      pwmMid   = (1 << pwmBits) / 2;
      pwmMax   = (1 << pwmBits) - 1;
      pwmShift = 8 - pwmBits;
      break;
    }
  }
  spk((uint32_t)pwmMid);
}

static uint32_t lastDuty = 0xFFFFFFFF;

void IRAM_ATTR onTick() {
  if (flushReq) { rbTail = rbHead; flushReq = false; lastDuty = 0xFFFFFFFF; }
  uint32_t d;
  if (rbHead != rbTail) {
    int32_t s = rb[rbTail & RB_MASK]; rbTail++;
    int32_t v = pwmMid + (s >> pwmShift);
    if (v < 0) v = 0; else if (v > pwmMax) v = pwmMax;
    d = (uint32_t)v;
  } else {
    if (phase != PH_INIT && phase != PH_REC) underrun++;
    d = (uint32_t)pwmMid;
  }
  if (d != lastDuty) { lastDuty = d; spk(d); }   // не дёргаем LEDC зря
}

static inline int16_t softClip(int32_t x) {
  const int32_t K = 90, L = 127;
  int32_t sg = (x < 0) ? -1 : 1; if (x < 0) x = -x;
  if (x > K) { int32_t o = x - K; x = K + (o * (L - K)) / (o + (L - K)); }
  return (int16_t)(sg * x);
}

// ===== ДИОД =====
static Adafruit_NeoPixel ledA(1, LED_PIN_A, NEO_GRB + NEO_KHZ800);
static Adafruit_NeoPixel ledB(1, LED_PIN_B, NEO_GRB + NEO_KHZ800);
static uint8_t baseR = 6, baseG = 6, baseB = 6;
static uint8_t ovR = 0, ovG = 0, ovB = 0; static uint32_t ovUntil = 0;
static uint8_t codeLeft = 0, codeR = 0, codeG = 0, codeB = 0;
static uint32_t codeNext = 0; static bool codeOn = false;
static uint32_t lastColor = 0xFFFFFFFF;

static void ledShow(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  if (c == lastColor) return;
  lastColor = c;
  ledA.setPixelColor(0, ledA.Color(r, g, b)); ledA.show();
  ledB.setPixelColor(0, ledB.Color(r, g, b)); ledB.show();
}
static void ledBase(uint8_t r, uint8_t g, uint8_t b) { baseR = r; baseG = g; baseB = b; }
static void ledFlash(uint8_t r, uint8_t g, uint8_t b, uint16_t ms) {
  ovR = r; ovG = g; ovB = b; ovUntil = millis() + ms;
}
static void ledCode(uint8_t n, uint8_t r, uint8_t g, uint8_t b) {
  codeLeft = n; codeR = r; codeG = g; codeB = b; codeNext = millis(); codeOn = false;
}
static void ledUpdate() {
  uint32_t now = millis();
  if (codeLeft) {
    if ((int32_t)(now - codeNext) >= 0) {
      codeOn = !codeOn;
      if (codeOn) codeNext = now + 90; else { codeNext = now + 170; codeLeft--; }
    }
    ledShow(codeOn ? codeR : 0, codeOn ? codeG : 0, codeOn ? codeB : 0);
    return;
  }
  if ((int32_t)(now - ovUntil) < 0) ledShow(ovR, ovG, ovB);
  else ledShow(baseR, baseG, baseB);
}
static void showCodeBlocking(uint8_t n, uint8_t r, uint8_t g, uint8_t b) {
  ledCode(n, r, g, b);
  while (codeLeft) { ledUpdate(); delay(5); }
  ledUpdate();
}

// ===== МИКРОФОН =====
static bool micOk = false;
static bool micInit() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = (int)SR,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,   // 64 такта BCLK на кадр
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,    // L/R микрофона на GND
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8, .dma_buf_len = 256,
    .use_apll = false, .tx_desc_auto_clear = false, .fixed_mclk = 0
  };
  i2s_pin_config_t p = {
    .bck_io_num = PIN_SCK, .ws_io_num = PIN_WS,
    .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = PIN_SD
  };
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &p) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

static void micTask(void*) {
  static int32_t raw[256];                 // 256 кадров = 16 мс
  int32_t dc = 0;
  for (;;) {
    size_t n = 0;
    i2s_read(I2S_NUM_0, raw, sizeof(raw), &n, portMAX_DELAY);
    int cnt = (int)(n / 4), pk = 0;
    for (int i = 0; i < cnt; i++) {
      int32_t s = raw[i] >> 8;             // 24 бита знаковые
      dc += (s - dc) >> 10;                // ФВЧ ~2.5 Гц
      s = ((s - dc) * micGain) >> 8;       // масштаб ±127
      int32_t a = (s < 0) ? -s : s;
      if (a > pk) pk = a;
      if (a > 127) clipCnt++;
      if (phase == PH_REC) {               // пишем ТОЛЬКО когда динамик молчит
        uint32_t p = recPos;
        if (p < REC_SAMPLES) {
          recBuf[p] = (int8_t)softClip(s);
          recPos = p + 1;
          if (a > recPeak) recPeak = a;
        } else recFull = true;
      }
    }
    if (pk > micPeak) micPeak = pk;
  }
}

// ===== WAV: память прошивки или LittleFS =====
struct WavInfo { uint32_t rate; uint16_t ch, bits; uint32_t dataBytes; };
static fs::File wf; static WavInfo wi;
static const uint8_t* wsrc = nullptr;
static uint32_t wpos = 0, wsize = 0;
static uint32_t bytesLeft, step16, frac;
static int32_t sPrev, sCur;                // НЕ y0/y1: это функции Бесселя
static int frameSize;
static uint8_t fbuf[512]; static size_t fbufLen = 0, fbufPos = 0;
static uint8_t wavErr = 0;
static bool fsOk = false; static uint8_t fsProbe = 0;

static bool srcRead(uint8_t* d, uint32_t n) {
  if (wsrc) {
    if (wpos + n > wsize) return false;
    memcpy(d, wsrc + wpos, n); wpos += n; return true;
  }
  return wf.read(d, n) == n;
}
static bool srcSeek(uint32_t p) {
  if (wsrc) { if (p > wsize) return false; wpos = p; return true; }
  return wf.seek(p);
}
static uint32_t srcPos()   { return wsrc ? wpos : (uint32_t)wf.position(); }
static uint32_t srcAvail() { return wsrc ? (wsize - wpos) : (uint32_t)wf.available(); }
static void     srcClose() { if (!wsrc) wf.close(); }

static bool wavParse(WavInfo &w) {
  uint8_t h[12];
  if (!srcRead(h, 12) || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return false;
  bool gotFmt = false;
  while (srcAvail() >= 8) {
    uint8_t c[8];
    if (!srcRead(c, 8)) return false;
    uint32_t sz = c[4] | (c[5] << 8) | (c[6] << 16) | ((uint32_t)c[7] << 24);
    if (!memcmp(c, "fmt ", 4)) {
      if (sz < 16) return false;
      uint8_t g[16]; if (!srcRead(g, 16)) return false;
      uint16_t tag = g[0] | (g[1] << 8);
      w.ch   = g[2] | (g[3] << 8);
      w.rate = g[4] | (g[5] << 8) | (g[6] << 16) | ((uint32_t)g[7] << 24);
      w.bits = g[14] | (g[15] << 8);
      if (tag != 1 || (w.bits != 8 && w.bits != 16) || w.ch < 1 || w.ch > 2) return false;
      if (sz > 16) srcSeek(srcPos() + (sz - 16) + (sz & 1));
      gotFmt = true;
    } else if (!memcmp(c, "data", 4)) { w.dataBytes = sz; return gotFmt; }
    else srcSeek(srcPos() + sz + (sz & 1));
  }
  return false;
}

static bool nextFrame(int32_t &v) {
  if (bytesLeft < (uint32_t)frameSize) return false;
  uint8_t b[4];
  for (int i = 0; i < frameSize; i++) {
    if (fbufPos >= fbufLen) {
      uint32_t want = (bytesLeft < sizeof(fbuf)) ? bytesLeft : (uint32_t)sizeof(fbuf);
      if (!srcRead(fbuf, want)) return false;
      fbufLen = want; fbufPos = 0;
    }
    b[i] = fbuf[fbufPos++]; bytesLeft--;
  }
  if (wi.bits == 16) {
    v = ((int16_t)(b[0] | (b[1] << 8))) >> 8;
    if (wi.ch == 2) v = (v + (((int16_t)(b[2] | (b[3] << 8))) >> 8)) / 2;
  } else {
    v = (int32_t)b[0] - 128;
    if (wi.ch == 2) v = (v + ((int32_t)b[2] - 128)) / 2;
  }
  return true;
}

static bool wavStart() {
  wavErr = 0;
#if HAVE_EMBEDDED_WAV
  wsrc = WAV_DATA; wsize = WAV_LEN; wpos = 0;
#else
  wsrc = nullptr; wpos = 0; wsize = 0;
  wf = LittleFS.open(WAV_PATH, "r");
  if (!wf) { wavErr = 2; return false; }
#endif
  if (!wavParse(wi)) { wavErr = 3; srcClose(); return false; }
  frameSize = (wi.bits / 8) * wi.ch;
  bytesLeft = wi.dataBytes; fbufLen = 0; fbufPos = 0; frac = 0;
  step16 = (uint32_t)(((uint64_t)wi.rate << 16) / SR);
  if (!nextFrame(sCur)) { wavErr = 3; srcClose(); return false; }
  sPrev = sCur;
  DBG("[WAV] %u Hz %u ch %u bit %u B\n", (unsigned)wi.rate,
      (unsigned)wi.ch, (unsigned)wi.bits, (unsigned)wi.dataBytes);
  return true;
}

static bool wavFeed() {
  if (rbFill() > 768) { vTaskDelay(1); return true; }
  for (int i = 0; i < 128; i++) {
    while (frac >= 65536) { sPrev = sCur; if (!nextFrame(sCur)) return false; frac -= 65536; }
    int32_t v = sPrev + (((sCur - sPrev) * (int32_t)frac) >> 16);
    frac += step16;
    rbPush(softClip(v));
  }
  return true;
}

static bool fsMount() {
  if (LittleFS.begin()) { fsProbe = 1; return true; }
  LittleFS.end();
  if (LittleFS.begin(false, "/littlefs", 10, "littlefs")) { fsProbe = 2; return true; }
  LittleFS.end();
  fsProbe = 9; return false;
}

// ===== ПЛЕЕР: синус / WAV / запись =====
static void playTask(void*) {
  float ph = 0, dph = 2.0f * (float)PI * 440.0f / (float)SR;
  for (;;) {
    if (phase == PH_SINE) {
      while (rbFill() < 512) {
        ph += dph; if (ph > 2.0f * (float)PI) ph -= 2.0f * (float)PI;
        rbPush((int16_t)(100.0f * sinf(ph)));
      }
      vTaskDelay(1);
    } else if (phase == PH_WAV) {
      if (!wavFeed()) { srcClose(); wavDone = true; vTaskDelay(pdMS_TO_TICKS(10)); }
    } else if (phase == PH_PLAY) {
      if (rbFill() > 768) { vTaskDelay(1); continue; }
      for (int i = 0; i < 128; i++) {
        uint32_t p = playPos;
        if (p >= recPos) { playDone = true; break; }
        rbPush((int16_t)recBuf[p]);
        playPos = p + 1;
      }
      if (playDone) vTaskDelay(pdMS_TO_TICKS(10));
    } else vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ===== ПЕРЕКЛЮЧЕНИЕ ФАЗ =====
static void enterPhase(Phase p) {
  phase = PH_INIT;                     // сначала останавливаем оба потока
  vTaskDelay(pdMS_TO_TICKS(3));
  flushReq = true; wavDone = false; playDone = false;
  phaseStart = millis();

  if (p == PH_WAV) {
    if (!kWavEmbedded && !fsOk) { lastErr = 1; ledCode(1, 70, 0, 0); p = PH_REC; }
    else if (!wavStart())       { lastErr = wavErr; ledCode(wavErr, 70, 0, 0); p = PH_REC; }
  }
  if (p == PH_REC) {
    recPos = 0; recPeak = 0; recFull = false;
    if (!micOk) { lastErr = 4; ledCode(4, 70, 0, 0); }
  }
  if (p == PH_PLAY) { playPos = 0; cycleCount++; }

  phase = p;
  if (p == PH_SINE)      ledBase(0, 0, 45);      // синий: синус
  else if (p == PH_WAV)  ledBase(0, 45, 0);      // зелёный: WAV
  else if (p == PH_REC)  ledBase(20, 0, 28);     // фиолетовый: ГОВОРИТЕ
  else if (p == PH_PLAY) ledBase(0, 30, 35);     // бирюзовый: повторяю
  else                   ledBase(6, 6, 6);
  DBG("[PH] -> %d\n", (int)p);
}

// ===== SETUP =====
static hw_timer_t *tmr = nullptr;

void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200); delay(300);
#endif
  ledA.begin(); ledB.begin();
  ledShow(25, 25, 25); delay(250);      // белый: питание есть

  spkInit();
  ledShow(0, 0, 45); delay(250);        // синий: ШИМ настроен

#if CORE3
  tmr = timerBegin(2000000);            // 2 МГц
  timerAttachInterrupt(tmr, &onTick);
  timerAlarm(tmr, 125, true, 0);        // 125 / 2 МГц = 62.5 мкс = 16 кГц
#else
  tmr = timerBegin(0, 40, true);        // 80 МГц / 40 = 2 МГц
  timerAttachInterrupt(tmr, &onTick, true);
  timerAlarmWrite(tmr, 125, true);
  timerAlarmEnable(tmr);
#endif

  micOk = micInit();
  ledShow(0, 45, 0); delay(250);        // зелёный: I2S настроен

  if (kWavEmbedded) fsOk = true; else fsOk = fsMount();
  (void)fsProbe;

  if (micOk) xTaskCreatePinnedToCore(micTask, "mic", 4096, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(playTask, "play", 4096, NULL, 3, NULL, 1);

  if (carrierReal == 0) lastErr = 5;
  ledBase(6, 6, 6);
  if (carrierReal && carrierIdx > 0) showCodeBlocking(carrierIdx + 1, 60, 45, 0);
  if (lastErr) showCodeBlocking(lastErr, 70, 0, 0);
  else         showCodeBlocking(kWavEmbedded ? 4 : 3, 0, 60, 0);

  enterPhase(PH_SINE);
}

// ===== LOOP: сценарий, АРУ, диод =====
void loop() {
  uint32_t now = millis();

  if (phase == PH_SINE && now - phaseStart >= SINE_MS) enterPhase(PH_WAV);
  else if (phase == PH_WAV && (wavDone || now - phaseStart > 60000)) enterPhase(PH_REC);
  else if (phase == PH_REC && (recFull || now - phaseStart >= REC_MS + 200)) {
    if (recPeak > 25) enterPhase(PH_PLAY);   // была речь — повторяем
    else              enterPhase(PH_REC);    // тишина — слушаем снова
  }
  else if (phase == PH_PLAY && (playDone || now - phaseStart > REC_MS + 2000)) {
#if DEMO_LOOP
    if (cycleCount >= 5) { cycleCount = 0; enterPhase(PH_SINE); }
    else enterPhase(PH_REC);
#else
    enterPhase(PH_REC);
#endif
  }

  static uint32_t tVU = 0, tHealth = 0, tErr = 0;
  static uint8_t  agcTick = 0;
  now = millis();

  if (now - tVU >= 100) {
    tVU = now;
    if (phase == PH_REC) {                   // АРУ и VU только на приёме
      int32_t p = micPeak; micPeak = 0;
      uint32_t c = clipCnt; clipCnt = 0;
      uint8_t lvl = (p >= 127) ? 8 : (uint8_t)((p * 8) / 128);
      ledBase((uint8_t)(3 + lvl * 9), 0, (uint8_t)(4 + lvl * 12));
      if (c > 20) ledFlash(70, 0, 0, 110);   // красный = перегруз
      if (++agcTick >= 3) {                  // АРУ раз в 300 мс
        agcTick = 0;
        uint8_t old = gainIdx;
        if (p > 115 && gainIdx > 0) gainIdx--;
        else if (p < 45 && p > 6 && gainIdx < GAIN_N - 1) gainIdx++;
        if (gainIdx != old) { micGain = GAIN_TAB[gainIdx]; ledFlash(40, 30, 0, 60); }
      }
    } else { micPeak = 0; clipCnt = 0; }
  }

  if (now - tHealth >= 2000) {
    tHealth = now;
    uint32_t u = underrun, d = dropped; underrun = 0; dropped = 0;
    DBG("[ST] ph=%d buf=%u under=%u drop=%u\n",
        (int)phase, (unsigned)rbFill(), (unsigned)u, (unsigned)d);
    if (phase != PH_INIT && phase != PH_REC && (u + d) > 320) ledCode(2, 60, 40, 0);
  }

  if (lastErr && now - tErr >= 6000) { tErr = now; ledCode(lastErr, 70, 0, 0); }

  ledUpdate();
}