# EspNowBus

[English README](README.md)

ESP32 / Arduino 向けの軽量な ESP-NOW グループメッセージバス。小規模ネットワーク（目安 6 ノード）の利用を前提に、暗号化と認証をデフォルト有効化しつつ、Arduino らしい簡潔な API を提供します。

## 特徴
- シンプルな API: `begin()`, `sendTo()`, `broadcast()`, `onReceive()`, `onSendResult()`。
- デフォルトでセキュア: ESP-NOW 暗号化・JOIN 認証・Broadcast 認証を有効化。
- 自動ペア登録: JOIN 要求をブロードキャストし、受け入れ可能なノードが自動で peer 登録。
- 安定した送信制御: FreeRTOS タスクが送信キューを直列処理。

## 基本コンセプト
- **groupName から鍵派生**: `groupName` → `groupSecret` → `groupId` / `keyAuth` / `keyBcast`。
- **ロール**: Master / Flat は登録受け入れ可、Slave は不可（`canAcceptRegistrations` で管理）。
- **パケット種別**: `DataUnicast`, `DataBroadcast`, `PeerAuthHello`, `PeerAuthResponse`, `ControlJoinReq`, `ControlJoinAck`。
- **セキュリティ**: Broadcast には `groupId`・`seq`・`authTag` を付与し、JOIN はチャレンジレスポンスで認証。暗号化利用を推奨。

## クイックスタート
```cpp
#include <EspNowBus.h>

EspNowBus bus;

void setup() {
  Serial.begin(115200);

  EspNowBus::Config cfg;
  cfg.groupName = "my-group";
  cfg.canAcceptRegistrations = true;  // Master / Flat
  cfg.useEncryption = true;
  cfg.maxQueueLength = 16;

  bus.onReceive([](const uint8_t* mac, const uint8_t* data, size_t len) {
    Serial.printf("From %02X:%02X... len=%d\n", mac[0], mac[1], (int)len);
  });

  bus.onSendResult([](const uint8_t* mac, bool ok) {
    Serial.printf("Send to %02X:%02X... %s\n", mac[0], mac[1], ok ? "OK" : "FAIL");
  });

  bus.begin(cfg);
  bus.sendRegistrationRequest();  // peer に参加を依頼
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
- `enableBroadcastAuth` (既定 true): Broadcast の HMAC 認証とリプレイ防止。
- `maxQueueLength` (既定 16): 送信キュー長。
- `maxPayloadBytes` (既定 1470): 送信ペイロード上限。ESP-IDF 5.4 以降は ~1470B、5.3 以前は実質 ~250B が上限。内部ヘッダ分を差し引く必要があり、実際に使えるのは Unicast で約 `maxPayloadBytes-6`、Broadcast で約 `maxPayloadBytes-6-4-16` バイト。
- `maxRetries` (既定 1): 初回送信後のリトライ回数。0 でリトライなし。
- `retryDelayMs` (既定 0): リトライ間隔。送信タイムアウト検知後は即再送がデフォルト（バックオフしたい場合のみ設定）。
- `txTimeoutMs` (既定 120): 送信中の応答待ちタイムアウト。経過で失敗扱い→リトライまたは諦め。
- `canAcceptRegistrations` (既定 true): このノードが peer を受け入れるか。
- `sendTimeoutMs` (既定 50): 送信キュー投入時のタイムアウト。`0`=非ブロック、`portMAX_DELAY`=無期限。
- `taskCore` (既定 `ARDUINO_RUNNING_CORE`): 送信タスクをピン留めするコア。`-1` で無指定、`0/1` で指定。デフォルトは loop と同じコア。
- `taskPriority` (既定 3): 送信タスク優先度。loop(1) より高く、WiFi 内部タスク(4〜5) より低めを推奨。
- `taskStackSize` (既定 4096): 送信タスクのスタックサイズ（バイト）。
- `enableAppAck` (既定 true): ユニキャストにアプリ層 ACK を自動付与。成功は `AppAckReceived`、未達はリトライののち `AppAckTimeout` で通知。
- ISR 非対応: `sendTo`/`broadcast` は ISR から呼べない（ブロッキング API を使用するため）。
- `replayWindowBcast` (既定 64): Broadcast のリプレイ窓（0 で無効）。
- `replayWindowJoin` (既定 64): JOIN のリプレイ窓（最近の JOIN seq を何件覚えて重複を落とすか。0 で無効）。同時多発の JOIN やリトライ時の重複処理を避ける用途。内部は 64bit 窓なので 64 を超える値は 64 に丸められます。
- `maxAckFailures` / `failureWindowMs` / `rejoinAfterPurge`: 連続 `AppAckTimeout` / `SendFailed` が閾値を超えたピアを自動でパージ（0 で無効）。`rejoinAfterPurge=true` ならパージ後に再JOIN要求を送る。

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
- `examples/BroadcastAndAck`: 定期ブロードキャスト＋論理ACK。全体への通知で到達確認を取りたい場合に。
- `examples/JoinAndUnicast`: JOIN 後、ランダムなピアへユニキャスト。再起動後のピア再発見（定期JOIN）と到達確認の例。
- `examples/SendToAllPeers`: `sendToAllPeers` で全ピアにユニキャスト同報。ブロードキャストより重いが、暗号化＋HMAC＋AppAck による到達確認を重視する用途に。
- `examples/MasterSlave/Master` / `.../Slave`: マスタは登録を受け付け、スレーブ（センサ）は受け付けない構成。スレーブが定期JOINでマスタを探し、`sendToAllPeers` でデータを送る（マルチマスタも可）。
- `examples/AutoPurge`: 連続 `AppAckTimeout`/`SendFailed` で自動パージし、JOIN/パージのコールバックを表示。リンク不安定な環境での自動復旧の例。
- `examples/SendStatusDemo`: `SendStatus` を switch で確認するデモ。app-ACK 有効時は相手が落ちていない限り自動再送で隠れることもある。
- `examples/NoAppAck`: AppAck 無効の例。`onAppAck` を設定しても呼ばれず、`SentOk` は物理送信成功のみ（軽量運用向け）。

### リトライと重複扱い
- 送信タスクは単一の送信スロットとフラグを持ち、ESP-NOW 送信完了 CB でフラグを下ろして `onSendResult` を通知。
- フラグが立ったまま `txTimeoutMs` を超えたらタイムアウト扱い→同じ msgId/seq で `maxRetries` 回までリトライ（`retryDelayMs` 既定 0 で即再送）。
- リトライ時はリトライフラグを立て、受信側は peer ごとに `msgId/seq` を見て重複を破棄（必要ならコールバックにリトライ情報を渡す）。
- 送信完了 CB では共有状態を触らず、FreeRTOS のタスク通知（`xTaskNotifyFromISR`）で送信タスクに結果を渡し、送信タスク側でフラグを下ろして `onSendResult` を実行する。
- 簡易 JOIN フロー: `sendRegistrationRequest()` で ControlJoinReq をブロードキャストし、受け入れ可能ノードが peer 登録して ControlJoinAck をユニキャスト返信（認証・暗号化は未実装）。
- Broadcast / Control パケットには groupId と HMAC(16B) を付与（keyBcast または keyAuth を使用）。受信側は検証し、誤りを破棄。Broadcast は peer ごとに 64 エントリのスライド窓でリプレイ防止。
- ESP-NOW 暗号化を OFF にしても、Broadcast/Control/AppAck は HMAC（keyBcast/keyAuth）で認証し、`enableAppAck` は ON のまま運用するのを推奨。
- JOIN パケットには 8 バイトの nonce を含め、Ack でエコー。完全なチャレンジレスポンスは今後の課題。
- JOIN も別窓でリプレイ制限し、Ack には responder 側の nonceB も返却（現状は保管のみ、今後の検証に利用予定）。
- 論理 ACK（`enableAppAck=true` が既定）: 受信側が msgId 付きで自動返信。物理 ACK だけでは到達保証せず、論理 ACK 未達は未達扱いでリトライ/再JOIN。物理 ACK 無しで論理 ACK が来た場合は成功扱いだが警告ログを残す。
- 再起動などで prevToken がずれても、新規 JOIN として扱い直して復旧するポリシーにしている。
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

## コールバック
- `onReceive(cb)`: 認証済みユニキャストと正当なブロードキャストを受信時に呼ばれる。
- `onSendResult(cb)`: キュー投入ごとの送信結果を通知。AppAck 有効時の完了判定は `AppAckReceived` / `AppAckTimeout`（基本はこれを見る）。
- `onAppAck(cb)`: 受信した全ての AppAck で呼ばれる（in-flight でなくても）。デバッグやテレメトリ向けで任意。
- `onJoinEvent(mac, accepted, isAck)`: JOIN 受理/拒否/成功時
- `onPeerPurged(mac)`: 自動パージ通知

## ドキュメント
- 仕様詳細: `SPEC.ja.md`
- 英語 README: `README.md`
- ユースケース例:
  - センサーノード → ゲートウェイの小規模ネットワーク
  - コントローラ → 複数ロボット/ガジェットへの操作
  - 小人数マルチプレイやイベント会場のローカル連携
  - グループ鍵で隔離されたアドホックなデバイスクラスタ

## ライセンス
MIT (`LICENSE` を参照)。
