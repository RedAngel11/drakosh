#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"

class NetworkClient {
public:
    void begin() {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        int attempts = 0;
        // Ждем подключения, но не бесконечно
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            attempts++;
        }
    }

    String pollCommand() {
        if (WiFi.status() != WL_CONNECTED) {
            return "none";
        }

        // Опрашиваем реле не чаще чем раз в 1 секунду
        if (millis() - _lastPollTime < _pollInterval) {
            return "none"; 
        }
        _lastPollTime = millis();

        WiFiClientSecure client;
        client.setInsecure(); // Игнорируем SSL сертификаты для экономии ресурсов

        HTTPClient http;
        http.begin(client, RELAY_PULL_URL);
        http.setTimeout(3000);

        int httpCode = http.GET();
        String command = "none";

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            if (!deserializeJson(doc, payload)) {
                command = doc["cmd"].as<String>();
            }
        }

        http.end();
        return command;
    }

private:
    unsigned long _lastPollTime = 0;
    const unsigned long _pollInterval = 1000; // 1 секунда
};