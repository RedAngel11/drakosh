#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Adafruit_NeoPixel.h>

#include <Arduino.h>
#include <driver/i2s.h>
#include <Adafruit_NeoPixel.h>

// ===== ПИНЫ =====
#define I2S_MIC_SCK   37
#define I2S_MIC_WS    38
#define I2S_MIC_SD    39
#define SPEAKER_PIN   21
#define RGB_PIN       48

// ===== ПАРАМЕТРЫ =====
#define SAMPLE_RATE   16000
#define PWM_FREQ      10000  // 10 кГц (компромисс)
#define PWM_RES       10
#define PWM_CHANNEL   0

Adafruit_NeoPixel rgb(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

// Для программного фильтра высоких частот (убирает постоянное напряжение)
int16_t prevSample = 0;

void setup() {
    rgb.begin();
    rgb.setPixelColor(0, rgb.Color(255, 0, 0)); // КРАСНЫЙ = Инициализация
    rgb.show();

    // 1. I2S (Микрофон)
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_MIC_SCK,
        .ws_io_num = I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_SD
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    // 2. PWM (Динамик)
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
    ledcAttachPin(SPEAKER_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 512); // Тишина

    rgb.setPixelColor(0, rgb.Color(0, 255, 0)); // ЗЕЛЕНЫЙ = Готов
    rgb.show();
}

void loop() {
    int16_t sample = 0;
    size_t bytesRead = 0;
    
    // Читаем 1 сэмпл с микрофона
    i2s_read(I2S_NUM_0, &sample, sizeof(sample), &bytesRead, portMAX_DELAY);

    if (bytesRead > 0) {
        // ДИАГНОСТИКА
        if (sample == 0) {
            rgb.setPixelColor(0, rgb.Color(0, 0, 255)); // СИНИЙ = Нули
        } else {
            // Фиолетовый с разной яркостью
            uint8_t brightness = (abs(sample) / 200 > 255) ? 255 : (abs(sample) / 200);
            rgb.setPixelColor(0, rgb.Color(brightness, 0, brightness)); 
        }
        rgb.show();

        // === ПРОГРАММНЫЙ ФИЛЬТР (Убираем постоянное напряжение) ===
        // Вычисляем разницу между текущим и прошлым сэмплом
        int16_t acSample = sample - prevSample;
        prevSample = sample;

        // === УСИЛЕНИЕ И КОНВЕРТАЦИЯ В PWM ===
        // Умножаем на 4, чтобы сделать звук громче
        int32_t amplified = acSample * 4; 
        
        // Ограничиваем, чтобы не вылезло за пределы 16 бит
        if (amplified > 32767) amplified = 32767;
        if (amplified < -32768) amplified = -32768;

        // Переводим из 16-битного знакового (-32768..32767) в 10-битный PWM (0..1023)
        int duty = ((amplified + 32768) * 1023) / 65535;
        
        // Защита от выхода за границы
        if (duty < 0) duty = 0;
        if (duty > 1023) duty = 1023;

        // Подаем на динамик
        ledcWrite(PWM_CHANNEL, duty);
    }
}