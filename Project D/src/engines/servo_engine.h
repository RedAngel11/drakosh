#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "config.h"

class ServoEngine {
public:
    void begin() {
        Wire.begin(4, 5);
        bool i2cFound = false;
        for (byte addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0 && addr == PCA9685_ADDRESS) {
                i2cFound = true; break;
            }
        }
        if (i2cFound) {
            pwm.begin();
            pwm.setOscillatorFrequency(27000000);
            pwm.setPWMFreq(50);
            _resetAll();
        }
    }

    void test() {
        int servo20 = SERVO_0 + (SERVO_90 - SERVO_0) * 20 / 90;
        
        for (int i = 0; i < 4; i++) {
            pwm.setPWM(i, 0, servo20);
            delay(400);
            pwm.setPWM(i, 0, SERVO_90);
            delay(400);
        }
    }

        // Безопасный танец для 4 сервоприводов (без перегрузки по току)
    void dance() {
        // 0. Гарантируем, что все начинаем с нейтрального положения
        _resetAll();
        delay(300);

        // 1. Движение "волной" по одному сервоприводу
        for (int i = 0; i < 4; i++) {
            pwm.setPWM(i, 0, SERVO_180); // Поворот
            delay(200);
            pwm.setPWM(i, 0, SERVO_90);  // Возврат
            delay(200);
        }
        
        delay(300);

        // 2. Обратная волна
        for (int i = 3; i >= 0; i--) {
            pwm.setPWM(i, 0, SERVO_180); 
            delay(200);
            pwm.setPWM(i, 0, SERVO_90);  
            delay(200);
        }

        // 3. Финальный "поклон" (все немного наклоняются вперед и возвращаются)
        int bowPos = SERVO_0 + 30; // Чуть больше нуля, но не максимум
        for (int i = 0; i < 4; i++) {
            pwm.setPWM(i, 0, bowPos);
        }
        delay(400);
        
        // 4. Возврат в исходное
        _resetAll();
    }

    void _resetAll() {
        for (int i = 0; i < 4; i++) pwm.setPWM(i, 0, SERVO_90);
    }

private:
    Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADDRESS);
};