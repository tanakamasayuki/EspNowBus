// en: Planned gateway-side EspNowIP example using Wi-Fi STA uplink.
// ja: Wi-Fi STA uplink を使う gateway 側 EspNowIP サンプル予定。

#include <Arduino.h>
#include <WiFi.h>
// #include <EspNowIP.h>

// TODO: replace with the actual EspNowIPGateway API once implemented.
// EspNowIPGateway gateway;

static const char *kUplinkSsid = "YOUR-UPLINK-SSID";
static const char *kUplinkPass = "YOUR-UPLINK-PASS";
static uint32_t lastInfoMs = 0;

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("02_GatewayWiFiSTA");
  Serial.println("This is a scaffold for the planned EspNowIP Wi-Fi gateway example.");

  WiFi.mode(WIFI_STA);
  WiFi.begin(kUplinkSsid, kUplinkPass);

  // TODO:
  // EspNowIPGateway::Config cfg;
  // cfg.groupName = "espnow-ip-demo";
  // cfg.uplink = ...; // Wi-Fi STA esp_netif
  // cfg.mtu = 1420;
  // cfg.channel = 6;                  // Optional: fix ESP-NOW to the same channel as the STA uplink.
  // cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional: lower PHY rate example for longer range.
  // gateway.begin(cfg);
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    uint32_t now = millis();
    if (now - lastInfoMs >= 5000UL)
    {
      lastInfoMs = now;
      Serial.printf("Wi-Fi STA connected, channel=%d, ip=%s\n",
                    WiFi.channel(),
                    WiFi.localIP().toString().c_str());
      Serial.println("Set the device-side EspNowBus channel to the same STA channel when testing this example.");
    }
  }

  // TODO:
  // gateway.poll();
  delay(1000);
}
