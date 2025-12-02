#include <EspNowBus.h>

EspNowBus bus;

void onReceive(const uint8_t* mac, const uint8_t* data, size_t len, bool wasRetry) {
  Serial.printf("From %02X:%02X:%02X:%02X:%02X:%02X len=%d retry=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                (int)len, wasRetry);
}

void onSendResult(const uint8_t* mac, EspNowBus::SendStatus status) {
  Serial.printf("Send to %02X:%02X:%02X:%02X:%02X:%02X status=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (int)status);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-bus-demo";

  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);

  if (!bus.begin(cfg)) {
    Serial.println("EspNowBus begin failed");
  }

  bus.sendRegistrationRequest();
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 1000) {
    last = millis();
    const char msg[] = "hello";
    bus.broadcast(msg, sizeof(msg));
  }
}
