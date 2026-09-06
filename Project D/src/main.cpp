#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

// ===== Настройки ленты =====
#define NUM_LEDS 60  // Количество диодов в ленте
#define BRIGHTNESS 50  // Яркость 0-255 (50 = ~20%, безопасно для питания)

Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

// ===== Состояние =====
bool ledOn = false;

// ===== Цвета =====
struct RGB { uint8_t r, g, b; };

RGB colors[] = {
    {255, 100, 0},    // Оранжевый
    {0, 255, 100},    // Зелёный
    {0, 100, 255},    // Синий
    {255, 0, 100},    // Розовый
    {100, 0, 255},    // Фиолетовый
};
const int NUM_COLORS = sizeof(colors) / sizeof(colors[0]);
int currentColor = 0;

// ===== Функции =====
void setAllPixels(RGB color) {
    uint32_t c = strip.Color(color.r, color.g, color.b);
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, c);
    }
    strip.show();
}

void turnOn() {
    ledOn = true;
    Serial.println("💡 ЛЕНТА ВКЛЮЧЕНА (цвет " + String(currentColor) + ")");
    setAllPixels(colors[currentColor]);
}

void turnOff() {
    ledOn = false;
    Serial.println("🌑 ЛЕНТА ВЫКЛЮЧЕНА");
    setAllPixels({0, 0, 0});
}

// ===== SETUP =====
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Тест кнопки и ленты ===");
    Serial.println("Лента: пин " + String(PIN_LED_STRIP));
    Serial.println("Кнопка: пин " + String(PIN_BUTTON));
    
    // Инициализация ленты
    strip.begin();
    strip.setBrightness(BRIGHTNESS);
    strip.show();  // Все выключены
    
    // Инициализация кнопки
    pinMode(PIN_BUTTON, INPUT_PULLUP);  // Внутренняя подтяжка к HIGH
    
    Serial.println("Готово! Нажми кнопку BOOT на плате.");
}

// ===== LOOP =====
void loop() {
    // Читаем кнопку (нажатие = LOW, потому что INPUT_PULLUP)
    bool buttonPressed = (digitalRead(PIN_BUTTON) == LOW);
    
    if (buttonPressed) {
        delay(50);  // Антидребезг (debounce)
        
        // Проверяем ещё раз (настоящее нажатие)
        if (digitalRead(PIN_BUTTON) == LOW) {
            if (ledOn) {
                // Если включена — выключаем И меняем цвет для следующего раза
                turnOff();
                currentColor = (currentColor + 1) % NUM_COLORS;
                Serial.println("   Следующий цвет: " + String(currentColor));
            } else {
                // Если выключена — включаем
                turnOn();
            }
            
            // Ждём отпускания кнопки
            while (digitalRead(PIN_BUTTON) == LOW) {
                delay(10);
            }
            delay(200);  // Пауза после отпускания
        }
    }
    
    delay(10);
}