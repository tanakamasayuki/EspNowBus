// en: Device-side connectivity diagnostics for EspNowIP.
// ja: EspNowIP の疎通診断を行う device 側サンプル。

#include <Arduino.h>
#include <EspNowIP.h>
#include <HTTPClient.h>
#include <Network.h>
#include <NetworkClient.h>
#include <NetworkUdp.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <lwip/ip_addr.h>
#include <ping/ping_sock.h>

namespace
{
constexpr const char *kGroupName = "espnow-ip-demo";
constexpr uint16_t kMtu = 1420;
constexpr uint32_t kRunIntervalMs = 30000;
constexpr uint32_t kPingTaskStackSize = 4096;
constexpr const char *kDnsHost = "example.com";
constexpr const char *kNtpHost = "pool.ntp.org";
constexpr uint16_t kNtpPort = 123;
constexpr const char *kHttpUrl = "http://example.com/";
} // namespace

struct PingResult
{
  volatile bool done = false;
  uint32_t replies = 0;
  uint32_t lastTimeMs = 0;
};

static EspNowIP ip;
static uint32_t lastInfoMs = 0;
static uint32_t lastRunMs = 0;
static bool ranOnce = false;

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
    Serial.printf("channel=%u second=%d ", primary, static_cast<int>(second));
  }
}

static void printIpInfo()
{
  if (!ip.netif())
    return;

  esp_netif_ip_info_t ipInfo{};
  esp_netif_dns_info_t dnsMain{};
  esp_netif_dns_info_t dnsBackup{};
  if (esp_netif_get_ip_info(ip.netif(), &ipInfo) == ESP_OK)
  {
    Serial.printf("ip=" IPSTR " gw=" IPSTR, IP2STR(&ipInfo.ip), IP2STR(&ipInfo.gw));
  }
  if (esp_netif_get_dns_info(ip.netif(), ESP_NETIF_DNS_MAIN, &dnsMain) == ESP_OK)
  {
    Serial.printf(" dns1=" IPSTR, IP2STR(&dnsMain.ip.u_addr.ip4));
  }
  if (esp_netif_get_dns_info(ip.netif(), ESP_NETIF_DNS_BACKUP, &dnsBackup) == ESP_OK &&
      dnsBackup.ip.u_addr.ip4.addr != 0)
  {
    Serial.printf(" dns2=" IPSTR, IP2STR(&dnsBackup.ip.u_addr.ip4));
  }
}

static void onPingSuccess(esp_ping_handle_t hdl, void *args)
{
  auto *result = static_cast<PingResult *>(args);
  uint32_t timeMs = 0;
  uint32_t size = sizeof(timeMs);
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &timeMs, size);
  result->replies++;
  result->lastTimeMs = timeMs;
}

static void onPingEnd(esp_ping_handle_t, void *args)
{
  auto *result = static_cast<PingResult *>(args);
  result->done = true;
}

static bool runPing(const char *label, const IPAddress &target)
{
  PingResult result{};
  esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
  config.count = 1;
  config.interval_ms = 100;
  config.timeout_ms = 1000;
  config.task_stack_size = kPingTaskStackSize;
  IP4_ADDR(&config.target_addr.u_addr.ip4, target[0], target[1], target[2], target[3]);
  IP_SET_TYPE_VAL(config.target_addr, IPADDR_TYPE_V4);

  esp_ping_callbacks_t callbacks{};
  callbacks.cb_args = &result;
  callbacks.on_ping_success = &onPingSuccess;
  callbacks.on_ping_end = &onPingEnd;

  esp_ping_handle_t handle = nullptr;
  if (esp_ping_new_session(&config, &callbacks, &handle) != ESP_OK)
  {
    Serial.printf("[FAIL] %s ping init failed\n", label);
    return false;
  }

  bool ok = false;
  if (esp_ping_start(handle) == ESP_OK)
  {
    uint32_t start = millis();
    while (!result.done && (millis() - start) < 3000UL)
    {
      delay(10);
    }
    ok = result.replies > 0;
  }

  esp_ping_stop(handle);
  esp_ping_delete_session(handle);

  if (ok)
  {
    Serial.printf("[ OK ] %s ping %s time=%lums\n", label, target.toString().c_str(), static_cast<unsigned long>(result.lastTimeMs));
  }
  else
  {
    Serial.printf("[FAIL] %s ping %s\n", label, target.toString().c_str());
  }
  return ok;
}

static bool runDnsTest(IPAddress &resolved)
{
  const IPAddress zero(static_cast<uint32_t>(0));
  int rc = Network.hostByName(kDnsHost, resolved);
  if (rc > 0 && resolved != INADDR_NONE && resolved != zero)
  {
    Serial.printf("[ OK ] DNS %s -> %s\n", kDnsHost, resolved.toString().c_str());
    return true;
  }
  Serial.printf("[FAIL] DNS %s rc=%d ip=%s\n", kDnsHost, rc, resolved.toString().c_str());
  return false;
}

static bool runNtpTest()
{
  IPAddress ntpIp;
  const IPAddress zero(static_cast<uint32_t>(0));
  int rc = Network.hostByName(kNtpHost, ntpIp);
  if (!(rc > 0 && ntpIp != INADDR_NONE && ntpIp != zero))
  {
    Serial.printf("[FAIL] NTP DNS %s rc=%d ip=%s\n", kNtpHost, rc, ntpIp.toString().c_str());
    return false;
  }

  NetworkUDP udp;
  if (!udp.begin(0))
  {
    Serial.println("[FAIL] NTP UDP begin");
    return false;
  }

  uint8_t packet[48]{};
  packet[0] = 0x1B;
  udp.beginPacket(ntpIp, kNtpPort);
  udp.write(packet, sizeof(packet));
  if (!udp.endPacket())
  {
    Serial.printf("[FAIL] NTP send %s\n", ntpIp.toString().c_str());
    udp.stop();
    return false;
  }

  uint32_t start = millis();
  while ((millis() - start) < 3000UL)
  {
    int size = udp.parsePacket();
    if (size >= 48)
    {
      uint8_t response[48]{};
      udp.read(response, sizeof(response));
      uint32_t seconds1900 = (static_cast<uint32_t>(response[40]) << 24) |
                             (static_cast<uint32_t>(response[41]) << 16) |
                             (static_cast<uint32_t>(response[42]) << 8) |
                             static_cast<uint32_t>(response[43]);
      uint32_t unixTime = seconds1900 > 2208988800UL ? seconds1900 - 2208988800UL : 0;
      Serial.printf("[ OK ] NTP %s (%s) unix=%lu\n",
                    kNtpHost,
                    ntpIp.toString().c_str(),
                    static_cast<unsigned long>(unixTime));
      udp.stop();
      return true;
    }
    delay(10);
  }

  Serial.printf("[FAIL] NTP %s (%s)\n", kNtpHost, ntpIp.toString().c_str());
  udp.stop();
  return false;
}

static bool runHttpTest()
{
  NetworkClient client;
  HTTPClient http;
  String preview;
  int bytesRead = 0;
  http.setConnectTimeout(3000);
  http.setTimeout(3000);
  if (!http.begin(client, String(kHttpUrl)))
  {
    Serial.printf("[FAIL] HTTP begin %s\n", kHttpUrl);
    return false;
  }

  int code = http.GET();
  if (code > 0)
  {
    NetworkClient *stream = http.getStreamPtr();
    if (stream)
    {
      uint32_t start = millis();
      while (http.connected() || stream->available() > 0)
      {
        while (stream->available() > 0)
        {
          const int c = stream->read();
          if (c < 0)
            break;
          ++bytesRead;
          if (preview.length() < 96)
            preview += static_cast<char>(c);
        }

        if ((millis() - start) > 5000UL)
          break;
        delay(1);
      }
    }

    preview.replace("\r", "\\r");
    preview.replace("\n", "\\n");
    Serial.printf("[ OK ] HTTP GET %s code=%d bytes=%d preview=\"%s\"\n",
                  kHttpUrl,
                  code,
                  bytesRead,
                  preview.c_str());
    http.end();
    return true;
  }

  Serial.printf("[FAIL] HTTP GET %s err=%d\n", kHttpUrl, code);
  http.end();
  return false;
}

static void runDiagnostics()
{
  Serial.println("== Connectivity Check ==");

  if (ip.netif())
  {
    esp_netif_ip_info_t ipInfo{};
    if (esp_netif_get_ip_info(ip.netif(), &ipInfo) == ESP_OK)
    {
      IPAddress gateway(ipInfo.gw.addr);
      if (!runPing("gateway", gateway))
      {
        Serial.println("== Connectivity Check Failed: gateway ping ==");
        return;
      }
    }
    else
    {
      Serial.println("[FAIL] gateway ping ip_info unavailable");
      Serial.println("== Connectivity Check Failed: ip_info unavailable ==");
      return;
    }
  }
  else
  {
    Serial.println("[FAIL] netif unavailable");
    Serial.println("== Connectivity Check Failed: netif unavailable ==");
    return;
  }

  IPAddress resolved;
  if (!runDnsTest(resolved))
  {
    Serial.println("== Connectivity Check Failed: DNS ==");
    return;
  }

  if (!runPing("internet", resolved))
  {
    Serial.println("== Connectivity Check Failed: internet ping ==");
    return;
  }

  if (!runNtpTest())
  {
    Serial.println("== Connectivity Check Failed: NTP ==");
    return;
  }

  if (!runHttpTest())
  {
    Serial.println("== Connectivity Check Failed: HTTP ==");
    return;
  }

  Serial.println("== Connectivity Check Complete ==");
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("05_DeviceConnectivityCheck");
  Serial.printf("groupName=%s mtu=%u\n", kGroupName, kMtu);
  printSelfInfo();
  Serial.println();

  EspNowIP::Config cfg;
  cfg.groupName = kGroupName;
  cfg.mtu = kMtu;
  // cfg.channel = 6;                  // Optional: fix the channel on all nodes when needed.
  // cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional: lower PHY rate example for longer range.

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
    printIpInfo();
    Serial.println();
  }

  if (ip.linkUp() && ip.hasLease())
  {
    if (!ranOnce || (now - lastRunMs) >= kRunIntervalMs)
    {
      ranOnce = true;
      lastRunMs = now;
      runDiagnostics();
    }
  }

  delay(10);
}
