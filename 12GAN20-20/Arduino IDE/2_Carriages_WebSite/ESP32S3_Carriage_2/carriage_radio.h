#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <cstring>

enum class PacketType : uint8_t { Hello = 1, Command = 2, Status = 3 };
enum class LightMode : uint8_t { Auto = 0, Off = 1, Red = 2, Green = 3, BlinkRed = 4 };

struct __attribute__((packed)) CarriagePacket {
  uint32_t group;
  uint8_t version;
  PacketType type;
  uint8_t carriageId;
  uint32_t session;
  uint32_t sequence;
  int16_t pwm;
  uint8_t servoPosition;
  LightMode lightMode;
};
static_assert(sizeof(CarriagePacket) == 19, "ESP-NOW packet layout");

constexpr uint8_t ESPNOW_PROTOCOL_VERSION = 2;
constexpr uint8_t REMOTE_CARRIAGE_ID = 2;
constexpr uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = {255, 255, 255, 255, 255, 255};
constexpr uint32_t MAX_FRAME_AGE_MS = 200;

struct RadioFrame {
  CarriagePacket packet;
  uint8_t source[ESP_NOW_ETH_ALEN];
  bool broadcast;
  int8_t rssi;
  uint32_t receivedAt;
};

class CarriageRadio {
 public:
  bool begin(wifi_interface_t interface, uint8_t channel) {
    interface_ = interface;
    channel_ = channel;
    frames_ = xQueueCreate(8, sizeof(RadioFrame));
    if (!frames_) return false;
    if (esp_now_init() != ESP_OK) {
      vQueueDelete(frames_);
      frames_ = nullptr;
      return false;
    }
    if (esp_now_register_recv_cb(receiveCallback) != ESP_OK) {
      esp_now_deinit();
      vQueueDelete(frames_);
      frames_ = nullptr;
      return false;
    }
    return true;
  }

  bool receive(RadioFrame &frame) {
    return frames_ && xQueueReceive(frames_, &frame, 0) == pdTRUE;
  }

  bool send(const uint8_t *destination, const CarriagePacket &packet) {
    if (!frames_) return false;
    if (!esp_now_is_peer_exist(destination)) {
      esp_now_peer_info_t peer{};
      memcpy(peer.peer_addr, destination, ESP_NOW_ETH_ALEN);
      peer.channel = channel_;
      peer.ifidx = interface_;
      if (esp_now_add_peer(&peer) != ESP_OK) return false;
    }
    return esp_now_send(destination, reinterpret_cast<const uint8_t *>(&packet),
                        sizeof(packet)) == ESP_OK;
  }

  void forget(const uint8_t *address) {
    if (frames_ && esp_now_is_peer_exist(address)) esp_now_del_peer(address);
  }

 private:
  static inline QueueHandle_t frames_ = nullptr;
  wifi_interface_t interface_ = WIFI_IF_STA;
  uint8_t channel_ = 0;

  // ESP-NOW calls this on the Wi-Fi task. Handle packets in loop().
  static void receiveCallback(const esp_now_recv_info_t *info, const uint8_t *data, int length) {
    if (!info || !data || length != int(sizeof(CarriagePacket))) return;
    RadioFrame frame{};
    memcpy(&frame.packet, data, sizeof(frame.packet));
    memcpy(frame.source, info->src_addr, ESP_NOW_ETH_ALEN);
    frame.broadcast = memcmp(info->des_addr, BROADCAST_MAC, ESP_NOW_ETH_ALEN) == 0;
    frame.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : -127;
    frame.receivedAt = millis();
    xQueueSend(frames_, &frame, 0);
  }
};
