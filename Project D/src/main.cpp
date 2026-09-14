#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include "config.h"

#define NUM_LEDS 60
#define BRIGHTNESS 50
#define I2C_SDA 47  
#define I2C_SCL 48  
#define PCA9685_ADDR 0x40
#define SERVO_CHANNEL 0

#define BUTTON_BOOT 0
#define DEBOUNCE_DELAY 50

#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS_LEFT   10
#define TFT_CS_RIGHT  9
#define TFT_DC        8
#define TFT_RST       7
#define TFT_BL        6

#define SCREEN_W 240
#define SCREEN_H 240
#define EYE_RADIUS 100
#define PUPIL_RADIUS 35

// =====================================================
// === ГЛОБАЛЬНЫЕ ОБЪЕКТЫ ==============================
// =====================================================
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADDR);
Adafruit_GC9A01A displayLeft(TFT_CS_LEFT, TFT_DC, TFT_RST);
Adafruit_GC9A01A displayRight(TFT_CS_RIGHT, TFT_DC, TFT_RST);

// =====================================================
// === СОСТОЯНИЯ СИСТЕМЫ ===============================
// =====================================================
enum SystemState {
    STATE_CENTER,
    STATE_LEFT,
    STATE_RIGHT
};

SystemState currentState = STATE_CENTER;

// Переменные для кнопки
bool lastButtonState = HIGH;
bool currentButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// =====================================================
// === ФУНКЦИИ УПРАВЛЕНИЯ КОМПОНЕНТАМИ =================
// =====================================================

// --- Управление лентой ---
void setStripColor(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t c = strip.Color(r, g, b);
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, c);
    strip.show();
}

// --- Управление сервой ---
void moveServoToAngle(int angle) {
    int pwmValue = map(angle, 0, 180, 150, 550);
    pwm.setPWM(SERVO_CHANNEL, 0, pwmValue);
}

// --- Управление глазами ---
void drawEye(Adafruit_GC9A01A &display, int pupilX, int pupilY, uint16_t irisColor) {
    int cx = SCREEN_W / 2;
    int cy = SCREEN_H / 2;
    
    display.fillScreen(GC9A01A_BLACK);
    display.fillCircle(cx, cy, EYE_RADIUS, GC9A01A_WHITE);
    
    int irisX = cx + pupilX * 40;
    int irisY = cy + pupilY * 40;
    display.fillCircle(irisX, irisY, PUPIL_RADIUS + 10, irisColor);
    display.fillCircle(irisX, irisY, PUPIL_RADIUS, GC9A01A_BLACK);
    display.fillCircle(irisX - 10, irisY - 10, 8, GC9A01A_WHITE);
}

void updateEyes(int pupilX, uint16_t irisColor) {
    drawEye(displayLeft, pupilX, 0, irisColor);
    drawEye(displayRight, pupilX, 0, irisColor);
}

// =====================================================
// === ПРИМЕНЕНИЕ СОСТОЯНИЯ КО ВСЕЙ СИСТЕМЕ ============
// =====================================================
void applySystemState() {
    switch(currentState) {
        case STATE_CENTER:
            // Серва
            moveServoToAngle(90);
            // Лента
            setStripColor(0, 255, 0);
            // Глаза
            updateEyes(0, GC9A01A_GREEN);
            break;
            
        case STATE_LEFT:
            moveServoToAngle(45);
            setStripColor(0, 0, 255);
            updateEyes(-1, GC9A01A_BLUE);
            break;
            
        case STATE_RIGHT:
            moveServoToAngle(135);
            setStripColor(255, 255, 0);
            updateEyes(1, GC9A01A_YELLOW);
            break;
    }
}

// =====================================================
// === ПЕРЕХОД В СЛЕДУЮЩЕЕ СОСТОЯНИЕ ===================
// =====================================================
void nextState() {
    switch(currentState) {
        case STATE_CENTER:
            currentState = STATE_LEFT;
            break;
        case STATE_LEFT:
            currentState = STATE_RIGHT;
            break;
        case STATE_RIGHT:
            currentState = STATE_CENTER;
            break;
    }
    applySystemState();
}

// =====================================================
// === ПРОВЕРКА КОМАНД =================================
// =====================================================

// Сейчас: проверка кнопки
bool checkButton() {
    int reading = digitalRead(BUTTON_BOOT);
    
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != currentButtonState) {
            currentButtonState = reading;
            
            if (currentButtonState == LOW) {
                return true;
            }
        }
    }
    
    lastButtonState = reading;
    return false;
}

// В будущем: проверка Telegram
// bool checkTelegramCommand() {
//     // Логика проверки входящих сообщений
//     // Если пришла команда "/left" → currentState = STATE_LEFT
//     // Если пришла команда "/right" → currentState = STATE_RIGHT
//     // Если пришла команда "/center" → currentState = STATE_CENTER
//     // return true если команда получена
// }

// =====================================================
// === ПРОВЕРКА I2C ====================================
// =====================================================
bool checkI2CConnection() {
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0 && address == PCA9685_ADDR) {
            return true;
        }
    }
    return false;
}

void showError() {
    for (int i = 0; i < 3; i++) {
        setStripColor(255, 0, 0);
        delay(300);
        setStripColor(0, 0, 0);
        delay(300);
    }
    setStripColor(255, 0, 0);
}

// =====================================================
// === SETUP ===========================================
// =====================================================
void setup() {
    pinMode(BUTTON_BOOT, INPUT_PULLUP);
    
    strip.begin();
    strip.setBrightness(BRIGHTNESS);
    setStripColor(255, 0, 0);
    delay(500);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    displayLeft.begin();
    displayRight.begin();
    displayLeft.setRotation(0);
    displayRight.setRotation(0);
    displayLeft.fillScreen(GC9A01A_BLACK);
    displayRight.fillScreen(GC9A01A_BLACK);

    Wire.begin(I2C_SDA, I2C_SCL);
    if (!checkI2CConnection()) {
        showError();
        while (1) { delay(1000); }
    }

    pwm.begin();
    pwm.setOscillatorFrequency(27000000);
    pwm.setPWMFreq(50);
    
    applySystemState();
    
    delay(500);
}

// =====================================================
// === LOOP ============================================
// =====================================================
void loop() {
    // Проверяем команды (сейчас кнопка, потом Telegram)
    if (checkButton()) {
        nextState();
    }
    
    // В будущем:
    // if (checkTelegramCommand()) {
    //     applySystemState();
    // }
    
    delay(10);
}