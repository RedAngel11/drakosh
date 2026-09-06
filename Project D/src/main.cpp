#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "config.h" 

// в этот раз мы отлаживаем движение сервопривода, поэтому код для ленты убран
// ===== PCA9685 и Сервоприводы =====
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

#define I2C_SDA 8  
#define I2C_SCL 9  

#define SERVOMIN  150 
#define SERVOMAX  600 
#define SERVO_FREQ 50 
#define SERVO_CHANNEL 0 

uint16_t degreesToPulse(int degrees) {
    return map(degrees, 0, 180, SERVOMIN, SERVOMAX);
}

// ===== Настройки плавности =====
const long MOVE_INTERVAL = 15;  // Задержка в мс между шагами в 1 градус. 
                                // (Меньше значение = быстрее движение. 15мс ~ 66 град/сек)
const long PAUSE_INTERVAL = 1000; // Пауза в мс в конечной точке перед следующим движением

// ===== Конечный автомат (State Machine) =====
enum SequenceState {
    SEQ_IDLE,         // Ждем кнопки
    SEQ_MOVE_TO_135,  // Двигаемся к 135°
    SEQ_MOVE_TO_45,   // Двигаемся к 45°
    SEQ_MOVE_TO_90    // Возвращаемся в 90°
};

SequenceState currentSeq = SEQ_IDLE;

// Переменные для плавного движения
int currentAngle = 90;  // Текущий физический угол сервы
int targetAngle = 90;   // Целевой угол

// Таймеры (неблокирующие)
unsigned long lastMoveMillis = 0;
unsigned long lastPauseMillis = 0;

// ===== SETUP =====
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Дракошка: Плавный тест сервопривода ===");

    pinMode(PIN_BUTTON, INPUT_PULLUP);  

    Wire.begin(I2C_SDA, I2C_SCL);
    pwm.begin();
    pwm.setOscillatorFrequency(27000000);
    pwm.setPWMFreq(SERVO_FREQ);
    delay(10);

    // Инициализация начального положения
    pwm.setPWM(SERVO_CHANNEL, 0, degreesToPulse(currentAngle));
    Serial.println("Серва в центре (90°). Нажмите кнопку BOOT для запуска плавного цикла!");
}

// ===== LOOP =====
void loop() {
    // 1. Обработка нажатия кнопки
    if (digitalRead(PIN_BUTTON) == LOW) {
        delay(50); // Антидребезг
        if (digitalRead(PIN_BUTTON) == LOW) {
            if (currentSeq == SEQ_IDLE) {
                currentSeq = SEQ_MOVE_TO_135;
                targetAngle = 135;
                lastPauseMillis = millis(); // Сбрасываем таймер паузы
                Serial.println("🚀 Начало цикла: цель 135°");
            }
            while (digitalRead(PIN_BUTTON) == LOW) delay(10); // Ждем отпускания
        }
    }

    // 2. Логика движения и пауз
    if (currentSeq != SEQ_IDLE) {
        
        // --- ФАЗА А: Движение к цели ---
        if (currentAngle != targetAngle) {
            if (millis() - lastMoveMillis >= MOVE_INTERVAL) {
                lastMoveMillis = millis();
                
                // Сдвигаем угол на 1 градус в сторону цели
                if (currentAngle < targetAngle) {
                    currentAngle++;
                } else {
                    currentAngle--;
                }
                
                // Отправляем новый угол на драйвер
                pwm.setPWM(SERVO_CHANNEL, 0, degreesToPulse(currentAngle));
            }
        } 
        // --- ФАЗА Б: Достижение цели и пауза ---
        else {
            // Мы достигли targetAngle! Ждем PAUSE_INTERVAL, чтобы пойти дальше.
            if (millis() - lastPauseMillis >= PAUSE_INTERVAL) {
                lastPauseMillis = millis(); // Сбрасываем таймер для следующей паузы
                
                // Переключаем состояние на следующее
                switch (currentSeq) {
                    case SEQ_MOVE_TO_135:
                        currentSeq = SEQ_MOVE_TO_45;
                        targetAngle = 45;
                        Serial.println("🎯 Достигнуто 135°. Цель: 45°");
                        break;
                        
                    case SEQ_MOVE_TO_45:
                        currentSeq = SEQ_MOVE_TO_90;
                        targetAngle = 90;
                        Serial.println("🎯 Достигнуто 45°. Цель: 90°");
                        break;
                        
                    case SEQ_MOVE_TO_90:
                        currentSeq = SEQ_IDLE;
                        Serial.println("🎯 Достигнуто 90°. Цикл завершен!\n");
                        break;
                }
            }
        }
    }
    
    // Небольшая задержка для разгрузки watchdog и Wi-Fi стека ESP32
    delay(1); 
}