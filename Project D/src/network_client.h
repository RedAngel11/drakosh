#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "config.h"
class NetworkClient {
public:
  bool begin() {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setSleep(false);
#if defined(WIFI_SSID) && defined(WIFI_PASS)
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 12000) delay(200);
#endif
#if USE_WIFI_MANAGER
    if (WiFi.status() != WL_CONNECTED) {
      WiFiManager wm;
      wm.setConfigPortalTimeout(120);
      wm.setConnectTimeout(15);
      wm.autoConnect("Drakoshka-Setup", "12345678");
    }
#endif
    return WiFi.status() == WL_CONNECTED;
  }

  bool connected() { return WiFi.status() == WL_CONNECTED; }

  String pollCommand() {
    if (!connected()) return "none";
    if (millis() - _lastPoll < POLL_INTERVAL_MS) return "none";
    _lastPoll = millis();

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, RELAY_PULL_URL);
    http.setTimeout(2500);
    http.setReuse(false);

    String command = "none";
    if (http.GET() == HTTP_CODE_OK) {
      String payload = http.getString();
      JsonDocument doc;
      if (!deserializeJson(doc, payload)) command = doc["cmd"].as<String>();
    }
    http.end();
    return command;
  }

private:
  uint32_t _lastPoll = 0;
};