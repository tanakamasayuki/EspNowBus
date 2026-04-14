// en: Gateway-side EspNowIP example using an AT modem PPP uplink.
// ja: AT モデム PPP uplink を使う gateway 側 EspNowIP サンプル。

#include <Arduino.h>
#include <EspNowIP.h>
#include <PPP.h>

EspNowIPGateway gateway;
static bool gatewayStarted = false;
static uint32_t lastInfoMs = 0;

#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
#else
#define PPP_MODEM_APN "internet"
#define PPP_MODEM_PIN ""
#define PPP_MODEM_RST 25
#define PPP_MODEM_RST_LOW false
#define PPP_MODEM_RST_DELAY 200
#define PPP_MODEM_TX 21
#define PPP_MODEM_RX 22
#define PPP_MODEM_RTS 26
#define PPP_MODEM_CTS 27
#define PPP_MODEM_FC ESP_MODEM_FLOW_CONTROL_HW
#define PPP_MODEM_MODEL PPP_MODEM_SIM7600
#endif

static void onEvent(arduino_event_id_t event, arduino_event_info_t)
{
  switch (event)
  {
  case ARDUINO_EVENT_PPP_START:
    Serial.println("PPP Started");
    break;
  case ARDUINO_EVENT_PPP_CONNECTED:
    Serial.println("PPP Connected");
    break;
  case ARDUINO_EVENT_PPP_GOT_IP:
    Serial.printf("PPP Got IP: %s gw=%s dns=%s\n",
                  PPP.localIP().toString().c_str(),
                  PPP.gatewayIP().toString().c_str(),
                  PPP.dnsIP().toString().c_str());
    break;
  case ARDUINO_EVENT_PPP_LOST_IP:
    Serial.println("PPP Lost IP");
    break;
  case ARDUINO_EVENT_PPP_DISCONNECTED:
    Serial.println("PPP Disconnected");
    break;
  case ARDUINO_EVENT_PPP_STOP:
    Serial.println("PPP Stopped");
    break;
  default:
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("06_GatewayPPPModem");
  Serial.println("AT modem PPP EspNowIP gateway example.");

  Network.onEvent(onEvent);

  PPP.setApn(PPP_MODEM_APN);
  PPP.setPin(PPP_MODEM_PIN);
  PPP.setResetPin(PPP_MODEM_RST, PPP_MODEM_RST_LOW, PPP_MODEM_RST_DELAY);
  PPP.setPins(PPP_MODEM_TX, PPP_MODEM_RX, PPP_MODEM_RTS, PPP_MODEM_CTS, PPP_MODEM_FC);

  Serial.println("Starting modem PPP...");
  PPP.begin(PPP_MODEM_MODEL);

  Serial.print("Model: ");
  Serial.println(PPP.moduleName());
  Serial.print("IMEI: ");
  Serial.println(PPP.IMEI());
}

void loop()
{
  if (PPP.connected() && !gatewayStarted)
  {
    EspNowIPGateway::Config cfg;
    cfg.groupName = "espnow-ip-demo";
    cfg.mtu = 1420;
    cfg.uplink = PPP.netif();
    gatewayStarted = gateway.begin(cfg);
    Serial.printf("gateway.begin -> %d\n", gatewayStarted);
  }

  uint32_t now = millis();
  if (PPP.connected() && (now - lastInfoMs) >= 5000UL)
  {
    lastInfoMs = now;
    Serial.printf("PPP ip=%s gw=%s dns=%s operator=%s rssi=%d\n",
                  PPP.localIP().toString().c_str(),
                  PPP.gatewayIP().toString().c_str(),
                  PPP.dnsIP().toString().c_str(),
                  PPP.operatorName().c_str(),
                  PPP.RSSI());
  }

  if (gatewayStarted)
    gateway.poll();

  delay(10);
}
