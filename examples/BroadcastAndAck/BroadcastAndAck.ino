#include <EspNowBus.h>

// en: Simple periodic broadcast with app-level ACK enabled (default)
// ja: アプリ層ACKを有効にしたシンプルな定期ブロードキャスト

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
  // en: Report send status (app-ACK enabled by default)
  // ja: 送信ステータスを表示（デフォルトで論理ACK待ち）
  Serial.printf("Send status=%d\n", (int)status);
}

void onAppAck(const uint8_t *mac, uint16_t msgId)
{
  // en: Logical ACK received
  // ja: 論理ACK（基本は onSendResult で完了判定）を受信（基本は onSendResult で完了判定）
  Serial.printf("AppAck msgId=%u\n", msgId);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-demo"; // en: Group name for communication / ja: 同じグループ名同士で通信可能

  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);
  bus.onAppAck(onAppAck);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }
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
