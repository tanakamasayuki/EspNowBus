// en: Select a common explicit Wi-Fi channel
// ja: 共通のWi-Fi チャンネルを明示する
#include <EspNowBus.h>

// en: Show how to set Wi-Fi channel explicitly. Default is auto (from groupName hash);
//     here we intentionally request channel 0 to demonstrate clamping to a valid channel.
// ja: Wi-Fi チャンネルを明示指定する例。デフォルトは groupName から自動生成。
//     ここではチャンネル 0 を指定し、1〜13 に丸められることを確認する。

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
  // en: Print sender and payload; app-ACK is auto-sent when enabled.
  // ja: 送信元とペイロードを表示。AppAck は有効時に自動返信。
  Serial.printf("RX from %02X:%02X:%02X:%02X:%02X:%02X data='%s' len=%u retry=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (const char *)data, (unsigned)len, wasRetry);
}

void printChannel()
{
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) == ESP_OK)
  {
    Serial.printf("Wi-Fi channel now %u (secondary=%d)\n", primary, static_cast<int>(second));
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  // en: Group name for communication
  // ja: 同じグループ名同士で通信可能
  cfg.groupName = "espnow-demo_" __FILE__;

  // en: By default, channel is auto-picked from the groupName hash (-1).
  //     Here we request channel 0; the library clamps to the valid 1–13 range.
  // ja: デフォルト（-1）は groupName をハッシュしてチャンネル自動決定。
  //     ここでは 0 を指定し、1〜13 にクリップされる例。
  cfg.channel = 0;

  // en: Register a receive observer; use explicit payload length and copy bytes needed beyond the callback.
  // ja: 受信処理を登録します。データ長を使い、コールバック後に必要なByteはコピーします。
  bus.onReceive(onReceive);

  Serial.printf("Requesting channel=%d (auto uses groupName hash)\n", cfg.channel);

  // en: Initialize this component after its prerequisites; inspect the returned status where provided.
  // ja: 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
    return;
  }
}

void loop()
{
  static uint32_t last = 0;
  if (millis() - last > 2000)
  {
    last = millis();
    const char msg[] = "hello on fixed channel";
    bus.broadcast(msg, sizeof(msg));
    printChannel();
  }
}
