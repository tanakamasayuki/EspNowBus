#include <EspNowBus.h>

// en: Master node — accepts registrations and mainly receives data
// ja: マスタノード — 登録を受け付け、主にデータ受信側

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
  // en: Print sender and payload; app-ACK is auto-sent when enabled.
  // ja: 送信元とペイロードを表示。AppAck は有効時に自動返信。
  Serial.printf("RX from %02X:%02X:%02X:%02X:%02X:%02X data='%s' len=%u retry=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (const char *)data, (unsigned)len, wasRetry);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-demo_04_MasterSlave"; // en: Group name for communication / ja: 同じグループ名同士で通信可能
  cfg.autoJoinIntervalMs = 10000;               // en: Auto-JOIN interval(30s->10s) / ja: 自動JOIN間隔(30秒->10秒)

  bus.onReceive(onReceive);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }
}

void loop()
{
  // en: Master mainly receives; no periodic send needed
  // ja: マスタは受信メイン。定期送信なし
  delay(1000);
}
