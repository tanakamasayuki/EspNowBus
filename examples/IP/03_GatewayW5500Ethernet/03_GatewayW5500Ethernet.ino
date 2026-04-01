// en: Planned gateway-side EspNowIP example using W5500 Ethernet uplink.
// ja: W5500 Ethernet uplink を使う gateway 側 EspNowIP サンプル予定。

#include <Arduino.h>
// #include <ETH.h>
// #include <EspNowIP.h>

// TODO: replace with the actual Ethernet / EspNowIPGateway API once implemented.
// EspNowIPGateway gateway;

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("03_GatewayW5500Ethernet");
  Serial.println("This is a scaffold for the planned EspNowIP W5500 Ethernet gateway example.");

  // TODO:
  // 1. Initialize the W5500 Ethernet uplink.
  // 2. Obtain / bind the Ethernet esp_netif as the uplink.
  // 3. Start EspNowIPGateway with NAT on top of that uplink.
  //
  // EspNowIPGateway::Config cfg;
  // cfg.groupName = "espnow-ip-demo";
  // cfg.mtu = 1420;
  // cfg.channel = 6;                  // Optional
  // cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional
  // cfg.uplink = ...; // Ethernet esp_netif
  // gateway.begin(cfg);
}

void loop()
{
  // TODO:
  // gateway.poll();
  delay(1000);
}

