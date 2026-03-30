#include <EspNowSerial.h>
#include <WiFi.h>

// en: Controller-side transparent USB bridge with local escape commands.
// ja: ローカル制御コマンド付きの透過 USB bridge を行う controller 側サンプル。

EspNowSerial serialHub;
EspNowSerialPort controlSerial;
int currentSession = -1;

void printMac(const uint8_t mac[6])
{
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void printHelp()
{
  Serial.println("Transparent bridge mode:");
  Serial.println("  Any normal line is forwarded to the selected session.");
  Serial.println("  Session 0 is selected automatically when it becomes available.");
  Serial.println("Local commands:");
  Serial.println("  ///list           - show sessions");
  Serial.println("  ///use <index>    - bind controlSerial to session index");
  Serial.println("  ///help           - show this help");
}

void listSessions()
{
  // en: Print every active session so the controller can choose one explicitly.
  // ja: controller 側で選べるよう、使用中 session を一覧表示する。
  Serial.println("Sessions:");
  for (size_t i = 0; i < serialHub.sessionCapacity(); ++i)
  {
    if (!serialHub.sessionInUse(i))
    {
      continue;
    }

    uint8_t mac[6] = {};
    bool hasMac = serialHub.sessionMac(i, mac);
    int avail = serialHub.sessionAvailable(i);

    Serial.printf("  [%u] connected=%d available=%d mac=",
                  static_cast<unsigned>(i),
                  serialHub.sessionConnected(i),
                  avail);
    if (hasMac)
    {
      printMac(mac);
    }
    else
    {
      Serial.print("(none)");
    }
    if (currentSession == static_cast<int>(i))
    {
      Serial.print("  <selected>");
    }
    Serial.println();
  }
}

void bindSessionIndex(int index)
{
  if (index < 0)
  {
    Serial.println("invalid index");
    return;
  }
  if (!serialHub.sessionInUse(static_cast<size_t>(index)))
  {
    Serial.println("session not in use");
    return;
  }
  if (!controlSerial.bindSession(static_cast<size_t>(index)))
  {
    Serial.println("bindSession failed");
    return;
  }
  currentSession = index;
  Serial.printf("selected session %d\n", currentSession);
}

void tryAutoBindDefaultSession()
{
  // en: Auto-select session 0 for the common two-device controller/device setup.
  // ja: 2 台構成を想定し、session 0 を自動選択する。
  if (currentSession >= 0 || controlSerial.connected())
  {
    return;
  }
  if (!serialHub.sessionInUse(0))
  {
    return;
  }
  if (!controlSerial.bindSession(0))
  {
    return;
  }

  currentSession = 0;
  Serial.println("auto-selected session 0");
}

void handleLine(const String &line)
{
  if (line == "///list")
  {
    listSessions();
    return;
  }
  if (line == "///help")
  {
    printHelp();
    return;
  }
  if (line.startsWith("///use "))
  {
    bindSessionIndex(line.substring(7).toInt());
    return;
  }

  // en: Lines that do not begin with the local escape prefix are forwarded as-is.
  // ja: ローカル escape prefix で始まらない行は、そのまま選択中 session へ転送する。
  if (currentSession < 0 || !controlSerial.connected())
  {
    Serial.println("no active session");
    return;
  }
  controlSerial.write(reinterpret_cast<const uint8_t *>(line.c_str()), line.length());
  controlSerial.write('\n');
}

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

  controlSerial.attach(serialHub);
  // en: Session 0 is selected automatically when it becomes available.
  // ja: session 0 が使えるようになったら自動選択する。
  printHelp();
}

void loop()
{
  serialHub.poll();
  tryAutoBindDefaultSession();

  static String commandLine;
  while (Serial.available() > 0)
  {
    // en: Read one line from USB Serial and either handle it locally or forward it.
    // ja: USB Serial から 1 行読み、ローカル処理または転送を行う。
    char c = static_cast<char>(Serial.read());
    if (c == '\r')
    {
      continue;
    }
    if (c == '\n')
    {
      handleLine(commandLine);
      commandLine = "";
      continue;
    }
    commandLine += c;
  }

  // en: Forward RX from the currently selected session to USB Serial.
  // ja: 現在選択中の session からの受信を USB Serial へ流す。
  while (controlSerial.available() > 0)
  {
    // en: Forward device-side output to the PC side USB serial.
    // ja: device 側の出力を PC 側の USB シリアルへ転送する。
    Serial.write(controlSerial.read());
  }
}
