#include <EspNowBus.h>

// en: Send a group-wide message using sendToAllPeers (unicast to each known peer).
// ja: sendToAllPeers でグループ内の全ピアへ同報（各ピアにユニキャスト）する例。
// en: Broadcast is lighter, but sendToAllPeers uses per-peer unicast: encryption, keyAuth HMAC, and app-level ACK for delivery assurance.
// ja: ブロードキャストの方が軽いが、sendToAllPeers は各ピアへユニキャストするため暗号化や keyAuth/HMAC、論理ACKで到達確認ができる。

EspNowBus bus;

void onReceive(const uint8_t* mac, const uint8_t* data, size_t len, bool wasRetry) {
  // en: Print sender and payload
  // ja: 送信元とペイロードを表示
  Serial.printf("RX from %02X:%02X:%02X:%02X:%02X:%02X len=%u retry=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (unsigned)len, wasRetry);
}

void onSendResult(const uint8_t* mac, EspNowBus::SendStatus status) {
  // en: AppAckReceived means logical ACK from that peer
  // ja: AppAckReceived はそのピアから論理ACKが返ったことを示す
  Serial.printf("Send to %02X:%02X:%02X:%02X:%02X:%02X status=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (int)status);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-group";
  cfg.enableAppAck = true; // en/ja: 論理ACKで到達確認

  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);

  if (!bus.begin(cfg)) {
    Serial.println("begin failed");
  }

  // en: request registration so peers add us
  // ja: 登録を依頼して他ピアに追加してもらう
  bus.sendRegistrationRequest();
}

void loop() {
  static uint32_t lastJoin = 0;
  static uint32_t lastSend = 0;

  // en/ja: 定期的にピア登録を依頼
  if (millis() - lastJoin > 5000) {
    lastJoin = millis();
    bus.sendRegistrationRequest();
  }

  if (millis() - lastSend > 4000) {
    lastSend = millis();
    size_t peers = bus.peerCount();
    if (peers == 0) {
      Serial.println("no peers yet");
      return;
    }
    const char msg[] = "hello all peers";
    bus.sendToAllPeers(msg, sizeof(msg));
  }
}
