// en: Minimal device-side EspNowIP example.
// ja: device 側 EspNowIP の最小構成サンプル。

#include <Arduino.h>
#include <EspNowIP.h>
#include <esp_netif.h>
#include <esp_wifi.h>

EspNowIP ip;
static uint32_t lastInfoMs = 0;

static void printSelfInfo()
{
  uint8_t mac[6]{};
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  uint8_t primary = 0;

  if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK)
  {
    Serial.printf("selfMac=%02X:%02X:%02X:%02X:%02X:%02X ",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  if (esp_wifi_get_channel(&primary, &second) == ESP_OK)
  {
    Serial.printf("channel=%u second=%d", primary, static_cast<int>(second));
  }
  else
  {
    Serial.print("channel=?");
  }
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("01_DeviceBasic");
  Serial.println("Minimal EspNowIP device example.");

  EspNowIP::Config cfg;
  cfg.groupName = "espnow-ip-demo";
  cfg.mtu = 1420;
  // cfg.channel = 6;                  // Optional: fix the channel on all nodes when needed.
  // cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional: lower PHY rate example for longer range.

  Serial.printf("groupName=%s mtu=%u\n", cfg.groupName, cfg.mtu);
  printSelfInfo();
  Serial.println();

  if (!ip.begin(cfg))
  {
    Serial.println("ip.begin failed");
  }
}

void loop()
{
  ip.poll();

  uint32_t now = millis();
  if (now - lastInfoMs >= 3000UL)
  {
    lastInfoMs = now;
    Serial.printf("linkUp=%d hasLease=%d ", ip.linkUp(), ip.hasLease());
    printSelfInfo();
    if (ip.hasLease() && ip.netif())
    {
      esp_netif_ip_info_t ipInfo{};
      if (esp_netif_get_ip_info(ip.netif(), &ipInfo) == ESP_OK)
      {
        Serial.printf(" ip=" IPSTR " gw=" IPSTR, IP2STR(&ipInfo.ip), IP2STR(&ipInfo.gw));
      }
    }
    Serial.println();
  }

  delay(10);
}
