#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include <LittleFS.h>
#include <math.h>
#include "config.h"

class AudioEngine {
public:
    // КОДЫ САМОДИАГНОСТИКИ (показывает лента красными вспышками):
    // 0 — всё в порядке
    // 1 — не установился I2S TX (динамик)
    // 2 — не установился I2S RX (микрофон)
    // 3 — не смонтировалась LittleFS
    // 4 — i2s_write вернул ошибку (красный вместо белого в окне тона)
    int errorCode = 0;

    void begin() {
        if (setupI2SOutput() != ESP_OK) { errorCode = 1; return; }
        if (setupI2SInput()  != ESP_OK) { errorCode = 2; return; }
        if (!LittleFS.begin(true))      { errorCode = 3; }
        _initialized = true;
    }

    void playWavFile(const char* path) {
        if (!_initialized) return;
        File file = LittleFS.open(path, "r");
        if (!file) { playTone(600, 300); return; }
        file.seek(44);
        uint8_t buffer[1024];
        size_t bytesRead;
        while (file.available()) {
            bytesRead = file.read(buffer, sizeof(buffer));
            size_t bytesWritten;
            if (i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY) != ESP_OK) errorCode = 4;
        }
        file.close();
    }

    void playTone(int frequency_Hz, int duration_ms) {
        if (!_initialized) return;
        const int sample_rate = 16000;
        const int samples_count = (sample_rate * duration_ms) / 1000;
        const int CHUNK = 512;
        int16_t buffer[CHUNK];
        int generated = 0;
        while (generated < samples_count) {
            int n = min(CHUNK, samples_count - generated);
            for (int i = 0; i < n; i++) {
                buffer[i] = (int16_t)(0.5 * 32767 * sin(2 * PI * frequency_Hz * (generated + i) / sample_rate));
            }
            size_t written;
            if (i2s_write(I2S_NUM_0, buffer, n * sizeof(int16_t), &written, portMAX_DELAY) != ESP_OK) errorCode = 4;
            generated += n;
        }
    }

    bool checkVoiceActivity() {
        if (!_initialized || errorCode) return false;
        int16_t samples[256];
        size_t bytesRead;
        i2s_read(I2S_NUM_1, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
        long sum = 0;
        int count = bytesRead / 2;
        for (int i = 0; i < count; i++) sum += (long)samples[i] * samples[i];
        return (sqrt(sum / max(1, count)) > VOICE_THRESHOLD);
    }

private:
    bool _initialized = false;

    esp_err_t setupI2SOutput() {
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = 16000,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 1024,
            .use_apll = false,
            .tx_desc_auto_clear = true
        };
        i2s_pin_config_t pin_config = {
            .bck_io_num = I2S_OUT_BCK,
            .ws_io_num = I2S_OUT_WS,
            .data_out_num = I2S_OUT_DATA,
            .data_in_num = I2S_PIN_NO_CHANGE
        };
        esp_err_t e = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
        if (e != ESP_OK) return e;
        return i2s_set_pin(I2S_NUM_0, &pin_config);
    }

    esp_err_t setupI2SInput() {
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = 16000,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = 512,
            .use_apll = false,
            .tx_desc_auto_clear = true
        };
        i2s_pin_config_t pin_config = {
            .bck_io_num = I2S_IN_BCK,
            .ws_io_num = I2S_IN_WS,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = I2S_IN_DATA
        };
        esp_err_t e = i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
        if (e != ESP_OK) return e;
        return i2s_set_pin(I2S_NUM_1, &pin_config);
    }
};