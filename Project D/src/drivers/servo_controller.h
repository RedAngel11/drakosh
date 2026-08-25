#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "config.h"

class ServoController {
public:
    void begin() {
        Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
        _pwm.begin();
        _pwm.setOscillatorFrequency(27000000);
        _pwm.setPWMFreq(PCA9685_FREQ);
        delay(100);
        
        // Инициализация всех серв в центральное положение
        Serial.println("🦾 Инициализация сервоприводов...");
        setAllServosToCenter();
        Serial.println("✅ Сервоприводы готовы");
    }

    // Плавное движение одной сервы
    void moveServo(uint8_t channel, uint16_t from, uint16_t to, int delayMs = SERVO_MOVE_DELAY) {
        if (from < to) {
            for (uint16_t pos = from; pos <= to; pos++) {
                _pwm.setPWM(channel, 0, pos);
                delay(delayMs);
            }
        } else {
            for (uint16_t pos = from; pos >= to; pos--) {
                _pwm.setPWM(channel, 0, pos);
                delay(delayMs);
            }
        }
    }

    // Движение к позиции (без плавности, быстро)
    void setServoPosition(uint8_t channel, uint16_t pulse) {
        _pwm.setPWM(channel, 0, pulse);
    }

    // Все сервы в центр
    void setAllServosToCenter() {
        for (uint8_t ch = 0; ch < 16; ch++) {
            _pwm.setPWM(ch, 0, SERVO_CENTER_PULSE);
        }
    }

    // Анимация "приветствие" - машем крыльями
    void waveWings() {
        Serial.println(" Машу крыльями!");
        for (int i = 0; i < 3; i++) {
            _pwm.setPWM(SERVO_CHANNEL_LEFT_WING, 0, SERVO_MIN_PULSE);
            _pwm.setPWM(SERVO_CHANNEL_RIGHT_WING, 0, SERVO_MAX_PULSE);
            delay(300);
            _pwm.setPWM(SERVO_CHANNEL_LEFT_WING, 0, SERVO_MAX_PULSE);
            _pwm.setPWM(SERVO_CHANNEL_RIGHT_WING, 0, SERVO_MIN_PULSE);
            delay(300);
        }
        setAllServosToCenter();
    }

    // Проверка связи с PCA9685
    bool checkConnection() {
        Wire.beginTransmission(PCA9685_ADDRESS);
        byte error = Wire.endTransmission();
        return (error == 0);
    }

private:
    Adafruit_PWMServoDriver _pwm = Adafruit_PWMServoDriver(PCA9685_ADDRESS);
};