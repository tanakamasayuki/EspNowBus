#include <EspNowSerial.h>
#include <WiFi.h>

// en: Minimal two-node Serial over EspNow example.
// ja: Serial over EspNow の最小 2 ノード構成サンプル。

EspNowSerial serialHub;
EspNowSerialPort controlSerial;

void setup()
{
  Serial.begin(115200);
  delay(500);

  // en: Shared group name for the Serial over EspNow link.
  // ja: Serial over EspNow で共有するグループ名。
  EspNowSerial::Config cfg;
  cfg.groupName = "espnow-serial-demo";

  if (!serialHub.begin(cfg))
  {
    Serial.println("serialHub.begin failed");
    return;
  }

  controlSerial.attach(serialHub);
  // en: Bind to the first session that becomes available.
  // ja: 最初に使える session へ bind する。
  controlSerial.bindFirstAvailable();

  Serial.println("EspNowSerial basic pair ready");
}

void loop()
{
  // en: Recommended to call poll() regularly from loop().
  // ja: poll() は loop() から定期的に呼ぶのを推奨。
  //
  // en: Port access may internally trigger a lightweight poll as a fallback,
  //     but relying on that shifts processing timing and makes behavior less predictable.
  // ja: Port への access 時にも内部で軽い poll が走る想定だが、
  //     それに頼ると処理タイミングがずれて挙動が読みにくくなるため非推奨。
  serialHub.poll();

  static uint32_t lastSend = 0;
  if (millis() - lastSend > 2000)
  {
    lastSend = millis();

    // en: Include sender information in the payload so received output is easier to identify.
    // ja: 受信表示で判別しやすいよう、送信元情報を payload に含める。
    //
    // en: In a 2-node setup this is mostly for visibility, but in a full-mesh setup with 3 or more nodes
    //     bindFirstAvailable() may end up bound to a peer you did not expect.
    // ja: 2 台構成では見やすさのための情報だが、3 台以上のフルメッシュ構成では
    //     bindFirstAvailable() が想定外の peer を選ぶ可能性がある。
    uint8_t selfMac[6] = {};
    WiFi.macAddress(selfMac);
    controlSerial.printf("hello from %02X:%02X:%02X:%02X:%02X:%02X\n",
                         selfMac[0], selfMac[1], selfMac[2],
                         selfMac[3], selfMac[4], selfMac[5]);
  }

  while (controlSerial.available() > 0)
  {
    // en: Forward received bytes to the local USB serial monitor.
    // ja: 受信した byte をローカルの USB シリアルへ流す。
    Serial.write(controlSerial.read());
  }
}
