#include <EspNowBus.h>

// en: Example with app-level ACK disabled (enableAppAck=false). onAppAck is set but will not be called.
// ja: 論理ACK無効の例（enableAppAck=false）。onAppAck を設定しても呼ばれないことを確認。

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
  // en: With app-ACK off, SentOk just means physical send success.
  // ja: 論理ACKオフでは SentOk は物理送信成功のみを意味する。
  Serial.printf("Send status=%d\n", (int)status);
}

void onAppAck(const uint8_t *mac, uint16_t msgId)
{
  // en: Should not be called because enableAppAck=false
  // ja: enableAppAck=false のため呼ばれないはず
  Serial.printf("AppAck (unexpected) msgId=%u\n", msgId);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-noappack";
  cfg.enableAppAck = false; // en: disable app-level ACK / ja: 論理ACK無効

  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);
  bus.onAppAck(onAppAck); // en: set, but won't be called / ja: 設定するが呼ばれない

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
      const char msg[] = "no-app-ack";
      bus.sendTo(target, msg, sizeof(msg));
    }
  }
}
