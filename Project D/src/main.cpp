#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "engines/servo_engine.h"

#include <Arduino.h>
#include <LittleFS.h>

#define SPEAKER_PIN 21
#define PWM_CHANNEL 0
#define PWM_FREQ    20000
#define PWM_RES     10

#define VOLUME 0.5

void setup() {
    Serial.begin(115200);
    delay(1000);

    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
    ledcAttachPin(SPEAKER_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 512);

    if (!LittleFS.begin()) {
        Serial.println("LittleFS error!");
        while (true) delay(100);
    }
    Serial.println("Ready.");
}

void playWav(const char* filename) {
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.printf("File %s not found!\n", filename);
        return;
    }

    Serial.println("Playing...");

    // Читаем заголовок WAV
    file.seek(22);
    uint16_t channels = file.read();
    
    file.seek(24);
    uint32_t sampleRate = file.read();
    
    file.seek(34);
    uint16_t bitsPerSample = file.read();
    
    Serial.printf("Ch: %d, SR: %lu Hz, Bits: %d\n", channels, sampleRate, bitsPerSample);

    // Ищем "data"
    file.seek(0);
    while (file.available()) {
        if (file.read() == 'd' && file.read() == 'a' && 
            file.read() == 't' && file.read() == 'a') {
            file.read(); file.read(); file.read(); file.read();
            break;
        }
    }

    // Буфер
    uint8_t buf[4096];
    int bufIdx = 0;
    int bufSize = 0;

    auto readByte = [&]() -> int {
        if (bufIdx >= bufSize) {
            bufSize = file.read(buf, sizeof(buf));
            bufIdx = 0;
            if (bufSize == 0) return -1;
        }
        return buf[bufIdx++];
    };

    uint32_t sampleDelay = 1000000 / sampleRate;

    while (true) {
        int b1 = readByte();
        if (b1 == -1) break;
        
        int b2 = readByte();
        if (b2 == -1) break;
        
        // 16-bit signed little-endian
        int16_t sample = (b2 << 8) | b1;
        
        // Конвертируем в 10-bit PWM
        int duty = ((sample + 32768) * 1023) / 65535;
        duty = 512 + (int)((duty - 512) * VOLUME);
        
        if (duty < 0) duty = 0;
        if (duty > 1023) duty = 1023;

        ledcWrite(PWM_CHANNEL, duty);
        delayMicroseconds(sampleDelay);
    }

    file.close();
    ledcWrite(PWM_CHANNEL, 512);
    Serial.println("Done.");
}

void loop() {
    playWav("/hello.wav");
    delay(3000);
}