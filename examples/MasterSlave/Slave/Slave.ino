#include <EspNowBus.h>

// en: Slave node — does NOT accept registrations, periodically finds masters and sends sensor data to all peers.
// ja: スレーブノード — 登録を受け付けず、マスター探索と全ピア宛て送信でセンサデータを送る。

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry)
{
  // en: Print sender and payload; app-ACK is auto-sent when enabled.
  // ja: 送信元とペイロードを表示。AppAck は有効時に自動返信。
  Serial.printf("RX from %02X:%02X:%02X:%02X:%02X:%02X data='%s' len=%u retry=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (const char *)data, (unsigned)len, wasRetry);
}

void onSendResult(const uint8_t *mac, EspNowBus::SendStatus status)
{
  // en: Report send status
  // ja: 送信ステータスを表示
  Serial.printf("Send status=%d\n", (int)status);
}

void onAppAck(const uint8_t *mac, uint16_t msgId)
{
  // en: Logical ACK
  // ja: 論理ACK（基本は onSendResult で完了判定）
  Serial.printf("AppAck msgId=%u\n", msgId);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-master";    // en: must match masters / ja: マスターと同じグループ名

  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }

  // en: initial registration request to find masters
  // ja: 起動直後にマスター探索
  bus.sendJoinRequest();
}

void loop()
{
  static uint32_t lastJoinFast = 0;
  static uint32_t lastJoinSlow = 0;
  static uint32_t lastSend = 0;

  size_t peers = bus.peerCount();

  // en: If no peers, search masters more frequently (fast JOIN)
  // ja: ピアがいない時は短周期でマスター探索
  if (peers == 0 && millis() - lastJoinFast > 3000)
  {
    lastJoinFast = millis();
    bus.sendJoinRequest();
  }

  // en: Even when peers exist, occasionally refresh JOIN for multi-master
  // ja: ピアがいてもマルチマスタ想定でたまに探索
  if (millis() - lastJoinSlow > 15000)
  {
    lastJoinSlow = millis();
    bus.sendJoinRequest();
  }

  // en: Send sensor-like payload to all known peers
  // ja: センサデータ風のペイロードを全ピアに送信
  if (peers > 0 && millis() - lastSend > 5000)
  {
    lastSend = millis();
    char msg[32];
    snprintf(msg, sizeof(msg), "value=%lu", (unsigned long)millis());
    bus.sendToAllPeers(msg, strlen(msg) + 1);
  }
}
