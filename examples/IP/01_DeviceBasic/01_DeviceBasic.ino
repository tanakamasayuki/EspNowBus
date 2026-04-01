// en: Planned minimal device-side EspNowIP example.
// ja: device 側 EspNowIP の最小構成サンプル予定。

#include <Arduino.h>
// #include <EspNowIP.h>

// TODO: replace with the actual EspNowIP API once implemented.
// EspNowIP ip;

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("01_DeviceBasic");
  Serial.println("This is a scaffold for the planned EspNowIP device example.");

  // TODO:
  // EspNowIP::Config cfg;
  // cfg.groupName = "espnow-ip-demo";
  // cfg.mtu = 1420;
  // cfg.channel = 6;                  // Optional: fix the channel on all nodes when needed.
  // cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional: lower PHY rate example for longer range.
  // ip.begin(cfg);
}

void loop()
{
  // TODO:
  // ip.poll();
  // Print link / lease state periodically.
  delay(1000);
}
