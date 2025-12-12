# EspNowBus

[English README](README.md)

ESP32 / Arduino 向けの軽量な ESP-NOW グループメッセージバス。小規模ネットワーク（目安 6 ノード）の利用を前提に、暗号化と認証をデフォルト有効化しつつ、Arduino らしい簡潔な API を提供します。

## 特徴
- シンプルな API: `begin()`, `sendTo()`, `broadcast()`, `onReceive()`, `onSendResult()`。
- デフォルトでセキュア: ESP-NOW 暗号化・JOIN 認証・Broadcast 認証を有効化。
- 自動ペア登録: JOIN 要求をブロードキャストし、受け入れ可能なノードが自動で peer 登録。
- 安定した送信制御: FreeRTOS タスクが送信キューを直列処理。
- ハートビートで生存監視: ユニキャスト Ping/Pong と再JOINでリンクを自動復旧。

## 基本コンセプト
- **groupName から鍵派生**: `groupName` → `groupSecret` → `groupId` / `keyAuth` / `keyBcast`。
- **パケット種別**: `DataUnicast`, `DataBroadcast`, `ControlJoinReq`, `ControlJoinAck`, `ControlHeartbeat`, `ControlAppAck`。
- **セキュリティ**: Broadcast には `groupId`・`seq`・`authTag` を付与し、JOIN はチャレンジレスポンスで認証。暗号化利用を推奨。JOIN/Ack/Heartbeat は peer 未登録でも届くよう平文送信だが HMAC で保護。

## クイックスタート
```cpp
#include <EspNowBus.h>

EspNowBus bus;

void setup() {
  Serial.begin(115200);

  EspNowBus::Config cfg;
  cfg.groupName = "my-group";
  cfg.useEncryption = true;
  cfg.maxQueueLength = 16;

  bus.onReceive([](const uint8_t* mac, const uint8_t* data, size_t len, bool wasRetry, bool isBroadcast) {
    Serial.printf("From %02X:%02X... len=%d retry=%d bcast=%d\n", mac[0], mac[1], (int)len, wasRetry, isBroadcast);
  });

  bus.onSendResult([](const uint8_t* mac, EspNowBus::SendStatus st) {
    Serial.printf("Send to %02X:%02X... status=%d\n", mac[0], mac[1], (int)st);
  });

  bus.begin(cfg);
  bus.sendJoinRequest();  // peer に参加を依頼（ブロードキャスト）
}

void loop() {
  const char msg[] = "hello";
  bus.broadcast(msg, sizeof(msg));
  delay(1000);
}
```

## Config 概要
- `groupName` (必須): グループ識別子。鍵・ID 生成の元になる。
- `useEncryption` (既定 true): ESP-NOW 暗号化。最大 peer 数は約 6。
- `enablePeerAuth` (既定 true): JOIN 時のチャレンジレスポンス。
- `channel` (既定 -1): Wi-Fi チャンネル。-1 は `groupName`/`groupId` をハッシュして 1〜13 を自動決定。明示指定は 1〜13 にクリップ。グループ全体で同じチャンネルに合わせること。
- `phyRate` (既定 `WIFI_PHY_RATE_11M_L`): ESP-NOW PHY 速度。ESP-IDF 5.1 以降は peer ごとの設定（ブロードキャスト用 peer も含む）。無効値は既定にフォールバック。目安:
  - `WIFI_PHY_RATE_1M_L` (802.11b): 遅いが遠距離でも安定。
  - `WIFI_PHY_RATE_11M_L` (802.11b): 中距離まで安定し、そこそこ高速。
  - `WIFI_PHY_RATE_24M` (802.11g): 近距離で高速かつ汎用性あり。
  - `WIFI_PHY_RATE_MCS4_LGI` (802.11n, 約39 Mbps): 無印 ESP32 で現実的な安定上限。
  - `WIFI_PHY_RATE_MCS7_LGI` (802.11n, 約65 Mbps): 最速だが ESP32-S3/C3 以外では不安定になりがち。
- `maxQueueLength` (既定 16): 送信キュー長。
- `maxPayloadBytes` (既定 1470): 送信ペイロード上限。ESP-IDF 5.4 以降は ~1470B、5.3 以前は実質 ~250B が上限。内部ヘッダ分を差し引く必要があり、実際に使えるのは Unicast で約 `maxPayloadBytes-6`、Broadcast で約 `maxPayloadBytes-6-4-16` バイト。
- `maxRetries` (既定 1): 初回送信後のリトライ回数。0 でリトライなし。
- `retryDelayMs` (既定 0): リトライ間隔。送信タイムアウト検知後は即再送がデフォルト（バックオフしたい場合のみ設定）。
- `txTimeoutMs` (既定 120): 送信中の応答待ちタイムアウト。経過で失敗扱い→リトライまたは諦め。
- `sendTimeoutMs` (既定 50): 送信キュー投入時のタイムアウト。`0`=非ブロック、`portMAX_DELAY`=無期限。
- `autoJoinIntervalMs` (既定 30000): JOIN 募集の自動送信間隔。0 で自動募集を無効化。
- `heartbeatIntervalMs` (既定 10000): ハートビート周期。1x 経過で Ping 送信、2x で対象限定JOIN、3x で切断。
- `taskCore` (既定 `ARDUINO_RUNNING_CORE`): 送信タスクをピン留めするコア。`-1` で無指定、`0/1` で指定。デフォルトは loop と同じコア。
- `taskPriority` (既定 3): 送信タスク優先度。loop(1) より高く、WiFi 内部タスク(4〜5) より低めを推奨。
- `taskStackSize` (既定 4096): 送信タスクのスタックサイズ（バイト）。
- `enableAppAck` (既定 true): ユニキャストにアプリ層 ACK を自動付与。成功は `AppAckReceived`、未達はリトライののち `AppAckTimeout` で通知。
- ISR 非対応: `sendTo`/`broadcast` は ISR から呼べない（ブロッキング API を使用するため）。
- `replayWindowBcast` (既定 32): Broadcast のリプレイ窓（0 で無効。送信元最大16件・32bit窓、超過時は最古の送信元を破棄）

### 明示的離脱（end）
- `end(stopWiFi=false, sendLeave=true)`: 送信キューを破棄し、`ControlLeave` をブロードキャストで 1 回送信（リトライなし、短時間だけ待って終了）。`stopWiFi=true` で Wi-Fi/ESP-NOW も停止、`sendLeave=false` で離脱通知を送らず静かに終了。
- `ControlLeave` 受信側は peer を即削除し、`onJoinEvent(mac, false, false)` を発火する（ハートビート 3x 超過のタイムアウト離脱と同じ通知）。

### 送信ごとのタイムアウト上書き
`sendTo` / `sendToAllPeers` / `broadcast` に任意の `timeoutMs` を指定可能。  
`0`=非ブロック、`portMAX_DELAY`=無期限、`kUseDefault`（`portMAX_DELAY - 1` を特別値に利用）= `Config.sendTimeoutMs` を使用。

### キューの挙動とメモリ目安
- ペイロードはキューにコピーされ、`len > maxPayloadBytes` は即失敗で返す。
- 送信キューは FreeRTOS の Queue にメタデータ（ポインタ+長さ+宛先種別など）を積み、実データ用の固定長バッファは `begin()` 時にまとめて確保。以降は `malloc` しない。確保失敗時は begin が失敗。
- メモリ目安: おおむね `maxPayloadBytes * maxQueueLength` にメタデータ分が加算（例: 1470B×16 ≒ 24KB）。
- 省メモリ/互換性重視なら `maxPayloadBytes` を 250 などに下げ、`maxQueueLength` も適宜調整。
- キューの状況確認: `sendQueueFree()` / `sendQueueSize()` で空きスロット数と投入済み件数を取得可能。
- ピア参照: `peerCount()` と `getPeer(index, macOut)` で登録済みピアを列挙できる。

## サンプルとユースケース
- [`examples/01_Broadcast`](examples/01_Broadcast): シンプルな定期ブロードキャスト（自動 JOIN 無効）。
- [`examples/02_JoinAndUnicast`](examples/02_JoinAndUnicast): JOIN 後、ランダムなピアへユニキャスト。再起動後のピア再発見（定期 JOIN）と到達確認の例。
- [`examples/03_SendToAllPeers`](examples/03_SendToAllPeers): `sendToAllPeers` で全ピアにユニキャスト同報。暗号化/HMAC/AppAck で到達確認を重視する用途に。
- [`examples/04_MasterSlave`](examples/04_MasterSlave): マスター（JOIN 受け入れ）とスレーブ（全ピアへセンサ風送信）のペアスケッチ。
- [`examples/05_SendStatusDemo`](examples/05_SendStatusDemo): `SendStatus` を switch で確認するデモ。リトライ/タイムアウトと AppAck の挙動を見る用途に。
- [`examples/06_NoAppAck`](examples/06_NoAppAck): AppAck 無効の例。`SentOk` は物理送信成功のみ（軽量運用向け）。
- [`examples/07_AutoPurge`](examples/07_AutoPurge): JOIN イベントコールバックとハートビートによる離脱/削除の挙動を確認。
- [`examples/08_ChannelOverride`](examples/08_ChannelOverride): Wi-Fi チャンネルを明示指定（0 指定時に 1〜13 にクリップされることを確認）。
- [`examples/09_PhyRateOverride`](examples/09_PhyRateOverride): PHY レートを `WIFI_PHY_RATE_1M_L` に変更し遠距離向けにする例（既定は 24M）。
- [`examples/10_LowFootprintBroadcast`](examples/10_LowFootprintBroadcast): 暗号化/AppAck/peerAuth 無効、ペイロード 250B、キュー縮小の最軽量ブロードキャスト。
- [`examples/11_FullConfigTemplate`](examples/11_FullConfigTemplate): Config 全項目を既定値で明示した雛形。
- [`examples/12_ExplicitLeave`](examples/12_ExplicitLeave): シリアルコマンドで `end(stopWiFi, sendLeave)`, Wi-Fi 停止/再開, `begin` 再参加, `ESP.restart()` を試す明示的離脱デモ。

### リトライ / JOIN / ハートビート / 重複扱い
- 送信タスクは単一の送信スロットとフラグを持ち、ESP-NOW 送信完了 CB でフラグを下ろして `onSendResult` を通知。
- フラグが立ったまま `txTimeoutMs` を超えたらタイムアウト扱い→同じ msgId/seq で `maxRetries` 回までリトライ（`retryDelayMs` 既定 0 で即再送）。
- リトライ時はリトライフラグを立て、受信側は peer ごとに `msgId/seq` を見て重複を破棄（必要ならコールバックにリトライ情報を渡す）。
- 送信完了 CB では共有状態を触らず、FreeRTOS のタスク通知（`xTaskNotifyFromISR`）で送信タスクに結果を渡し、送信タスク側でフラグを下ろして `onSendResult` を実行する。
- JOIN フロー: `sendJoinRequest(targetMac)` で ControlJoinReq をブロードキャスト（HMAC+targetMac）。受け入れ側は `groupId/targetMac/HMAC` を検証し、ControlJoinAck（nonceA echo + nonceB + targetMac, HMAC）をブロードキャストで返す。双方が Ack 受信後に peer 追加し、以後のユニキャストを暗号化する。
- Broadcast / Control パケットには groupId と HMAC(16B) を付与（keyBcast または keyAuth を使用）。Broadcast のリプレイは送信元最大16件・32bit窓で抑止し、超過時は最古の送信元を破棄する。
- ESP-NOW 暗号化を OFF にしても、Broadcast/Control/AppAck/Heartbeat は HMAC（keyBcast/keyAuth）で認証し、`enableAppAck` は ON のまま運用するのを推奨。
- ハートビート: ユニキャスト Ping/Pong（AppAck なし）。Pong 受信で生存判定。途絶時は 2x で対象限定 JOIN、3x で切断。
- 論理 ACK（`enableAppAck=true` が既定）: 受信側が msgId 付きで自動返信。物理 ACK だけでは到達保証せず、論理 ACK 未達は未達扱いでリトライ/再JOIN。物理 ACK 無しで論理 ACK が来た場合は成功扱いだが警告ログを残す。
- SendStatus の解釈: app-ACK 有効のユニキャストは `AppAckReceived` が成功、`AppAckTimeout` が失敗。`SentOk` は app-ACK 無効時の物理送信成功に限る。
- ControlAppAck: msgId をヘッダ id とペイロードに持ち、keyAuth HMAC を付けたユニキャストの論理 ACK（`enableAppAck` true の場合に自動送信）。重複受信でも AppAck を返して再送を止める。

### SendStatus 一覧
- `Queued`: キュー投入成功
- `SentOk`: 物理送信成功（app-ACK 無効時のみ）
- `SendFailed`: 物理送信失敗（ESP-NOW 失敗）
- `Timeout`: 物理送信タイムアウト
- `DroppedFull`: enqueue 時にキュー満杯でドロップ
- `DroppedOldest`: 予約（未使用）
- `TooLarge`: `maxPayloadBytes` 超過
- `Retrying`: リトライ中
- `AppAckReceived`: 論理ACK受信（app-ACK 有効時）
- `AppAckTimeout`: 論理ACK未達（リトライ枯渇、app-ACK 有効時）

SendStatus の扱い:
- 進捗 (`Queued`, `Retrying`) と最終結果（app-ACK 無効: `SentOk` / `SendFailed`/`Timeout`、app-ACK 有効: `AppAckReceived` / `AppAckTimeout`）の両方を送るため、1パケットにつき複数イベントが届くことがある。
- 通常はピアが正常なら自動リトライで成功し、細かく見なくてもよい。クリティカル要件では失敗ステータスを監視し、再JOINなどでリカバリする。

## コールバック
- `onReceive(const uint8_t* mac, const uint8_t* data, size_t len, bool wasRetry, bool isBroadcast)`: 認証済みユニキャストと正当なブロードキャストを受信時に呼ばれる。`wasRetry` が true の場合は送信側がリトライフラグを立てている。`isBroadcast` で経路の違いを判別できる。
- `onSendResult(const uint8_t* mac, SendStatus status)`: キュー投入ごとの送信結果を通知。AppAck 有効時の完了判定は `AppAckReceived` / `AppAckTimeout`（基本はこれを見る）。
- `onAppAck(const uint8_t* mac, uint16_t msgId)`: 受信した全ての AppAck で呼ばれる（in-flight でなくても）。デバッグやテレメトリ向けで任意。
- `onJoinEvent(const uint8_t mac[6], bool accepted, bool isAck)`: JOIN 受理/拒否/成功/離脱時。`accepted=true,isAck=false`=JoinReq 受理、`accepted=true,isAck=true`=JoinAck 受信成功、`accepted=false,isAck=true`=JoinAck 失敗、`accepted=false,isAck=false`=ハートビートタイムアウトまたは ControlLeave 受信による離脱。

## ドキュメント
- 仕様詳細: [`SPEC.ja.md`](SPEC.ja.md)
- 英語 README: [`README.md`](README.md)
- ユースケース例:
  - センサーノード → ゲートウェイの小規模ネットワーク
  - コントローラ → 複数ロボット/ガジェットへの操作
  - 小人数マルチプレイやイベント会場のローカル連携
  - グループ鍵で隔離されたアドホックなデバイスクラスタ

## ライセンス
MIT（[`LICENSE`](LICENSE) を参照）。
