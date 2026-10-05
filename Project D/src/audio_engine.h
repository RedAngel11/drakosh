#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include <LittleFS.h>
#include "config.h"

class AudioEngine {
public:
    void begin() {
        setupI2SOutput();
        setupI2SInput();
        if (!LittleFS.begin(true)) {
            Serial.println("❌ LittleFS mount failed");
        } else {
            Serial.println("✅ LittleFS initialized");
        }
        _initialized = true;
    }

    void playWavFile(const char* path) {
        if (!_initialized) return;
        File file = LittleFS.open(path, "r");
        if (!file) {
            Serial.println("⚠️ Файл не найден: " + String(path) + ". Проигрываю тестовый тон.");
            playTone(440, 500); // Фолбэк, если файла нет
            return;
        }
        file.seek(44); // Пропускаем WAV заголовок
        uint8_t buffer[1024];
        size_t bytesRead;
        Serial.println("▶️ Воспроизведение: " + String(path));
        while (file.available()) {
            bytesRead = file.read(buffer, sizeof(buffer));
            size_t bytesWritten;
            i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
        }
        file.close();
    }

    void playTone(int frequency_Hz, int duration_ms) {
        if (!_initialized) return;
        const int sample_rate = 16000;
        const int samples_count = (sample_rate * duration_ms) / 1000;
        int16_t buffer[samples_count];
        for (int i = 0; i < samples_count; i++) {
            buffer[i] = (int16_t)(0.5 * 32767 * sin(2 * PI * frequency_Hz * i / sample_rate));
        }
        size_t bytes_written;
        i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &bytes_written, portMAX_DELAY);
    }

    bool checkVoiceActivity() {
        if (!_initialized) return false;
        int16_t samples[256];
        size_t bytesRead;
        i2s_read(I2S_NUM_1, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
        
        long sum = 0;
        int count = bytesRead / 2;
        for (int i = 0; i < count; i++) {
            sum += (long)samples[i] * samples[i];
        }
        long rms = sqrt(sum / count);
        return (rms > VOICE_THRESHOLD);
    }

private:
    bool _initialized = false;
    
    void setupI2SOutput() {
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
        i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
        i2s_set_pin(I2S_NUM_0, &pin_config);
    }
    
    void setupI2SInput() {
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
        // ИСПРАВЛЕНИЕ ОШИБКИ КОМПИЛЯЦИИ ЗДЕСЬ:
        i2s_pin_config_t pin_config = {
            .bck_io_num = I2S_IN_BCK,
            .ws_io_num = I2S_IN_WS,
            .data_out_num = I2S_PIN_NO_CHANGE, // <-- Обязательно для RX!
            .data_in_num = I2S_IN_DATA
        };
        i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
        i2s_set_pin(I2S_NUM_1, &pin_config);
    }
};