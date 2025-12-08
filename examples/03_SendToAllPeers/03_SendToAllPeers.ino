#include <EspNowBus.h>

// en: Send a group-wide message using sendToAllPeers (unicast to each known peer).
// ja: sendToAllPeers でグループ内の全ピアへ同報（各ピアにユニキャスト）する例。
// en: Broadcast is lighter, but sendToAllPeers uses per-peer unicast: encryption, keyAuth HMAC, and app-level ACK for delivery assurance.
// ja: ブロードキャストの方が軽いが、sendToAllPeers は各ピアへユニキャストするため暗号化や keyAuth/HMAC、論理ACKで到達確認ができる。

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
  cfg.groupName = "espnow-demo_" __FILE__; // en: Group name for communication / ja: 同じグループ名同士で通信可能

  bus.onReceive(onReceive);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }
}

void loop()
{
  static uint32_t lastSend = 0;
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
