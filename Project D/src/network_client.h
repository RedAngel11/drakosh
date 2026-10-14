#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "config.h"

class NetworkClient {
public:
    void begin() {
        // Captive portal: если нет сохранённой сети - запустит AP "Drakoshka-Setup"
        // Пользователь подключается телефоном, вводит SSID и пароль
        WiFiManager wm;
        wm.setConfigPortalTimeout(180); // 3 минуты на настройку
        wm.autoConnect("Drakoshka-Setup", "12345678"); // пароль от точки доступа
    }

    String pollCommand() {
        if (WiFi.status() != WL_CONNECTED) return "none";

        if (millis() - _lastPollTime < _pollInterval) return "none";
        _lastPollTime = millis();

        WiFiClientSecure client;
        client.setInsecure();

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
    const unsigned long _pollInterval = 1000;
};