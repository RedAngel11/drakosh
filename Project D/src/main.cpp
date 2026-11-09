#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Adafruit_NeoPixel.h>

#include "config.h"
#include "engines/servo_engine.h"

ServoEngine servoEngine;
WiFiManager wifiManager;

// Инициализация NeoPixel (1 светодиод, пин из config.h, формат GRB)
Adafruit_NeoPixel pixel(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// Вспомогательная функция для установки цвета
void setColor(uint32_t color) {
    pixel.setPixelColor(0, color);
    pixel.show();
}

void setup() {
    // Инициализация светодиода
    pixel.begin();
    pixel.setBrightness(50); // Яркость 50 из 255, чтобы не слепил
    pixel.clear();
    pixel.show();
    
    // Красный — старт
    setColor(pixel.Color(255, 0, 0));
    delay(500);
    
    // 1. Инициализация сервоприводов
    servoEngine.begin();
    
    // Зеленый — серво инициализированы
    setColor(pixel.Color(0, 255, 0));
    delay(500);
    
    // 2. Тест сервоприводов
    servoEngine.test();
    
    // Желтый — тест завершен
    setColor(pixel.Color(255, 255, 0));
    delay(500);
    
    // 3. Настройка Wi-Fi
    bool res = wifiManager.autoConnect("drakosha-setup", "drakosha123");
    
    if (!res) {
        // Красный мигает — ошибка
        for (int i = 0; i < 5; i++) {
            setColor(pixel.Color(255, 0, 0));
            delay(200);
            pixel.clear(); pixel.show();
            delay(200);
        }
        ESP.restart();
    } else {
        // Синий — Wi-Fi подключен (ждем команд)
        setColor(pixel.Color(0, 0, 255));
    }
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(RELAY_PULL_URL);
        
        int httpCode = http.GET();
        
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                const char* cmd = doc["cmd"];
                
                if (cmd != nullptr && strcmp(cmd, "none") != 0) {
                    // Фиолетовый — команда получена
                    setColor(pixel.Color(128, 0, 128));
                    delay(300);
                    
                    if (strcmp(cmd, "dance") == 0) {
                        // === ПОШАГОВАЯ ОТЛАДКА ТАНЦА ===
                        
                        // Фаза 1: по очереди (Оранжевый)
                        setColor(pixel.Color(255, 165, 0));
                        for (int i = 0; i < 4; i++) {
                            // Здесь вызываем танец по частям, чтобы видеть прогресс
                            // Но пока оставим вызов всей функции, а цвет просто держим
                            delay(250); 
                        }
                        
                        // Пауза между фазами (Розовый)
                        setColor(pixel.Color(255, 105, 180));
                        delay(400);
                        
                        // Фаза 2: 0 и 3 (Пурпурный)
                        setColor(pixel.Color(255, 0, 255));
                        delay(400);
                        
                        // Фаза 3: 1 и 2 (Лаймовый)
                        setColor(pixel.Color(50, 205, 50));
                        delay(400);
                        
                        // Танец завершен (Белый)
                        setColor(pixel.Color(255, 255, 255));
                        delay(500);
                        
                        // Возврат в режим ожидания (Синий)
                        setColor(pixel.Color(0, 0, 255));
                        
                        // Вызываем реальный танец после индикации, 
                        // чтобы убедиться, что код доходит до конца
                        servoEngine.dance();
                    }
                }
            }
        }
        
        http.end();
    } else {
        // Красный — Wi-Fi отключен
        setColor(pixel.Color(255, 0, 0));
        WiFi.reconnect();
    }
    
    delay(1000);
}