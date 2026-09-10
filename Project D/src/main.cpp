#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "config.h"

// Лента
#define NUM_LEDS 60
#define BRIGHTNESS 50
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

// PCA9685
#define I2C_SDA 47  
#define I2C_SCL 48  
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
#define SERVO_CHANNEL 0 

void setStripColor(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t c = strip.Color(r, g, b);
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, c);
    strip.show();
}

void setup() {
    // 1. Лента: Красный (Запуск)
    strip.begin();
    strip.setBrightness(BRIGHTNESS);
    setStripColor(255, 0, 0); 
    delay(1000); 

    // 2. I2C и PCA9685
    Wire.begin(I2C_SDA, I2C_SCL);
    pwm.begin();
    pwm.setOscillatorFrequency(27000000);
    pwm.setPWMFreq(50); 
    
    // Ставим серву в центр
    pwm.setPWM(SERVO_CHANNEL, 0, 375); // 375 это примерно 90 градусов
    
    // Лента: Зеленый (Готово, сейчас поедем)
    setStripColor(0, 255, 0); 
    delay(1000);
}

void loop() {
    // Лента: Синий (Движение)
    setStripColor(0, 0, 255); 
    
    // Поворот влево (45 градусов -> импульс ~262)
    pwm.setPWM(SERVO_CHANNEL, 0, 262);
    delay(1500); // Ждем 1.5 сек

    // Поворот вправо (135 градусов -> импульс ~487)
    pwm.setPWM(SERVO_CHANNEL, 0, 487);
    delay(1500); // Ждем 1.5 сек
}