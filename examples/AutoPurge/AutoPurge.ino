#include <EspNowBus.h>

// en: Demonstrate auto-purge on consecutive AppAckTimeout/SendFailed, with callbacks for join/purge events.
// ja: AppAckTimeout/SendFailed 連続時の自動パージと、JOIN/パージのコールバック例。

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry)
{
  // en: Print sender and payload; app-ACK is auto-sent when enabled.
  // ja: 送信元とペイロードを表示。AppAck は有効時に自動返信。
  Serial.printf("RX len=%u retry=%d\n", (unsigned)len, wasRetry);
}

void onSendResult(const uint8_t *mac, EspNowBus::SendStatus status)
{
  // en: Report send status (app-ACK enabled by default)
  // ja: 送信ステータスを表示（デフォルトで論理ACK待ち）
  Serial.printf("Send status=%d\n", (int)status);
}

void onJoinEventCb(const uint8_t mac[6], bool accepted, bool isAck)
{
  // en: Report join events (accepted/denied, request vs ack)
  // ja: JOIN イベントを表示（受理/拒否、ReqかAckか）
  Serial.printf("JoinEvent mac=%02X:%02X:%02X:%02X:%02X:%02X accepted=%d isAck=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], accepted, isAck);
}

void onPurgeEventCb(const uint8_t mac[6])
{
  // en: Notified when a peer is auto-purged
  // ja: ピアが自動パージされたときに通知
  Serial.printf("Purged mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-purge";
  cfg.maxAckFailures = 3;      // en: 3 consecutive failures to purge / ja: 3回連続の失敗でパージ
  cfg.failureWindowMs = 20000; // en: Reset count if no communication for over 20 seconds / ja: 最後の通信から20秒以上経過していたらカウントリセット
  cfg.rejoinAfterPurge = true; // en: Send registration request after purge / ja: パージ後に再JOIN要求

  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);
  bus.onJoinEvent(onJoinEventCb);
  bus.onPeerPurged(onPurgeEventCb);

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

  // en: Periodically ask others to register us (helps when peers reboot)
  // ja: 定期的にピア登録を依頼（相手が再起動しても再登録できるように）
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
