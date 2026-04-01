// en: Planned gateway-side EspNowIP example using physical UART PPP uplink.
// ja: 物理 UART PPP uplink を使う gateway 側 EspNowIP サンプル予定。

#include <Arduino.h>
#include <EspNowIP.h>
#include <EspNowPPPoS.h>

EspNowIPGateway gateway;
EspNowPPPoS pppos;
static bool gatewayStarted = false;
static uint32_t lastInfoMs = 0;

namespace
{
  // The user is responsible for serial initialization.
  // Replace these with any already-initialized streams, for example:
  //   Serial1.begin(115200, SERIAL_8N1, 16, 17);
  //   pppIo = &Serial1;
  //   debugOut = &Serial;
  // or:
  //   USBSerial.begin(115200);
  //   pppIo = &USBSerial;
  //   debugOut = nullptr;
  Stream *pppIo = nullptr;
  Print *debugOut = &Serial;
  constexpr bool kEnablePppLogs = false;

  void logLine(const char *msg)
  {
    if (debugOut && msg)
      debugOut->println(msg);
  }

  void logf(const char *fmt, ...)
  {
    if (!debugOut || !fmt)
      return;

    char buffer[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    debugOut->print(buffer);
  }
} // namespace

void setup()
{
  delay(200);

  if (pppIo && debugOut == pppIo)
    debugOut = nullptr;

  logLine("04_GatewayPPPSerial");
  logLine("Physical UART PPP EspNowIP gateway scaffold.");
  logLine("Target topology: ESP32 UART <-> host PC PPP peer.");

  if (!pppIo)
  {
    logLine("PPP I/O stream is not configured.");
    logLine("User responsibility:");
    logLine("1. Initialize a serial/USB stream");
    logLine("2. Assign it to pppIo");
    logLine("3. Optionally assign debugOut to another Print");
    logLine("4. Re-run this sketch");
  }
  else
  {
    EspNowPPPoS::Config pppCfg;
    pppCfg.logger = kEnablePppLogs ? debugOut : nullptr;
    if (!pppos.begin(*pppIo, pppCfg))
      logLine("pppos.begin failed");
    else
      logLine("pppos.begin -> 1");
  }
}

void loop()
{
  pppos.poll();

  if (pppos.connected() && !gatewayStarted && pppos.netif())
  {
    EspNowIPGateway::Config cfg;
    cfg.groupName = "espnow-ip-demo";
    cfg.mtu = 1420;
    // cfg.channel = 6;                  // Optional
    // cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional
    cfg.uplink = pppos.netif();
    gatewayStarted = gateway.begin(cfg);
    logf("gateway.begin -> %d\n", gatewayStarted);
  }

  uint32_t now = millis();
  if (pppos.connected() && pppos.netif() && (now - lastInfoMs) >= 5000UL)
  {
    lastInfoMs = now;
    esp_netif_ip_info_t info{};
    esp_netif_dns_info_t dns{};
    if (esp_netif_get_ip_info(pppos.netif(), &info) == ESP_OK)
    {
      logf("PPP ip=" IPSTR " gw=" IPSTR " mask=" IPSTR,
           IP2STR(&info.ip), IP2STR(&info.gw), IP2STR(&info.netmask));
      if (esp_netif_get_dns_info(pppos.netif(), ESP_NETIF_DNS_MAIN, &dns) == ESP_OK)
      {
        logf(" dns=" IPSTR, IP2STR(&dns.ip.u_addr.ip4));
      }
      logLine("");
    }
  }

  if (gatewayStarted)
    gateway.poll();
  delay(10);
}
