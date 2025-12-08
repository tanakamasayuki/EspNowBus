#include <EspNowBus.h>

// en: Minimal-footprint broadcast. AppAck OFF, encryption OFF, peer-auth OFF, payload capped at 250 bytes.
// ja: 最軽量設定のブロードキャスト例。AppAck 無効、暗号化無効、peer認証無効、送信長 250 バイトに制限。

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
  // en: Print sender and length only (AppAck disabled here)
  // ja: 送信元と長さのみ表示（ここでは AppAck 無効）
  Serial.printf("RX from %02X:%02X:%02X:%02X:%02X:%02X len=%u retry=%d bcast=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                static_cast<unsigned>(len), wasRetry, isBroadcast);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-demo_" __FILE__;            // en: group identifier / ja: グループ識別子
  cfg.useEncryption = false;                          // en: encryption OFF / ja: 暗号化無効
  cfg.enablePeerAuth = false;                         // en: peer auth OFF / ja: peer 認証無効
  cfg.enableAppAck = false;                           // en: AppAck OFF (physical ACK only) / ja: AppAck 無効（物理 ACK のみ）
  cfg.maxPayloadBytes = EspNowBus::kMaxPayloadLegacy; // en: cap buffers at 250 bytes / ja: 250 バイトに制限
  cfg.maxQueueLength = 4;                             // en: shrink queue to reduce memory / ja: キュー長を縮小しメモリ削減

  bus.onReceive(onReceive);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
    return;
  }

  Serial.printf("Started with min config: encryption=%d peerAuth=%d appAck=%d maxPayload=%u queue=%u\n",
                cfg.useEncryption, cfg.enablePeerAuth, cfg.enableAppAck,
                static_cast<unsigned>(cfg.maxPayloadBytes), static_cast<unsigned>(cfg.maxQueueLength));
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
