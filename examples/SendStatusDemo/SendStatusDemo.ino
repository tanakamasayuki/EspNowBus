#include <EspNowBus.h>

// en: Demonstrate SendStatus handling via switch and logging.
// ja: SendStatus を switch で判定し、状態を出力するデモ。

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry)
{
  // en: Print sender and payload; app-ACK is auto-sent when enabled.
  // ja: 送信元とペイロードを表示。AppAck は有効時に自動返信。
  Serial.printf("RX len=%u retry=%d\n", (unsigned)len, wasRetry);
}

void onSendResult(const uint8_t *mac, EspNowBus::SendStatus status)
{
  // en: AppAckReceived means the peer returned a logical ACK (delivery confirmed).
  // ja: AppAckReceived は相手から論理ACKが返ってきた（到達確認）ことを示す。
  switch (status)
  {
  case EspNowBus::Queued:
    Serial.println("Queued");
    break;
  case EspNowBus::SentOk:
    Serial.println("SentOk (physical send success; app-ACK disabled)");
    break;
  case EspNowBus::SendFailed:
    Serial.println("SendFailed (physical send failed)");
    break;
  case EspNowBus::Timeout:
    Serial.println("Timeout (physical send timeout)");
    break;
  case EspNowBus::DroppedFull:
    Serial.println("DroppedFull (queue full)");
    break;
  case EspNowBus::DroppedOldest:
    Serial.println("DroppedOldest (not used in current implementation)");
    break;
  case EspNowBus::TooLarge:
    Serial.println("TooLarge (len > maxPayloadBytes)");
    break;
  case EspNowBus::Retrying:
    Serial.println("Retrying (resend in progress)");
    break;
  case EspNowBus::AppAckReceived:
    Serial.println("AppAckReceived (logical ACK arrived)");
    break;
  case EspNowBus::AppAckTimeout:
    Serial.println("AppAckTimeout (no logical ACK after retries)");
    break;
  }
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
  cfg.groupName = "espnow-status";
  cfg.enableAppAck = true; // en: Enable logical ACK / ja: 論理ACKを使う

  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);
  bus.onAppAck(onAppAck);

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
      const char msg[] = "status-demo";
      bus.sendTo(target, msg, sizeof(msg));
    }
  }
}
