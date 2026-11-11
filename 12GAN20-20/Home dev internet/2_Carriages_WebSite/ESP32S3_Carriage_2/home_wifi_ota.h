#pragma once

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <esp_wifi.h>

// Both sketches keep ESP-NOW on the external access point's fixed channel.
// Reconnection can scan briefly, so it is attempted only while the motor is idle.
class HomeWifiOta {
 public:
  bool begin(void (*stopOutputs)()) {
    stopOutputs_ = stopOutputs;
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    if (!WiFi.mode(WIFI_STA) || esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK) return false;
    attemptConnect();
    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_ATTEMPT_TIMEOUT_MS) {
      delay(20);
    }
    checkConnection();
    if (WiFi.status() != WL_CONNECTED) abandonAttempt();
    return esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) == ESP_OK ||
           WiFi.status() == WL_CONNECTED;
  }

  void service(bool idle) {
    checkConnection();
    if (WiFi.status() == WL_CONNECTED) {
      if (!otaStarted_) startServices();
      ArduinoOTA.handle();
      return;
    }
    if (otaStarted_) {
      ArduinoOTA.end();
      MDNS.end();
      otaStarted_ = false;
    }
    if (updating_) return;
    if (connecting_ && millis() - attemptAt_ >= WIFI_ATTEMPT_TIMEOUT_MS) abandonAttempt();
    if (!connecting_ && idle && millis() - attemptAt_ >= WIFI_RETRY_INTERVAL_MS) attemptConnect();
  }

  bool connecting() const { return connecting_; }
  bool updating() const { return updating_; }

 private:
  void attemptConnect() {
    attemptAt_ = millis();
    connecting_ = true;
    // The external AP must be configured to WIFI_CHANNEL on 2.4 GHz.
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
  }

  void abandonAttempt() {
    WiFi.disconnect(false, false);
    connecting_ = false;
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  }

  void checkConnection() {
    if (WiFi.status() != WL_CONNECTED) return;
    uint8_t primary = 0;
    wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&primary, &secondary) != ESP_OK || primary != WIFI_CHANNEL) {
      Serial.printf("[HOME] AP channel must be %u; rejecting channel %u\n",
                    unsigned(WIFI_CHANNEL), unsigned(primary));
      abandonAttempt();
      return;
    }
    connecting_ = false;
  }

  void startServices() {
    const bool mdns = MDNS.begin(MDNS_HOSTNAME);
    if (mdns) {
      MDNS.enableArduino(3232, true);
    }
    ArduinoOTA.setHostname(OTA_HOSTNAME).setPassword(OTA_PASSWORD).setMdnsEnabled(false);
    ArduinoOTA.onStart([this] {
      updating_ = true;
      if (stopOutputs_) stopOutputs_();
      Serial.println("[OTA] Transfer started; outputs stopped");
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("[OTA] Error %u; reboot before resuming movement\n", unsigned(error));
    });
    ArduinoOTA.begin();
    otaStarted_ = true;
    Serial.printf("[HOME] %s connected: %s, OTA: %s.local\n", WIFI_SSID,
                  WiFi.localIP().toString().c_str(), OTA_HOSTNAME);
  }

  void (*stopOutputs_)() = nullptr;
  uint32_t attemptAt_ = 0;
  bool connecting_ = false;
  bool otaStarted_ = false;
  bool updating_ = false;
};
