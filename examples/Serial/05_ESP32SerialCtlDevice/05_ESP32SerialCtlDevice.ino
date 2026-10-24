// en: Expose a serial command device through the bus
// ja: シリアル Command DeviceをBus越しに使う
#include <ESP32SerialCtl.h>
#include <EspNowSerial.h>
#include <WiFi.h>

// en: Device-side example that exposes ESP32SerialCtl over Serial over EspNow.
// ja: ESP32SerialCtl を シリアル over EspNow 越しに公開する device 側サンプル。

EspNowSerial serialHub;
EspNowSerialPort controlSerial;
esp32serialctl::ESP32SerialCtl<> remoteCli(controlSerial);

void setup()
{
  Serial.begin(115200);
  delay(500);

  // en: Shared group name used by the controller bridge and this CLI device.
  // ja: controller bridge とこの CLI device で共有するグループ名。
  EspNowSerial::Config cfg;
  cfg.groupName = "espnow-serial-demo";

  // en: The device side stays silent and only connects to an advertising controller.
  // ja: device 側は自分から募集せず、controller 側の募集にだけ接続する。
  cfg.advertise = false;

  // en: Initialize this component after its prerequisites; inspect the returned status where provided.
  // ja: 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
  if (!serialHub.begin(cfg))
  {
    Serial.println("serialHub.begin failed");
    return;
  }

  controlSerial.attach(serialHub);
  // en: Bind to the first controller-side session so the remote CLI becomes reachable.
  // ja: 最初の controller 側 session に bind して、リモート CLI を使えるようにする。
  controlSerial.bindFirstAvailable();

  uint8_t selfMac[6] = {};
  WiFi.macAddress(selfMac);
  Serial.printf("ESP32SerialCtl device ready: %02X:%02X:%02X:%02X:%02X:%02X\n",
                selfMac[0], selfMac[1], selfMac[2],
                selfMac[3], selfMac[4], selfMac[5]);
  Serial.println("Use examples/Serial/02_ControllerBridge on the controller side.");
}

void loop()
{
  // en: Keep the hub progressing regularly, then let ESP32SerialCtl service the selected session.
  // ja: hub を定期的に進めたうえで、選択済み session 上で ESP32SerialCtl を処理する。
  serialHub.poll();
  // en: Process pending commands regularly; blocking the caller postpones new input.
  // ja: 保留コマンドを定期的に処理します。呼び出し元を止めると次の入力も遅れます。
  remoteCli.service();
}
