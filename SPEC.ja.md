# EspNowBus 仕様書

## 1. 概要
**EspNowBus** は、ESP32 / ESP-NOW を Arduino から扱いやすくするための  
軽量な「グループ指向メッセージバス」ライブラリです。

- ESP-NOW を **混信せず**・**気軽に** 使えるようにする  
- 小規模ネットワーク（〜6ノード）向けに、**暗号化＋認証をデフォルト有効化**  
- `sendTo()`, `broadcast()`, `onReceive()` などの簡潔な API  
- FreeRTOS タスクによる **自律的な送信制御**

---

## 2. デザイン目標
- `groupName` を 1 つ指定するだけで、内部で鍵派生・署名・暗号化が行われ、グループ外との通信を論理的に隔離する
- 他の ESP-NOW ネットワークと **混信しない**
- デフォルトで
  - ESP-NOW 暗号化  
  - チャレンジレスポンス認証  
  - ブロードキャスト認証  
  をすべて有効化（「そこそこ安全」を実現）
- Arduino らしいシンプルな API
- FreeRTOS による安定した送信キュー管理
- 自動ペア登録による「ノード起動 → 自動参加」を簡単に実現

---

## 3. アーキテクチャ概要
- **1ホップのみ**（中継・ルーティングなし）
- `groupName` をもとに以下を内部生成：
- `groupSecret`（グループ共通の秘密値）
- `groupId`（公開グループID）
- `keyAuth`（ペア登録認証用キー）
- `keyBcast`（ブロードキャスト認証用キー）
- 送信はキューに積み、**1件ずつ送信（直列処理）**
- 自動ペア登録：
  - 全ノードがブロードキャストで登録要求を出せる
  - 「受け入れ可能」なノードは自動的に peer 登録

### グループ指向通信の基本思想
- ESP-NOW の **ブロードキャスト** と **ユニキャスト** を使い分ける  
  - ブロードキャスト: 暗号化せず、`groupName` 由来の署名だけを付ける。誰にでも届くが改ざん・なりすましは防止できる。複数端末への一斉通知やペア探索に使う。  
  - ユニキャスト: デフォルトで暗号化。指定先だけに届くので機密データにも利用可。送信確認はペイロードも含めた「論理送達確認」で行う。
- グループ外からのパケットは署名検証で即遮断することで「ゆるやかな閉域」を実現し、同じ `groupName` を設定した端末だけで簡単にネットワークを構成できる。

---

## 4. ノードロールと自動ペア登録

### 4.1 ロール
EspNowBus は「ペア登録を受け入れるかどうか」を簡潔に管理するため  
以下 3 種類のロール概念を持つ：

| ロール | 説明 |
|-------|------|
| **Master** | 中心ノード（ゲートウェイなど）。登録要求を受け入れ可能。 |
| **Flat** | 対等ノード（ゲームのプレイヤーなど）。登録要求を受け入れ可能。 |
| **Slave** | **受け入れ不可**。登録依頼だけ投げられる。 |

※ ロールは実装上は `canAcceptRegistrations` フラグで管理される。

### 4.2 自動ペア登録の概要
- **全ノードが** `ControlJoinReq`（登録要求）をブロードキャスト可能
- Master / Flat（かつ受け入れON）は：
  1. JOIN要求を受信
  2. groupId・authTag を検証
  3. チャレンジレスポンスで認証
  4. 合格すれば自動で `addPeer()`  
- Slave ノードは受け入れ不可

---

## 5. セキュリティモデル

### 5.1 groupName からの派生
```
groupName → groupSecret
groupSecret → groupId / keyAuth / keyBcast
```

### 5.2 ESP-NOW 暗号化
- **デフォルト：ON**  
  → 最大 6 peer  
- OFF にすると最大 20 peer まで扱えるが、内容は平文

### 5.3 チャレンジレスポンス（JOIN時）
- JOIN / 再JOIN の際に実施
- `keyAuth` を使った相互認証
- 暗号化 OFF の場合でも最小限の認証を担保

### 5.4 ブロードキャスト認証
- Broadcast パケットには必ず
  - `groupId`
  - `seq`
  - `authTag = HMAC(keyBcast, ...)`
- 他グループや偽造パケットを防ぐ
- ペイロード自体は暗号化しないためグループ外の端末にも届く。改ざん・なりすましは防げるが、機密データの送信には使用しない

### 5.5 ユニキャストの論理送達確認
- 物理 ACK だけでは「届いたが復号に失敗」でも成功扱いになるため、ユニキャストでは `enableAppAck=true` を前提に **ペイロード内容を含めた HMAC を検証してから完了** とする
- app-ACK 無効時は ESP-NOW の物理 ACK でのみ完了判定するが、到達保証は下がる

---

## 6. パケット構造（論理仕様）

### 6.1 BaseHeader（全パケット共通）
- `magic`（1）: EspNowBus パケット識別
- `version`（1）
- `type`（1）: PacketType
- `flags`（1）: ビットフラグ  
  - bit0: `isRetry`（同一 `msgId`/`seq` の再送時に 1）  
  - bit1〜7: 予約
- `id`（2）: Unicast は msgId、Broadcast/JOIN は seq

### 6.2 PacketType 一覧
- `DataUnicast`  
- `DataBroadcast`  
- `PeerAuthHello` / `PeerAuthResponse`  
- `ControlJoinReq` / `ControlJoinAck`  
- `ControlAppAck`（論理 ACK 用）

### 6.3 種別別の振る舞い
#### DataUnicast
- `[BaseHeader][msgId][UserPayload]`
- groupId は含まない
- 既存 peer からの通信のみ受理
- `msgId` は送信元ごとに単調増加（uint16、オーバーフローで wrap）。リトライ時は同じ `msgId` を使い、`flags.isRetry=1`
- 受信側は peer ごとに「最後に処理した msgId」を保持し、同一 msgId は重複として破棄（※仕様により後述）

#### DataBroadcast
- `[BaseHeader][groupId][seq][authTag][UserPayload]`
- groupId・authTag が正しい場合のみ onReceive へ渡す
- `seq`（uint16 など固定幅）は送信元ごとに単調増加。リトライ時は同じ `seq` を使い、`flags.isRetry=1`

#### ControlJoinReq / Ack / AppAck（固定長）
- 共通: `groupId(4, LE)` + `authTag(16)` を付与し、HMAC は `keyAuth` を使用  
- ControlJoinReq:
  - `BaseHeader`（id に seq）
  - `groupId`
  - `nonceA[8]`
  - `prevToken[8]`（前回 responder の nonceB。無ければ 0 埋め）
  - `authTag = HMAC(keyAuth, header..prevToken)`
- ControlJoinAck:
  - `BaseHeader`（id に seq）
  - `groupId`
  - `nonceA[8]`（エコー）
  - `nonceB[8]`（新規生成）
  - `authTag = HMAC(keyAuth, header..nonceB)`
- ControlAppAck（ユニキャストの論理 ACK）:
  - `BaseHeader`（id に msgId）
  - `groupId`
  - `msgId`（2byte, LE）
  - `authTag = HMAC(keyAuth, header..msgId)`

#### ControlAppAck
- ユニキャストの論理 ACK
- `[BaseHeader(id=msgId)][groupId][authTag][AppAckPayload(msgId)]`
- HMAC は keyAuth を使用
- ユニキャストのみで使用し、`enableAppAck=true` の場合に自動送信

---

## 7. API 仕様（高レベル）

### 7.1 Config 構造体

```cpp
struct Config {
    const char* groupName;               // 必須

    bool useEncryption        = true;    // ESP-NOW 暗号化
    bool enablePeerAuth       = true;    // チャレンジレスポンス ON
    bool enableBroadcastAuth  = true;    // Broadcast 認証

    // 無線設定
    int8_t channel = -1;                 // -1 で groupName 由来のハッシュ値から自動決定 (1〜13 を使用)、範囲外はクリップ
    wifi_phy_rate_t phyRate = WIFI_PHY_RATE_11M_L; // 送信速度。既定は 11M。必要に応じて高速化

    uint16_t maxQueueLength   = 16;      // 送信キュー長
    uint16_t maxPayloadBytes  = 1470;    // 送信ペイロード上限（ESP-NOW v2.0 想定）。互換性重視なら 250 に下げる
    uint32_t sendTimeoutMs    = 50;      // キュー投入時の既定タイムアウト。0=非ブロック, portMAX_DELAY=無期限
    uint8_t  maxRetries       = 1;       // 送信リトライ回数（初回送信を除く）。0 でリトライなし
    uint16_t retryDelayMs     = 0;       // リトライ間隔。送信タイムアウト検知後に即再送が既定なので 0ms（バックオフしたい場合のみ設定）
    uint32_t txTimeoutMs      = 120;     // 送信中の応答待ちタイムアウト。経過で失敗扱い→リトライまたは諦め

    // このノードが登録要求を「受け入れる権限を持つか」
    bool canAcceptRegistrations = true;  // Slave にしたい場合は false

    // 送信タスク（送信キュー処理）の RTOS 設定
    int8_t taskCore = ARDUINO_RUNNING_CORE; // -1 でピン留めなし、0/1 で指定。既定は loop と同じコア。
    UBaseType_t taskPriority = 3;           // 1〜5 目安。loop(1) より高く、WiFi タスク(4〜5) より低めが推奨。
    uint16_t taskStackSize = 4096;          // 送信タスクのスタックサイズ（バイト）

    // アプリ層 ACK（論理 ACK）を自動付与するか
    bool enableAppAck = true;               // 既定 ON。OFF にすると物理 ACK のみで送達確認はアプリ任せ

    // リプレイ窓サイズ（可変設定）
    uint16_t replayWindowBcast = 64;        // Broadcast 用
    uint16_t replayWindowJoin  = 64;        // JOIN 用（内部窓は64まで）

    // 連続失敗時の自動パージ設定（デフォルト無効）
    uint8_t  maxAckFailures    = 0;         // 0 で無効。連続 AppAckTimeout/SendFailed がこの回数を超えたらパージ
    uint32_t failureWindowMs   = 30000;     // 連続失敗をカウントする時間窓
    bool     rejoinAfterPurge  = false;     // パージ後に sendRegistrationRequest を自動送信するか
};
```

### 7.3 ログ出力
- ESP-IDF のログマクロ（`ESP_LOGE/W/I/D/V`）を利用する。デフォルトタグは `"EspNowBus"`。
- 主な出力例（目安）:
  - `ESP_LOGE`: `begin` 失敗（esp_now_init/メモリ確保/タスク生成）、peer 追加失敗、HMAC 検証失敗（原因付き）、リトライ枯渇による送信失敗
  - `ESP_LOGW`: 送信タイムアウト、キュー満杯でドロップ、JOIN リプレイ検出、未知の PacketType 受信
  - `ESP_LOGI`: `begin` 成功、JOIN 成功（Ack 確認）、peer 追加/削除
  - `ESP_LOGD/V`: デバッグ用途（seq/msgId/リトライ回数など詳細トレース）

推奨値の目安:
- フル MTU を使いたい場合は `maxPayloadBytes = 1470`（デフォルト）。  
- 互換性・メモリ重視では `maxPayloadBytes = 250`（kMaxPayloadLegacy）に下げ、`maxQueueLength` もメモリに合わせて調整。

### 7.2 EspNowBus クラス

```cpp
class EspNowBus {
public:
    bool begin(const Config& cfg);

    bool begin(const char* groupName,
               bool canAcceptRegistrations = true,
               bool useEncryption = true,
               uint16_t maxQueueLength = 16);

    void end();

    // timeoutMs: 0=非ブロック, portMAX_DELAY=無期限, kUseDefault=Config.sendTimeoutMs
    bool sendTo(const uint8_t mac[6], const void* data, size_t len, uint32_t timeoutMs = kUseDefault);
    bool sendToAllPeers(const void* data, size_t len, uint32_t timeoutMs = kUseDefault);
    bool broadcast(const void* data, size_t len, uint32_t timeoutMs = kUseDefault);

    void onReceive(ReceiveCallback cb);
    void onSendResult(SendResultCallback cb);

    bool addPeer(const uint8_t mac[6]);
    bool removePeer(const uint8_t mac[6]);
    bool hasPeer(const uint8_t mac[6]) const;

    void setAcceptRegistration(bool enable);

    bool sendRegistrationRequest();

    // イベントコールバック設定
    void onJoinEvent(JoinEventCb cb);   // JOIN 受理/拒否/成功時
    void onPeerPurged(PurgeEventCb cb); // 自動パージ時
};

// timeout の特別値
static constexpr uint32_t kUseDefault = portMAX_DELAY - 1; // Config.sendTimeoutMs を使うための値。portMAX_DELAY は「無期限」
static constexpr uint16_t kMaxPayloadDefault = 1470; // ESP-NOW v2.0 の MTU 目安
static constexpr uint16_t kMaxPayloadLegacy  = 250;  // 互換性重視サイズ
```

無線設定:
- `channel`: -1 の場合は `groupId` を 1〜13 にマッピングして自動決定。明示指定は 1〜13 にクリップして使用。
- `phyRate`: `wifi_phy_rate_t` の値を渡す（例: `WIFI_PHY_RATE_11M_L` 既定, 高速化したい場合は 2M/11M/24M などに変更）。環境が対応しない値を渡した場合は既定値にフォールバックする想定。ESP-IDF 5.1 以降は peer ごとの設定（ユニキャスト/ブロードキャスト用 peer の両方）として適用する。

---

## 8. 動作仕様

### 8.1 送信
- sendTo / broadcast / sendToAllPeers → 送信キューに追加
- 内部 FreeRTOS タスクが 1件ずつ処理
- 送信完了は onSendResult で通知
- 完了判定の基本方針  
  - **ユニキャスト + enableAppAck=true**: 受信側で HMAC 検証済みの AppAck を受信して初めて完了（ペイロードが壊れていても物理 ACK は返るため）  
  - **ユニキャスト + enableAppAck=false**: ESP-NOW の物理 ACK で完了  
  - **ブロードキャスト**: ACK が取れないため、ペイロード長と送信速度に応じた待ち時間を置いて完了扱いとする（その間は次の送信をキューで待機）
- `len > Config.maxPayloadBytes` の場合は即座に enqueue 失敗を返す
- `maxPayloadBytes` は IDF の `ESP_NOW_MAX_DATA_LEN(_V2)` を上限・ヘッダ分を下限にクリップする。実際にユーザーデータに使えるバイト数は Unicast でおおよそ `maxPayloadBytes - 6`、Broadcast/Control で `maxPayloadBytes - 6 - 4 - 16` と少なくなる点に注意。
- 送信キュー用メモリは `begin()` で一括確保し、以後 malloc しない  
  - ペイロードは固定長バッファ（`maxPayloadBytes` 分）にコピーして保持  
  - キュー管理は FreeRTOS Queue（`xQueueCreate` 系）を使用。エントリは「バッファへのポインタ + 長さ + 宛先種別」などメタデータのみ  
  - メモリ目安: `maxPayloadBytes * maxQueueLength` + メタデータ。例: 1470B × 16 ≒ 24KB + α  
  - メモリが厳しい場合は `maxPayloadBytes` を 250 などに、`maxQueueLength` も小さめに調整  
  - 事前確保に失敗した場合は `begin()` が `false` を返す
- キュー投入のタイムアウトは `timeoutMs` 引数で指定。`kUseDefault` の場合は `Config.sendTimeoutMs` を使用  
  - `timeoutMs = 0`: 非ブロック  
  - `timeoutMs = portMAX_DELAY`: 無期限ブロック（ISR では使用不可）  
  - `kUseDefault` は `portMAX_DELAY - 1` を特別値として使用（`portMAX_DELAY` と衝突させないため）
  - デフォルト `sendTimeoutMs = 50` ms 程度を想定し、必要に応じて変更
- 送信タスク内の送信状態管理（シングルスロット）  
  - 「送信中フラグ」と `currentTx` を保持。キューから取り出したら即 ESP-NOW 送信し、送信開始時刻を記録してフラグ ON  
  - ESP-NOW の送信完了コールバックでは状態を直接触らず、FreeRTOS のタスク通知（`xTaskNotifyFromISR`）で送信タスクへ結果を渡す  
  - 送信タスク側は通知を受けたら送信中フラグを OFF にし、結果を onSendResult へ通知  
  - 送信中フラグが ON のまま `txTimeoutMs` を超えたらタイムアウト扱いで失敗→リトライ判定へ  
- 送信リトライ: タイムアウト or ESP-NOW 送信失敗時に、同じ `msgId/seq` を保持したまま `Config.maxRetries` 回まで即再送（`retryDelayMs` が 0 の場合）  
  - `retryDelayMs` を設定した場合はその間隔をあける（指数バックオフする場合も初期値として利用）  
  - リトライ時は `flags.isRetry=1` をセット  
  - 全試行が失敗したら onSendResult で `SendFailed` を通知  
- 送信タスクはデフォルトで ARDUINO_RUNNING_CORE（loop と同じコア）にピン留めし、優先度 3・スタック 4096B で生成  
  - `taskCore = -1` でピン留めなし、0/1 でコア指定可  
  - 優先度を上げ過ぎると WiFi/ESP-NOW タスクを妨げる可能性あり
- 物理 ACK（ESP-NOW の MAC 層 ACK）は、復号に失敗しても返る点に注意。`onSendResult(SentOk)` は「物理送信成功」を意味し、論理的な到達は保証しない  
- アプリ層 ACK（論理 ACK）: `enableAppAck=true` の場合、ユニキャスト受信時に msgId を含む Ack パケットを自動返信し、送信側は Ack 未達ならリトライ/再JOIN を行う  
  - 物理 ACK が無くても論理 ACK を受け取れた場合は「到達成功」とみなしつつ警告ログを残す  
  - 物理 ACK だけで論理 ACK が無い場合は「未達/不明」としてリトライまたは再JOIN を行う
  - app-ACK 有効のユニキャストでは `AppAckReceived` を最終成功、`AppAckTimeout` を失敗として通知し、`SentOk` は app-ACK 無効時のみの完了通知（物理 ACK は完了判定に使わない）
- 連続失敗時の自動パージ（オプション）: `maxAckFailures>0` の場合、`failureWindowMs` 内に連続で `AppAckTimeout`/`SendFailed` がしきい値を超えたピアを `removePeer`。`rejoinAfterPurge=true` ならパージ後に `sendRegistrationRequest()` を自動送信する
  - onSendResult の完了は AppAckReceived（論理 ACK）を以て成功とし、AppAckTimeout で失敗扱い（物理送信成功だけでは完了としない）

### 8.2 受信
- BaseHeader → PacketType で分岐
- DataUnicast → 認証済み peer のみ許可
- DataBroadcast → groupId & authTag を検証
- ControlJoinReq → 自動ペア登録フローへ渡す

### 8.3 自動ペア登録
- 募集（JOIN 要請）はブロードキャストで送信し、設定により自動・手動を併用する  
  - 自動募集: 設定値 `0` で無効、`>0` でその ms 間隔で定期送信  
  - 手動募集: 任意タイミングで API を呼んで送信  
  - 全体募集（誰でも応募可）と、特定 MAC を明示した「対象限定募集」を使い分けられる。後者は既存ペアだけを再接続したいときに使う
- 受信側の応募判定  
  - groupId 不一致は無視  
  - 未ペア端末からの募集 → 応募する  
  - 既存ペアからの募集 → 直近ハートビート時刻と送信失敗カウントを確認し、リンク切れの可能性があれば再応募。通信良好なら無視
- 片側が再起動してユニキャストが届かなくなった場合でも、ブロードキャスト募集（必要なら対象限定）で再ペアリングできる前提の運用とする

#### 要求側（全ノード）
```
sendRegistrationRequest()
↓
ControlJoinReq をブロードキャスト（groupId + authTag）
```

#### 受け入れ側（Master / Flat）
1. JOIN要求を受信  
2. groupId / authTag を検証  
3. `canAcceptRegistrations && acceptRegistration == true` の場合のみ対応  
4. 認証OK → addPeer()  
5. ControlJoinAck を返す（ユニキャスト）

### 8.4 重複検出・リトライ扱い
- Unicast: peer ごとに最後に受理した `msgId` を記録し、同一 `msgId`（リトライ）は破棄（必要なら onReceive に「リトライだった」メタ情報を渡す）  
- Broadcast: `seq` の再送は authTag 検証後、リプレイ窓で破棄。`flags.isRetry` はデバッグ用フラグとして利用  
- リプレイ窓幅は 16〜64 程度を想定し、オーバーフロー時も最も近い未来方向のみを受理する簡易窓で実装
- 論理 ACK: 受信側が重複と判定して UserPayload を渡さなかった場合でも、`enableAppAck=true` なら msgId を含む Ack を返信する（送信側の再送抑止のため）
- onSendResult のステータス例: `Queued`, `SentOk`, `SendFailed`, `Timeout`, `DroppedFull`, `DroppedOldest`, `TooLarge`, `Retrying`, `AppAckReceived`, `AppAckTimeout` を固定列挙で定義
- ControlAppAck のリプレイ: in-flight の msgId と一致するもののみ受理し、その他は無視（警告ログ）。16bit msgId の wrap によりごく稀に誤完了の可能性はあるが許容する方針
- リプレイ窓設定 `replayWindowBcast`/`Join` は実質 64 が上限（内部は 64bit）。設定値が 64 を超える場合は 64 に丸めて扱う

### 8.5 ハートビートとペア維持
- ユニキャスト受信や暗号化済みのソフト ACK を受信したタイミングで「生存確認時刻」と「失敗カウント」をリセットする
- 設定したハートビート確認時間を超過したら、ユニキャストでハートビート確認パケットを送信する（ピアが外れていれば暗号化解除に失敗して受信できない想定）
- ハートビート確認時間の **2 倍** を超過したら、ペア先の MAC を含めた対象限定のブロードキャスト募集を送信し、再ペアリングを試みる
- **3 倍** 超過したら生存していないと判定し、ペアを解除する
- 片側再起動によるユニキャスト不達を吸収するため、上記の対象限定募集でリンク復旧を優先する設計とする

---

## 9. 想定ユースケース
- センサーノード → ゲートウェイ  
- コントローラ → 複数ロボット  
- 小人数のゲームネットワーク  
- イベント会場のローカル連携ガジェット

---

## 10. v1 で扱わない範囲
- 多段中継 / メッシュ / ルーティング
- ロビー管理（募集期間の制御など）
- プレイヤーID管理
- 高度なノードロール制御
