#include <EspNowSerial.h>
#include <WiFi.h>

// en: Controller-side USB bridge example with session list and session switching.
// ja: Session 一覧表示と切り替えを行う controller 側 USB bridge サンプル。

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
  Serial.println("Commands:");
  Serial.println("  list              - show sessions");
  Serial.println("  use <index>       - bind controlSerial to session index");
  Serial.println("  send <text>       - send text to current session");
  Serial.println("  help              - show this help");
}

void listSessions()
{
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

void handleLine(const String &line)
{
  if (line == "list")
  {
    listSessions();
    return;
  }
  if (line == "help")
  {
    printHelp();
    return;
  }
  if (line.startsWith("use "))
  {
    bindSessionIndex(line.substring(4).toInt());
    return;
  }
  if (line.startsWith("send "))
  {
    if (currentSession < 0 || !controlSerial.connected())
    {
      Serial.println("no active session");
      return;
    }
    String payload = line.substring(5);
    controlSerial.printf("[controller] %s\n", payload.c_str());
    return;
  }
  Serial.println("unknown command");
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
  printHelp();
}

void loop()
{
  serialHub.poll();

  static String commandLine;
  while (Serial.available() > 0)
  {
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
    Serial.write(controlSerial.read());
  }
}
