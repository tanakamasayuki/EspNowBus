#include <EspNowBus.h>

// en: Demonstrate auto-purge on consecutive AppAckTimeout/SendFailed.
// ja: AppAckTimeout/SendFailed が続いたピアを自動パージするデモ。

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry)
{
  Serial.printf("RX len=%u retry=%d\n", (unsigned)len, wasRetry);
}

void onSendResult(const uint8_t *mac, EspNowBus::SendStatus status)
{
  Serial.printf("Send status=%d\n", (int)status);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-purge";
  cfg.maxAckFailures = 3;      // en/ja: 3回連続の失敗でパージ
  cfg.failureWindowMs = 20000; // en/ja: 20秒窓
  cfg.rejoinAfterPurge = true; // en/ja: パージ後に再JOIN要求

  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }

  bus.sendRegistrationRequest();
}

void loop()
{
  static uint32_t lastJoin = 0;
  static uint32_t lastSend = 0;

  if (millis() - lastJoin > 5000)
  {
    lastJoin = millis();
    bus.sendRegistrationRequest();
  }

  if (millis() - lastSend > 3000)
  {
    lastSend = millis();
    size_t peers = bus.peerCount();
    if (peers == 0)
    {
      Serial.println("no peers");
      return;
    }
    uint8_t target[6];
    if (bus.getPeer(0, target))
    {
      const char msg[] = "ping";
      bus.sendTo(target, msg, sizeof(msg));
    }
  }
}
