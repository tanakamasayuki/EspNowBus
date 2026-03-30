#include <EspNowSerial.h>

// en: Controller-side example that scans all sessions and forwards RX to USB Serial.
// ja: 全 session を走査し、受信データを USB Serial へ転送する controller 側サンプル。

EspNowSerial serialHub;
EspNowSerialPort monitorPort;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowSerial::Config cfg;
  cfg.groupName = "espnow-serial-demo";

  if (!serialHub.begin(cfg))
  {
    Serial.println("serialHub.begin failed");
    return;
  }

  monitorPort.attach(serialHub);
  Serial.println("MultiSessionMonitor ready");
}

void loop()
{
  serialHub.poll();

  for (size_t i = 0; i < serialHub.sessionCapacity(); ++i)
  {
    if (!serialHub.sessionInUse(i))
    {
      continue;
    }
    if (!serialHub.sessionConnected(i))
    {
      continue;
    }
    if (serialHub.sessionAvailable(i) <= 0)
    {
      continue;
    }
    if (!monitorPort.bindSession(i))
    {
      continue;
    }

    uint8_t mac[6] = {};
    bool hasMac = serialHub.sessionMac(i, mac);
    if (hasMac)
    {
      Serial.printf("[%u %02X:%02X:%02X:%02X:%02X:%02X] ",
                    static_cast<unsigned>(i),
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    else
    {
      Serial.printf("[%u] ", static_cast<unsigned>(i));
    }

    while (monitorPort.available() > 0)
    {
      Serial.write(monitorPort.read());
    }
  }
}
