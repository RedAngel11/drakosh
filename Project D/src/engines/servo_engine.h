#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "config.h"

class ServoEngine {
public:
    void begin() {
        Wire.begin(47, 48);
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

    // Новый танец:
    // Фаза 1: сервы 0..4 по очереди на 180°
    // Фаза 2: 0 и 4 одновременно возвращаются на 90°
    // Фаза 3: 1, 2, 3 одновременно возвращаются на 90°
    void dance() {
        // Фаза 1: по очереди
        for (int i = 0; i < 5; i++) {
            pwm.setPWM(i, 0, SERVO_180);
            delay(250);
        }
        delay(400);

        // Фаза 2: 0 и 4 одновременно на 90°
        pwm.setPWM(0, 0, SERVO_90);
        pwm.setPWM(4, 0, SERVO_90);
        delay(400);

        // Фаза 3: 1, 2, 3 одновременно на 90°
        pwm.setPWM(1, 0, SERVO_90);
        pwm.setPWM(2, 0, SERVO_90);
        pwm.setPWM(3, 0, SERVO_90);
        delay(400);
    }

    void _resetAll() {
        for (int i = 0; i < SERVO_COUNT; i++) pwm.setPWM(i, 0, SERVO_90);
    }

private:
    Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADDRESS);
};