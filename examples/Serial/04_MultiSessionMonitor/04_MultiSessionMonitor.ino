#include <EspNowSerial.h>

// en: Controller-side example that scans all sessions and forwards RX to USB Serial.
// ja: 全 session を走査し、受信データを USB Serial へ転送する controller 側サンプル。

EspNowSerial serialHub;
EspNowSerialPort monitorPort;

void setup()
{
  Serial.begin(115200);
  delay(500);

  // en: Shared group name for all monitored serial sessions.
  // ja: 監視対象の serial session で共有するグループ名。
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
  // en: Keep the session table and TX/RX state updated.
  // ja: session 一覧と TX/RX 状態を更新する。
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

    // en: Prefix each output line with session index and peer MAC when known.
    // ja: どの session の出力か分かるよう、index と MAC を先頭に付ける。
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
      // en: Stream the selected session data to the local USB serial monitor.
      // ja: 選択中 session のデータをローカル USB シリアルへ流す。
      Serial.write(monitorPort.read());
    }
  }
}
