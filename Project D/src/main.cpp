#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>

#include "config.h"
#include "light_engine.h"
#include "drivers/servo_controller.h"
#include "brain/brain.h"
#include "commands/command_handler.h"

// ===== Глобальные объекты =====
LightEngine light;
ServoController servo;
Brain brain;
CommandHandler commandHandler;

WiFiClientSecure client;

// ===== Состояние =====
bool wifiOk = false;
unsigned long lastReconnect = 0;
unsigned long lastBoxCheck = 0;

// ===== Relay функции =====
String relayGet(const String& path) {
    if (!client.connect(RELAY_HOST, RELAY_PORT)) {
        return "";
    }
    
    client.print("GET " + path + " HTTP/1.1\r\n");
    client.print("Host: " + String(RELAY_HOST) + "\r\n");
    client.print("Connection: close\r\n");
    client.print("\r\n");
    
    String raw = "";
    unsigned long startTime = millis();
    unsigned long lastRx = millis();
    
    while (millis() - startTime < 5000) {
        light.update(); // Свет живёт даже во время запроса
        
        if (client.available()) {
            raw += (char)client.read();
            lastRx = millis();
        } else if (millis() - lastRx > 400) {
            break; // 400 мс тишины — ответ получен
        }
        delay(2);
    }
    
    client.stop();
    
    int headerEnd = raw.indexOf("\r\n\r\n");
    return (headerEnd >= 0) ? raw.substring(headerEnd + 4) : "";
}

// ===== SETUP =====
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n=== 🦕 Дракошка просыпается... ===");
    
    // 1. Инициализация света
    Serial.print("💡 Инициализация LED... ");
    light.begin(PIN_LED_STRIP, LED_COUNT);
    Serial.println("✅");
    
    // 2. Инициализация сервоприводов
    Serial.print(" Инициализация PCA9685... ");
    if (servo.checkConnection()) {
        servo.begin();
        Serial.println("✅");
    } else {
        Serial.println("❌ PCA9685 не найден!");
        light.setEmotion(Emotion::ALARM, true);
    }
    
    // 3. Инициализация мозга
    brain.begin(&servo, &light);
    
    // 4. Инициализация обработчика команд
    commandHandler.begin(&brain, &servo);
    
    // 5. Настройка WiFi
    Serial.print("📡 Настройка Wi-Fi... ");
    WiFiManager wm;
    wm.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);
    
    if (wm.autoConnect(WIFI_AP_SSID, WIFI_AP_PASS)) {
        wifiOk = true;
        Serial.println("✅");
        Serial.println("   IP: " + WiFi.localIP().toString());
        brain.setMode(BrainMode::JOY);
        delay(1000);
    } else {
        Serial.println("❌");
        Serial.println("   Работа в автономном режиме");
        brain.setMode(BrainMode::CALM);
    }
    
    brain.setMode(BrainMode::CALM);
    Serial.println("\n=== ✅ Дракошка готов к работе! ===\n");
}

// ===== LOOP =====
void loop() {
    // 1. Обновление света (плавные переходы)
    light.update();
    
    // 2. Обновление мозга (автономное поведение)
    brain.update();
    
    // 3. Переподключение WiFi при необходимости
    if (wifiOk && WiFi.status() != WL_CONNECTED) {
        if (millis() - lastReconnect > RECONNECT_INTERVAL) {
            lastReconnect = millis();
            Serial.println(" Переподключение Wi-Fi...");
            WiFi.reconnect();
        }
    }
    
    // 4. Проверка почтового ящика Telegram
    if (wifiOk && WiFi.status() == WL_CONNECTED && millis() - lastBoxCheck > BOX_POLL_INTERVAL) {
        lastBoxCheck = millis();
        
        String cmd = relayGet("/box/pop");
        if (cmd.length() > 0 && cmd != "empty") {
            Serial.println("📨 Из Telegram: " + cmd);
            commandHandler.handleCommand(cmd);
        }
    }
    
    // 5. Обработка команд из Serial
    commandHandler.handleSerialCommand();
    
    delay(10);
}