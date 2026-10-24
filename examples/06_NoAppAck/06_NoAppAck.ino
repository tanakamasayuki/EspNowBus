// en: Compare sending without application acknowledgements
// ja: Application ACKなしの送信を確認する
#include <EspNowBus.h>

// en: Example with app-level ACK disabled (enableAppAck=false). onAppAck is set but will not be called.
// ja: 論理ACK無効の例（enableAppAck=false）。onAppAck を設定しても呼ばれないことを確認。

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
  // en: Print sender and payload; app-ACK is auto-sent when enabled.
  // ja: 送信元とペイロードを表示。AppAck は有効時に自動返信。
  Serial.printf("RX from %02X:%02X:%02X:%02X:%02X:%02X data='%s' len=%u retry=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (const char *)data, (unsigned)len, wasRetry);
}

void onSendResult(const uint8_t *mac, EspNowBus::SendStatus status)
{
  // en: AppAckReceived means the peer returned a logical ACK (delivery confirmed).
  // ja: AppAckReceived は相手から論理ACKが返ってきた（到達確認）ことを示す。
  Serial.printf("TX to %02X:%02X:%02X:%02X:%02X:%02X status=",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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
  // en: Group name for communication
  // ja: 同じグループ名同士で通信可能
  cfg.groupName = "espnow-demo_" __FILE__;
  cfg.enableAppAck = false;                // en: disable app-level ACK / ja: 論理ACK無効

  // en: Register a receive observer; use explicit payload length and copy bytes needed beyond the callback.
  // ja: 受信処理を登録します。データ長を使い、コールバック後に必要なByteはコピーします。
  bus.onReceive(onReceive);
  // en: Observe asynchronous progress/completion and distinguish physical from application acknowledgement.
  // ja: 非同期の途中経過と完了を確認し、物理送信とApplication ACKを区別します。
  bus.onSendResult(onSendResult);
  bus.onAppAck(onAppAck); // en: set, but won't be called / ja: 設定するが呼ばれない

  // en: Initialize this component after its prerequisites; inspect the returned status where provided.
  // ja: 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }

  bus.sendJoinRequest();
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
