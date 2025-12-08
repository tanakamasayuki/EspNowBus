#include <EspNowBus.h>

// en: Simple periodic broadcast
// ja: シンプルな定期ブロードキャスト

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
  // en: Print sender and payload
  // ja: 送信元とペイロードを表示
  Serial.printf("RX from %02X:%02X:%02X:%02X:%02X:%02X data='%s' len=%u retry=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (const char *)data, (unsigned)len, wasRetry);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-demo_" __FILE__; // en: Group name for communication / ja: 同じグループ名同士で通信可能
  cfg.autoJoinIntervalMs = 0;              // en: Disable auto-JOIN / ja: 自動JOIN無効化

  bus.onReceive(onReceive);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }
}

void loop()
{
  static uint32_t last = 0;
  if (millis() - last > 2000)
  {
    last = millis();
    const char msg[] = "hello broadcast";
    bus.broadcast(msg, sizeof(msg));
  }
}
