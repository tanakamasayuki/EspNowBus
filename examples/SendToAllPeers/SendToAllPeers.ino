#include <EspNowBus.h>

// en: Send a group-wide message using sendToAllPeers (unicast to each known peer).
// ja: sendToAllPeers でグループ内の全ピアへ同報（各ピアにユニキャスト）する例。
// en: Broadcast is lighter, but sendToAllPeers uses per-peer unicast: encryption, keyAuth HMAC, and app-level ACK for delivery assurance.
// ja: ブロードキャストの方が軽いが、sendToAllPeers は各ピアへユニキャストするため暗号化や keyAuth/HMAC、論理ACKで到達確認ができる。

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry)
{
  // en: Print sender and payload; app-ACK is auto-sent when enabled.
  // ja: 送信元とペイロードを表示。AppAck は有効時に自動返信。
  Serial.printf("RX from %02X:%02X:%02X:%02X:%02X:%02X len=%u retry=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (unsigned)len, wasRetry);
}

void onSendResult(const uint8_t *mac, EspNowBus::SendStatus status)
{
  // en: Report send status
  // ja: 送信ステータスを表示
  Serial.printf("Send to %02X:%02X:%02X:%02X:%02X:%02X status=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (int)status);
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
  cfg.groupName = "espnow-group";

  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);
  bus.onAppAck(onAppAck);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }

  // en: request registration so peers add us
  // ja: 登録を依頼して他ピアに追加してもらう
  bus.sendRegistrationRequest();
}

void loop()
{
  static uint32_t lastJoin = 0;
  static uint32_t lastSend = 0;

  // en: Periodically ask others to register us (helps when peers reboot)
  // ja: 定期的にピア登録を依頼（相手が再起動しても再登録できるように）
  if (millis() - lastJoin > 5000)
  {
    lastJoin = millis();
    bus.sendRegistrationRequest();
  }

  if (millis() - lastSend > 4000)
  {
    lastSend = millis();
    size_t peers = bus.peerCount();
    if (peers == 0)
    {
      Serial.println("no peers yet");
      return;
    }
    const char msg[] = "hello all peers";
    bus.sendToAllPeers(msg, sizeof(msg));
  }
}
