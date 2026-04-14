// en: Gateway-side EspNowIP example using wired Ethernet uplink.
// ja: 有線 Ethernet uplink を使う gateway 側 EspNowIP サンプル。

#include <Arduino.h>
#include <EspNowIP.h>
#include <ETH.h>

EspNowIPGateway gateway;
static bool gatewayStarted = false;
static bool ethConnected = false;
static uint32_t lastInfoMs = 0;

// Reference board: LilyGo T-Internet-COM (LAN8720 RMII)
namespace
{
  constexpr int kEthAddr = 0;
  constexpr int kEthPowerPin = 4;
  constexpr int kEthMdcPin = 23;
  constexpr int kEthMdioPin = 18;
  constexpr eth_phy_type_t kEthType = ETH_PHY_LAN8720;
  constexpr eth_clock_mode_t kEthClockMode = ETH_CLOCK_GPIO0_OUT;
} // namespace

static void onNetworkEvent(arduino_event_id_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_ETH_START:
    Serial.println("ETH Started");
    ETH.setHostname("espnow-ip-eth-gateway");
    break;
  case ARDUINO_EVENT_ETH_CONNECTED:
    Serial.println("ETH Connected");
    break;
  case ARDUINO_EVENT_ETH_GOT_IP:
    Serial.printf("ETH MAC: %s, IPv4: %s, %s, %uMbps\n",
                  ETH.macAddress().c_str(),
                  ETH.localIP().toString().c_str(),
                  ETH.fullDuplex() ? "FULL_DUPLEX" : "HALF_DUPLEX",
                  ETH.linkSpeed());
    ethConnected = true;
    break;
  case ARDUINO_EVENT_ETH_LOST_IP:
    Serial.println("ETH Lost IP");
    ethConnected = false;
    break;
  case ARDUINO_EVENT_ETH_DISCONNECTED:
    Serial.println("ETH Disconnected");
    ethConnected = false;
    break;
  case ARDUINO_EVENT_ETH_STOP:
    Serial.println("ETH Stopped");
    ethConnected = false;
    break;
  default:
    Serial.printf("ETH Event: %ld\n", static_cast<long>(event));
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("04_GatewayEthernet");
  Serial.println("Wired Ethernet EspNowIP gateway example.");
  Serial.println("Current reference target: LilyGo T-Internet-COM (LAN8720 via ETH.h).");

  Network.onEvent(onNetworkEvent);
  ETH.begin(kEthType, kEthAddr, kEthMdcPin, kEthMdioPin, kEthPowerPin, kEthClockMode);
}

void loop()
{
  if (ethConnected && !gatewayStarted)
  {
    EspNowIPGateway::Config cfg;
    cfg.groupName = "espnow-ip-demo";
    cfg.mtu = 1420;
    // cfg.channel = 6;                  // Optional
    // cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional
    cfg.uplink = ETH.netif();
    gatewayStarted = gateway.begin(cfg);
    Serial.printf("gateway.begin -> %d\n", gatewayStarted);
  }

  uint32_t now = millis();
  if (ethConnected && (now - lastInfoMs) >= 5000UL)
  {
    lastInfoMs = now;
    Serial.printf("ETH ip=%s gw=%s dns=%s\n",
                  ETH.localIP().toString().c_str(),
                  ETH.gatewayIP().toString().c_str(),
                  ETH.dnsIP().toString().c_str());
  }

  if (gatewayStarted)
    gateway.poll();
  delay(10);
}
