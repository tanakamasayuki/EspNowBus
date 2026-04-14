// en: Gateway-side EspNowIP example using Wi-Fi STA uplink.
// ja: Wi-Fi STA uplink を使う gateway 側 EspNowIP サンプル。

#include <Arduino.h>
#include <WiFi.h>
#include <EspNowIP.h>
#include <esp_netif.h>
#include <time.h>

EspNowIPGateway gateway;

#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
#else
#define WIFI_SSID "YourSSID"     // Enter your Wi-Fi SSID here / Wi-FiのSSIDを入力
#define WIFI_PASS "YourPassword" // Enter your Wi-Fi password here / Wi-Fiのパスワードを入力
#endif

static uint32_t lastInfoMs = 0;
static bool gatewayStarted = false;
static bool timeSyncStarted = false;
static bool timeSyncOk = false;

static void startTimeSync()
{
  if (timeSyncStarted)
    return;

  configTime(0, 0, "pool.ntp.org", "time.google.com", "ntp.nict.jp");
  timeSyncStarted = true;
  Serial.println("Started NTP sync for uplink check.");
}

static bool isTimeSynced()
{
  time_t now = time(nullptr);
  return now >= 1700000000;
}

static void printUplinkInfo()
{
  esp_netif_t *uplink = WiFi.STA.netif();
  esp_netif_ip_info_t ipInfo{};
  esp_netif_dns_info_t dns{};

  Serial.printf("Wi-Fi STA connected, channel=%ld, ip=%s",
                WiFi.channel(),
                WiFi.localIP().toString().c_str());

  if (uplink && esp_netif_get_ip_info(uplink, &ipInfo) == ESP_OK)
  {
    Serial.printf(" gw=" IPSTR " netmask=" IPSTR,
                  IP2STR(&ipInfo.gw),
                  IP2STR(&ipInfo.netmask));
  }

  if (uplink && esp_netif_get_dns_info(uplink, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK)
  {
    Serial.printf(" dns=" IPSTR, IP2STR(&dns.ip.u_addr.ip4));
  }

  if (isTimeSynced())
  {
    char buf[32]{};
    time_t now = time(nullptr);
    struct tm tmNow{};
    gmtime_r(&now, &tmNow);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tmNow);
    Serial.printf(" ntp=ok now=%s", buf);
    timeSyncOk = true;
  }
  else
  {
    Serial.print(" ntp=waiting");
  }

  Serial.println();
  Serial.println("Set the device-side EspNowBus channel to the same STA channel when testing this example.");
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("03_GatewayWiFiSTA");
  Serial.println("EspNowIP Wi-Fi STA gateway example.");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.println("Waiting for Wi-Fi STA uplink before starting gateway...");
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    startTimeSync();

    if (!gatewayStarted)
    {
      EspNowIPGateway::Config cfg;
      cfg.groupName = "espnow-ip-demo";
      cfg.uplink = WiFi.STA.netif();
      cfg.mtu = 1420;
      // cfg.channel = 6;                  // Optional: fix ESP-NOW to the same channel as the STA uplink.
      // cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional: lower PHY rate example for longer range.
      gatewayStarted = gateway.begin(cfg);
      Serial.printf("gateway.begin -> %d\n", gatewayStarted);
    }

    uint32_t now = millis();
    if (now - lastInfoMs >= 5000UL)
    {
      lastInfoMs = now;
      printUplinkInfo();
    }

    if (!timeSyncOk && isTimeSynced())
    {
      timeSyncOk = true;
      Serial.println("NTP sync completed. Uplink internet access looks available.");
    }
  }

  if (gatewayStarted)
    gateway.poll();

  delay(10);
}
