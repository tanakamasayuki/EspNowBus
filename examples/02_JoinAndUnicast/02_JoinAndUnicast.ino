// en: Join a peer and send unicast messages
// ja: 相手へJoinしてUnicastを送る
#include <EspNowBus.h>

// en: Join peers, then unicast with app-level ACK, choosing a random peer.
// ja: JOIN でピア登録後、論理ACK付きでランダムなピアへユニキャスト送信。
// en: Reception policy: Unicast is accepted only from known peers (added via JOIN/Ack or ensurePeer on first receive).
// ja: 受信ポリシー: ユニキャストは登録済みピアからのみ受理（JOIN/Ack や受信時の ensurePeer で登録）。

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
  // en: Group name for communication
  // ja: 同じグループ名同士で通信可能
  cfg.groupName = "espnow-demo_" __FILE__;

  // en: Register a receive observer; use explicit payload length and copy bytes needed beyond the callback.
  // ja: 受信処理を登録します。データ長を使い、コールバック後に必要なByteはコピーします。
  bus.onReceive(onReceive);

  // en: Initialize this component after its prerequisites; inspect the returned status where provided.
  // ja: 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }
}

void loop()
{
  static uint32_t lastSend = 0;

  if (millis() - lastSend > 3000)
  {
    lastSend = millis();
    size_t peers = bus.peerCount();
    if (peers == 0)
    {
      Serial.println("no peers yet (broadcast JOIN continues)");
      return;
    }

    // en: Pick a random registered peer. Only registered peers can receive unicast (ensurePeer on RX adds them).
    // ja: 登録済みピアからランダム選択。ユニキャストは登録済みピアだけが受信可能（受信時の ensurePeer で登録される）。
    size_t idx = random(peers);
    uint8_t target[6];
    if (!bus.getPeer(idx, target))
    {
      Serial.println("getPeer failed");
      return;
    }
    const char msg[] = "hello peer";
    bus.sendTo(target, msg, sizeof(msg));
  }
}
