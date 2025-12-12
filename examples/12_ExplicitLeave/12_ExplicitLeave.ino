#include <EspNowBus.h>

// en: Demo for explicit leave/end & restart via serial commands.
// ja: シリアルコマンドで明示的離脱（end）、再参加（begin）、再起動を試すデモ。

EspNowBus bus;
bool running = false;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
  Serial.printf("RX %s from %02X:%02X:%02X:%02X:%02X:%02X len=%u retry=%d\n",
                isBroadcast ? "bcast" : "uni",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                static_cast<unsigned>(len), wasRetry);
}

void onSendResult(const uint8_t *mac, EspNowBus::SendStatus status)
{
  const char *name = "";
  switch (status)
  {
  case EspNowBus::Queued:
    name = "Queued";
    break;
  case EspNowBus::SentOk:
    name = "SentOk";
    break;
  case EspNowBus::SendFailed:
    name = "SendFailed";
    break;
  case EspNowBus::Timeout:
    name = "Timeout";
    break;
  case EspNowBus::DroppedFull:
    name = "DroppedFull";
    break;
  case EspNowBus::DroppedOldest:
    name = "DroppedOldest";
    break;
  case EspNowBus::TooLarge:
    name = "TooLarge";
    break;
  case EspNowBus::Retrying:
    name = "Retrying";
    break;
  case EspNowBus::AppAckTimeout:
    name = "AppAckTimeout";
    break;
  case EspNowBus::AppAckReceived:
    name = "AppAckReceived";
    break;
  }
  Serial.printf("TX to %02X:%02X:%02X:%02X:%02X:%02X status=%s\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], name);
}

void onJoinEvent(const uint8_t mac[6], bool accepted, bool isAck)
{
  // en: accepted=false, isAck=false: leave detected by timeout or receiving ControlLeave
  // ja: accepted=false, isAck=false: timeout or ControlLeave 受信で離脱検知
  Serial.printf("JoinEvent mac=%02X:%02X:%02X:%02X:%02X:%02X accepted=%d isAck=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], accepted, isAck);
}

void printHelp()
{
  Serial.println("Commands:");
  Serial.println("  e: end(stopWiFi=false, sendLeave=true)");
  Serial.println("  p: end(stopWiFi=true, sendLeave=true)");
  Serial.println("  b: begin (re-join)");
  Serial.println("  s: send broadcast 'hi'");
  Serial.println("  r: ESP.restart()");
  Serial.println("  h: help");
}

void startBus()
{
  if (running)
  {
    Serial.println("bus already running");
    return;
  }
  EspNowBus::Config cfg;
  cfg.groupName = "espnow-demo_" __FILE__; // en: required groupName string / ja: 必須のグループ名文字列
  bus.onReceive(onReceive);
  bus.onSendResult(onSendResult);
  bus.onJoinEvent(onJoinEvent);
  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
    return;
  }
  running = true;
  Serial.println("bus started");
}

void stopBus(bool stopWiFi)
{
  if (!running && !stopWiFi)
  {
    Serial.println("bus already stopped");
  }
  // en: sendLeave=true to send leave notification
  // ja: sendLeave=true で離脱通知を送る
  bus.end(stopWiFi, true);
  running = false;
  Serial.printf("bus ended (stopWiFi=%d)\n", stopWiFi);
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  printHelp();
  startBus();
}

void loop()
{
  while (Serial.available())
  {
    char c = Serial.read();
    if (c >= 'A' && c <= 'Z')
    {
      c = static_cast<char>(c - 'A' + 'a'); // en: enforce lowercase for commands / ja: コマンドは小文字に変換
    }
    switch (c)
    {
    case 'e':
      stopBus(false);
      break;
    case 'p':
      stopBus(true);
      break;
    case 'b':
      startBus();
      break;
    case 's':
    {
      const char msg[] = "hi";
      bus.broadcast(msg, sizeof(msg));
      break;
    }
    case 'r':
      Serial.println("Restarting...");
      ESP.restart();
      break;
    case 'h':
      printHelp();
      break;
    default:
      // en: ignore others
      // ja: その他は無視
      break;
    }
  }
}
