#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include "light_engine.h"

const char* RELAY_HOST = "helloesp32.ksushat75.workers.dev";

LightEngine light;
WiFiClientSecure client;
bool wifiOk = false;
unsigned long lastReconnect = 0, lastBox = 0;

String relayGet(const String& path) {
  if (!client.connect(RELAY_HOST, 443)) return "";
  client.print("GET " + path + " HTTP/1.1\r\nHost: " + String(RELAY_HOST) + "\r\nConnection: close\r\n\r\n");
  String raw = "";
  unsigned long t0 = millis(), lastRx = millis();
  while (millis() - t0 < 5000) {
    light.update();                 // свет живёт даже во время запроса
    if (client.available()) {
      raw += (char)client.read();
      lastRx = millis();
    } else if (millis() - lastRx > 400) {
      break;                        // 400 мс тишины — ответ получен
    }
    delay(2);
  }
  client.stop();
  int h = raw.indexOf("\r\n\r\n");
  return h >= 0 ? raw.substring(h + 4) : "";
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\nДракошка просыпается...");
  light.begin(48, 1);
  client.setInsecure();

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (wm.autoConnect("Drakoshka_Setup", "drakoshka123")) {
    wifiOk = true;
    Serial.println("Wi-Fi есть! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("нет Wi-Fi — автономно");
  }
}

void loop() {
  light.update();

  if (wifiOk && WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnect > 5000) { lastReconnect = millis(); WiFi.reconnect(); }
  }

  // 📨 почтовый ящик: команды от TG-бота
  if (wifiOk && WiFi.status() == WL_CONNECTED && millis() - lastBox > 1500) {
    lastBox = millis();
    String cmd = relayGet("/box/pop");
    if (cmd.indexOf("light_on") >= 0)  { Serial.println("📨 из TG: посвети!"); light.setEmotion(Emotion::JOY); }
    if (cmd.indexOf("light_off") >= 0) { Serial.println("📨 из TG: погасни");  light.setEmotion(Emotion::OFF); }
  }

  // локальные команды из Serial
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    if      (s.indexOf("радость") >= 0) light.setEmotion(Emotion::JOY);
    else if (s.indexOf("спокой")  >= 0) light.setEmotion(Emotion::CALM);
    else if (s.indexOf("поддерж") >= 0) light.setEmotion(Emotion::SUPPORT);
    else if (s.indexOf("дедлайн") >= 0 || s.indexOf("тревога") >= 0) light.setEmotion(Emotion::ALARM);
    else if (s.indexOf("сон")     >= 0) light.setEmotion(Emotion::SLEEP);
  }

  delay(10);
}