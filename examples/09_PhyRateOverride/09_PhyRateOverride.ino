// en: Choose the physical radio rate
// ja: 無線のPHY Rateを指定する
#include <EspNowBus.h>
#include <esp_wifi.h>

// en: Change ESP-NOW PHY rate. Default is WIFI_PHY_RATE_24M; switch to WIFI_PHY_RATE_1M_L for longer range.
// ja: ESP-NOW の PHY レートを変更する例。デフォルト (WIFI_PHY_RATE_24M) から遠距離向けの WIFI_PHY_RATE_1M_L に変更。

EspNowBus bus;

const char *rateName(wifi_phy_rate_t rate)
{
  switch (rate)
  {
  case WIFI_PHY_RATE_1M_L:
    return "WIFI_PHY_RATE_1M_L";
  case WIFI_PHY_RATE_2M_L:
    return "WIFI_PHY_RATE_2M_L";
  case WIFI_PHY_RATE_11M_L:
    return "WIFI_PHY_RATE_11M_L";
  case WIFI_PHY_RATE_24M:
    return "WIFI_PHY_RATE_24M";
  case WIFI_PHY_RATE_54M:
    return "WIFI_PHY_RATE_54M";
  default:
    return "WIFI_PHY_RATE_*";
  }
}

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

  // en: Default ESP-NOW rate is WIFI_PHY_RATE_24M; set to low-rate WIFI_PHY_RATE_1M_L for better range.
  // ja: 既定の WIFI_PHY_RATE_24M から、遠距離に強い WIFI_PHY_RATE_1M_L へ変更。
  cfg.phyRate = WIFI_PHY_RATE_1M_L;

  // en: Register a receive observer; use explicit payload length and copy bytes needed beyond the callback.
  // ja: 受信処理を登録します。データ長を使い、コールバック後に必要なByteはコピーします。
  bus.onReceive(onReceive);

  Serial.printf("Requesting phyRate=%s (%d)\n", rateName(cfg.phyRate), static_cast<int>(cfg.phyRate));

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
    const char msg[] = "hello at low rate";
    bus.broadcast(msg, sizeof(msg));
  }
}
