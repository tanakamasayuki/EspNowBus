// en: Planned gateway-side EspNowIP example using physical UART PPP uplink.
// ja: 物理 UART PPP uplink を使う gateway 側 EspNowIP サンプル予定。

#include <Arduino.h>
// #include <EspNowIP.h>

// TODO: replace with the actual PPP / EspNowIPGateway API once implemented.
// EspNowIPGateway gateway;

static HardwareSerial &pppSerial = Serial1;
static constexpr int kPppRxPin = 16;
static constexpr int kPppTxPin = 17;
static constexpr uint32_t kPppBaud = 115200;

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("04_GatewayPPPSerial");
  Serial.println("This is a scaffold for the planned EspNowIP PPP-over-UART gateway example.");

  pppSerial.begin(kPppBaud, SERIAL_8N1, kPppRxPin, kPppTxPin);

  // TODO:
  // 1. Bring up PPP over the physical UART toward the host PC.
  // 2. Obtain / bind the PPP esp_netif as the uplink.
  // 3. Start EspNowIPGateway with NAT on top of that uplink.
}

void loop()
{
  // TODO:
  // gateway.poll();
  delay(1000);
}

