#include <EspNowBus.h>

// en: Demonstrate JOIN callbacks and send status handling (auto-purge removed).
// ja: JOIN のコールバックと送信ステータスを確認する例（自動パージ機能は廃止）。

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

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-purge";

  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);
  bus.onJoinEvent(onJoinEventCb);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }

  bus.sendJoinRequest();
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
    bus.sendJoinRequest();
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
