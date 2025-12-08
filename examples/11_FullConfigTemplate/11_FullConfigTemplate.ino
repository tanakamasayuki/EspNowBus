#include <EspNowBus.h>

// en: Template sketch with every Config field set to its default value, based on JoinAndUnicast.
// ja: すべての Config をデフォルト値で明示した雛形（JoinAndUnicast ベース）。

EspNowBus bus;

void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
  // en: Print sender and payload (AppAck enabled by default)
  // ja: 送信元とペイロードを表示（AppAck は既定で有効）
  Serial.printf("RX from %02X:%02X:%02X:%02X:%02X:%02X data='%s' len=%u retry=%d bcast=%d\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                (const char *)data, static_cast<unsigned>(len), wasRetry, isBroadcast);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspNowBus::Config cfg;
  cfg.groupName = "espnow-demo_" __FILE__; // en: required groupName string / ja: 必須のグループ名文字列

  // en: Security / ACK settings (all defaults)
  // ja: セキュリティと ACK の設定（すべて既定値）
  cfg.useEncryption = true;      // en: enable ESP-NOW encryption / ja: ESP-NOW 暗号化を有効
  cfg.enablePeerAuth = true;     // en: authenticate peers on join / ja: JOIN 時に peer を認証
  cfg.enableAppAck = true;       // en: app-level ACK on unicast / ja: ユニキャストで AppAck を利用

  // en: Radio settings
  // ja: 無線関連の設定
  cfg.channel = -1;                  // en: -1 auto → 1-13 from group hash / ja: -1 自動（ハッシュで 1〜13 を決定）
  cfg.phyRate = WIFI_PHY_RATE_11M_L; // en: 11M long-range default / ja: 11M(L) が既定

  // en: Queue / payload / timeouts
  // ja: キュー / ペイロード / タイムアウト設定
  cfg.maxQueueLength = 16;                              // en: TX queue depth / ja: 送信キュー長
  cfg.maxPayloadBytes = EspNowBus::kMaxPayloadDefault;  // en: max payload bytes (1470) / ja: 最大ペイロード 1470 バイト
  cfg.sendTimeoutMs = 50;                               // en: enqueue wait before fail / ja: キュー投入待ちタイムアウト
  cfg.maxRetries = 1;                                   // en: resend count for AppAck / ja: AppAck 用の再送回数
  cfg.retryDelayMs = 0;                                 // en: delay between retries / ja: 再送間隔
  cfg.txTimeoutMs = 120;                                // en: physical TX timeout / ja: 物理送信タイムアウト

  // en: JOIN / heartbeat
  // ja: JOIN とハートビート
  cfg.autoJoinIntervalMs = 30000;  // en: periodic JOIN interval ms / ja: 定期 JOIN 間隔 ms
  cfg.heartbeatIntervalMs = 10000; // en: heartbeat ping interval / ja: ハートビート ping 間隔

  // en: Task config
  // ja: タスク設定
  cfg.taskCore = ARDUINO_RUNNING_CORE; // en: -1 unpinned; 0/1 pin core / ja: -1 非固定、0/1 でコア固定
  cfg.taskPriority = 3;                // en: worker task priority / ja: ワーカタスク優先度
  cfg.taskStackSize = 4096;            // en: worker stack size bytes / ja: ワーカスタックサイズ（バイト）

  // en: Broadcast replay window
  // ja: ブロードキャストのリプレイウィンドウ
  cfg.replayWindowBcast = 32;          // en: anti-replay window per sender / ja: 送信者ごとのリプレイ対策幅

  bus.onReceive(onReceive);

  if (!bus.begin(cfg))
  {
    Serial.println("begin failed");
  }
  else
  {
    Serial.println("EspNowBus started with explicit defaults (details: see README)");
  }
}

void loop()
{
  static uint32_t lastSend = 0;

  if (millis() - lastSend > 3000)
  {
    lastSend = millis();
    size_t peers = bus.peerCount();
    if (peers == 0)
    {
      Serial.println("no peers yet (broadcast JOIN running)");
      return;
    }

    // en: Choose a random registered peer and send a unicast message.
    // ja: 登録済みピアからランダムに選んでユニキャスト送信。
    size_t idx = random(peers);
    uint8_t target[6];
    if (!bus.getPeer(idx, target))
    {
      Serial.println("getPeer failed");
      return;
    }
    const char msg[] = "hello peer";
    bus.sendTo(target, msg, sizeof(msg));
  }
}
