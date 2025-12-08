#include <EspNowBus.h>

// en: Slave node — does NOT accept registrations, periodically finds masters and sends sensor data to all peers.
// ja: スレーブノード — 登録を受け付けず、マスター探索と全ピア宛て送信でセンサデータを送る。

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
  cfg.autoJoinIntervalMs = 0;                   // en: Disable auto-JOIN / ja: 自動JOIN無効化

  bus.onReceive(onReceive);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }
}

void loop()
{
  static uint32_t lastSend = 0;

  // en: Send sensor-like payload to all known peers
  // ja: センサデータ風のペイロードを全ピアに送信
  if (millis() - lastSend > 5000)
  {
    lastSend = millis();
    size_t peers = bus.peerCount();
    if (peers == 0)
    {
      Serial.println("no peers yet");
      return;
    }

    char msg[32];
    snprintf(msg, sizeof(msg), "value=%lu", (unsigned long)millis());
    bus.sendToAllPeers(msg, strlen(msg) + 1);
  }

  delay(10);
}
