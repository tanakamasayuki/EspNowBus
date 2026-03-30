#include <EspNowSerial.h>
#include <WiFi.h>

// en: Device-side example that stays silent and only connects to an advertising controller.
// ja: controller 側の募集にだけ接続し、自分では募集しない device 側サンプル。

EspNowSerial serialHub;
EspNowSerialPort controlSerial;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowSerial::Config cfg;
  cfg.groupName = "espnow-serial-demo";

  // en: Planned policy example: device side does not advertise sessions on its own.
  // ja: 想定ポリシー例: device 側は自分から募集しない。
  cfg.advertise = false;

  if (!serialHub.begin(cfg))
  {
    Serial.println("serialHub.begin failed");
    return;
  }

  controlSerial.attach(serialHub);
  controlSerial.bindFirstAvailable();

  uint8_t selfMac[6] = {};
  WiFi.macAddress(selfMac);
  Serial.printf("Silent device ready: %02X:%02X:%02X:%02X:%02X:%02X\n",
                selfMac[0], selfMac[1], selfMac[2],
                selfMac[3], selfMac[4], selfMac[5]);
}

void loop()
{
  // en: Regular poll() calls are still recommended even if port access can trigger fallback processing.
  // ja: Port access 側の補助処理に頼らず、ここでも定期 poll() を推奨。
  serialHub.poll();

  while (controlSerial.available() > 0)
  {
    int c = controlSerial.read();
    if (c < 0)
    {
      break;
    }

    // en: Echo received bytes both to USB Serial and back to the controller session.
    // ja: 受信 byte を USB Serial に出しつつ controller session にもそのまま返す。
    Serial.write(static_cast<uint8_t>(c));
    controlSerial.write(static_cast<uint8_t>(c));
  }
}
