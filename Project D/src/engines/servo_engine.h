#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "config.h"

class ServoEngine {
public:
    void begin() {
        Wire.begin(47, 48); // SDA, SCL
        
        bool i2cFound = false;
        for (byte address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            if (Wire.endTransmission() == 0 && address == PCA9685_ADDRESS) {
                i2cFound = true;
                break;
            }
        }

        if (i2cFound) {
            pwm.begin();
            pwm.setOscillatorFrequency(27000000);
            pwm.setPWMFreq(50); // 50 Гц для сервоприводов
        }
    }

    void dance() {
        // Дергаем по 2-3 сервы, чтобы не перегружать блок питания
        _moveServos({0, 1}, 300, 400);
        delay(300);
        _moveServos({2, 3}, 400, 500);
        delay(300);
        _moveServos({4, 5, 6}, 300, 400);
        delay(300);
        _moveServos({7, 8, 9}, 400, 500);
        delay(300);
        
        // Возврат в исходное положение
        _resetAllServos();
    }

private:
    Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADDRESS);

    void _moveServos(std::initializer_list<int> servos, int minPulse, int maxPulse) {
        for (int num : servos) {
            int pulse = random(minPulse, maxPulse);
            pwm.setPWM(num, 0, pulse);
        }
    }

    void _resetAllServos() {
        for (int i = 0; i < 10; i++) {
            pwm.setPWM(i, 0, 300); // Центральное положение (подстрой под свои сервы)
        }
    }
};