# EspNowSerial 仕様案

## 1. 目的
`EspNowSerial` は、`EspNowBus` を内部 transport として利用し、peer ごとの Serial Session を管理する Hub クラスである。  
各 Session は `EspNowSerialPort` として公開され、Arduino の `Stream` / `Print` に近い使い勝手で 1:1 のバイトストリーム通信を提供する。

この仕様は以下を重視する。

- `EspNowBus` の既存機能を最大限そのまま活かす
- peer が増えたら Serial Session も自動的に増える
- `EspNowSerialPort` をグローバル変数で安全に定義できる
- 将来の他プロトコル実装の足場になるよう、Bus 上位の責務境界を明確にする

## 2. 設計方針

### 2.1 レイヤ分離
- `EspNowBus`
  - peer 発見
  - JOIN / 再JOIN
  - peer 管理
  - ESP-NOW 送受信
  - unicast の送信直列化
  - app-ACK ベースの再送
  - 重複抑止
  - heartbeat と切断検知
- `EspNowSerial`
  - `EspNowBus` の上位で Serial Session を自動管理
  - peer と session の対応付け
  - `EspNowSerialPort` の bind / unbind 管理
  - Serial 用 protocol multiplexing
- `EspNowSerialPort`
  - 1 Session に bind される `Stream` 互換インターフェイス
  - RX/TX FIFO を通じた byte stream API

### 2.2 非目標
以下を対象外とする。

- mesh / multi-hop / routing
- 1 `EspNowSerialPort` が複数 session を同時に扱うこと
- Bus とは別実装の再送・再順序化レイヤ
- 厳密な baud rate 再現
- `HardwareSerial` 互換の割り込み駆動

## 3. 基本モデル

### 3.1 通信モデル
- 論理的には peer ごとに 1 本の full-duplex byte stream を持つ
- 物理的には `EspNowBus::sendTo()` による unicast で運ぶ
- packet 境界は `EspNowSerialPort` 利用者に見せない
- `EspNowSerial` は複数 peer 分の Session を持てる
- `EspNowSerialPort` はそのうち 1 Session に bind される

### 3.2 トポロジ
- ネットワーク上に複数 peer が存在してよい
- 各 peer に対して独立した Serial Session を持つ
- peer が `EspNowBus` で追加されたら、対応する Serial Session も自動生成される
- peer が離脱したら、対応する Session は切断状態へ遷移する

### 3.3 Session の考え方
Session は「peer に紐付く論理ストリームの文脈」を意味する。  
再送、重複抑止、heartbeat などの transport 信頼性は主に `EspNowBus` が担い、`EspNowSerial` はその上位で Session と `Stream` API を提供する。

Session が保持する主な状態:
- bind 先 peer MAC
- 接続状態
- sessionNonce
- RX/TX FIFO
- port に bind 済みかどうか

## 4. コンポーネント

### 4.1 `EspNowBus`
- 公開 transport API
- `EspNowSerial` はこれを内部利用する
- `groupName` や JOIN 設定は `EspNowSerial` からまとめて与えられる

### 4.2 `EspNowSerial`
責務:
- `EspNowBus` の所有またはラップ
- peer ごとの Serial Session 自動生成
- Session 一覧管理
- 受信 packet の session への振り分け
- `EspNowSerialPort` の bind 先解決

想定利用イメージ:

```cpp
EspNowSerial serialHub;
EspNowSerialPort controlSerial;
```

```cpp
void setup() {
  EspNowSerial::Config cfg;
  cfg.groupName = "my-group";

  serialHub.begin(cfg);
  controlSerial.bindFirstAvailable(serialHub);
}
```

### 4.3 `EspNowSerialPort`
責務:
- `Stream` 互換 API
- 特定 Session への bind
- 未接続時も安全に存在

重要要件:
- グローバル変数で定義可能であること
- `setup()` 前に bind 未完了でも安全であること
- 未接続時は `available() == 0`, `read() == -1`, `peek() == -1`, `write()` は 0 を返せること

### 4.4 将来拡張向けの上位プロトコル識別
将来 `Serial` 以外の上位プロトコルを同じ `EspNowBus` 上に載せるため、Bus に渡す user payload 先頭に上位プロトコル識別ヘッダを置く。

```cpp
struct EspNowBusAppHeader {
    uint8_t protocolId;   // 0x01 = Serial
    uint8_t protocolVer;  // 1
    uint8_t packetType;   // protocol specific
    uint8_t flags;
};
```

- `protocolId` は将来の protocol multiplexer 用予約
- `EspNowSerial` は `protocolId == 0x01` のみ処理

## 5. Session モデル

### 5.1 Session の生成
- `EspNowBus` で peer が利用可能になった時点で Session を自動生成する
- Session は peer と 1:1 に対応する
- 1 peer あたり 1 Serial Session

### 5.2 Session の寿命
- peer 発見または JOIN 成功で `Disconnected -> Connected`
- peer timeout / leave / remove で `Connected -> Disconnected`
- peer 再参加で同じ Session を再利用してもよいが、`sessionNonce` は更新する

### 5.3 Session slot と index
- Session は固定長 slot 配列で管理する
- 各 Session は stable な index を持つ
- peer 切断時も slot は維持し、Session 状態のみ `Disconnected` に遷移する
- 同じ peer が再接続した場合は、可能な限り同じ slot を再利用する
- Session index は controller 側スケッチから参照できる安定識別子として扱う

### 5.4 Session と Port の関係
- Session は hub 内部の通信単位
- `EspNowSerialPort` は Session を参照するハンドル
- 1 Port は 1 Session のみ参照する
- 1 Session に複数 Port を bind することは想定しない

方針:
- 1 Session に対して bind できる `EspNowSerialPort` は 1 つまで

## 6. Port バインドモデル

### 6.1 bind 単位
- `EspNowSerialPort` は 1 Session に bind される
- bind 対象は内部的には Session ID または peer MAC で識別する

### 6.2 bind 方法
以下を持つ。

- `bind(mac)`
  - 指定 peer の Session に bind
- `bindFirstAvailable()`
  - 最初に利用可能になった Session に bind
- `unbind()`
  - Port と Session の関連を解除

### 6.3 遅延 bind
- `EspNowSerialPort` は bind 先がまだ存在しなくても生成可能
- `bind(mac)` を先に呼んでおき、実際の Session 生成後に接続成立としてよい
- `bindFirstAvailable()` は該当 Session が見つかるまで Pending 状態で待機してよい

### 6.4 未接続時の挙動
未接続とは以下を含む。

- hub 未開始
- bind 未設定
- bind 先 Session 未生成
- peer 切断済み

このとき `EspNowSerialPort` は無効状態として振る舞う。

- `available()` は `0`
- `read()` は `-1`
- `peek()` は `-1`
- `write()` は `0`
- `connected()` は `false`
- `operator bool()` を持つなら `false`

## 7. Serial プロトコル

### 7.1 パケット種別

```cpp
enum EspNowSerialPacketType : uint8_t {
    SerialHello     = 1,
    SerialHelloAck  = 2,
    SerialData      = 3,
    SerialClose     = 4,
};
```

### 7.2 `SerialHello`
用途:
- Session の論理接続開始
- 再接続時のエポック更新

```cpp
struct SerialHelloPayload {
    uint8_t  endpointId;
    uint8_t  options;
    uint16_t mtuHint;
    uint32_t sessionNonce;
};
```

### 7.3 `SerialHelloAck`
用途:
- Session 接続確定

```cpp
struct SerialHelloAckPayload {
    uint8_t  endpointId;
    uint8_t  options;
    uint16_t mtuHint;
    uint32_t peerSessionNonce;
    uint32_t localSessionNonce;
};
```

### 7.4 `SerialData`
用途:
- 実データ搬送

```cpp
struct SerialDataHeader {
    uint8_t  endpointId;
    uint8_t  reserved;
    uint32_t sessionNonce;
};
```

- `sessionNonce` は現在の Session と一致しない場合に破棄
- 独自 seq は持たない
- 順序性・再送・重複抑止は `EspNowBus` の unicast + app-ACK に委譲する

### 7.5 `SerialClose`
用途:
- 明示切断

```cpp
struct SerialClosePayload {
    uint8_t  endpointId;
    uint8_t  reason;
    uint16_t reserved;
    uint32_t sessionNonce;
};
```

## 8. 状態遷移

### 8.1 Hub 状態
- `Stopped`
- `Running`

### 8.2 Session 状態
- `Unbound`
  - Session はあるが Port 未 bind
- `Bound`
  - Port が bind 済み
- `Disconnected`
  - peer は消失または未接続

補足:
- Session は hub 内部には存在していても、Port が bind されていない限り `Unbound`
- Port から見た usable 状態は「Session が存在し、かつ peer が Connected」である

### 8.3 Port 状態
- `Detached`
  - hub 未 bind
- `Pending`
  - hub はあるが Session 未確定
- `Attached`
  - Session 参照あり
- `Active`
  - Session 参照があり、peer 接続済み

## 9. データモデル

### 9.1 Stream 抽象
- 利用者に見えるのは byte stream
- `write()` は Session の TX FIFO に積まれる
- Bus 送信時に適切な大きさで分割される

### 9.2 フラグメンテーション
- 1 Bus payload に載る Serial 生データ量は以下で決まる

```text
serialPayloadMax =
  EspNowBus.Config.maxPayloadBytes
  - EspNowBus header (6 bytes)
  - EspNowBusAppHeader
  - SerialDataHeader
```

- 現行の unicast data では `serialPayloadMax = Config.maxPayloadBytes - 16` として扱う
- 内訳は `EspNowBus` header 6 byte + `EspNowBusAppHeader` 4 byte + `SerialDataHeader` 6 byte
- `EspNowSerial` は `write()` されたデータを `serialPayloadMax` 単位で分割して送信する
- 受信側は受け取った `SerialData` payload をそのまま RX FIFO に連結する

### 9.3 順序保証
- `EspNowBus` の「1 in-flight / app-ACK / retry」モデルを前提に、同一 peer 間の到着順維持を期待する
- `EspNowSerial` 自身は out-of-order 再整列バッファを持たない
- 仕様上の順序保証は transport が提供する範囲に限定する

## 10. バッファリング

### 10.1 TX
- 各 Session は独立した TX FIFO を持つ
- `EspNowSerialPort::write()` は bind 中 Session の TX FIFO に積む
- bind 先が無効なら 0 を返す

### 10.2 RX
- 各 Session は独立した RX FIFO を持つ
- `available()`, `read()`, `peek()` は bind 中 Session の RX FIFO を読む
- overflow 時は新規データ拒否を基本方針とする

## 11. 実行モデル

### 11.1 イベント駆動
- `EspNowBus` 受信 callback を `EspNowSerial` が購読する
- peer 追加・離脱イベントに応じて Session を更新する

### 11.2 バックグラウンド処理
推奨:
- `EspNowSerial::poll()` を `loop()` から呼ぶ

`poll()` の責務:
- Session の TX FIFO flush
- `SerialHello` / `SerialHelloAck` / `SerialClose` の進行
- Pending bind の解決
- 切断 session の再接続処理

`poll()` の性質:
- non-blocking を前提とする
- FreeRTOS task のような常駐処理ではなく、軽量な状態更新関数として扱う
- 1 回の呼び出しで過剰に処理しすぎず、短時間で復帰する
- `EspNowBus` 側が送信待ち中でも長く待たずに戻る

### 11.3 Port アクセス時の自動補助 poll
- `available()`, `read()`, `peek()`, `write()`, `flush()` などの `EspNowSerialPort` API は、必要に応じて hub 側の軽量 poll を内部で呼び出してよい
- これにより、利用者が `loop()` で明示的に `poll()` を呼ばない場合でも最低限の進行を確保する
- 自動 poll は毎回フル実行せず、直近で実行済みなら即 return してよい

内部方針:
- hub は `pollIfNeeded()` 相当の内部関数を持ってよい
- `pollIfNeeded()` は直近実行時刻や再入防止フラグを見て、不要なら何もしない
- 明示 `poll()` が推奨経路であり、自動 poll は補助機構とする

## 12. API 仕様案

### 12.1 Hub

```cpp
class EspNowSerial {
public:
    struct Config {
        const char* groupName = nullptr;
        uint8_t endpointId = 0;
        size_t sessionCount = 8;
        size_t rxBufferSize = 512;
        size_t txBufferSize = 512;
        bool autoReconnect = true;
    };

    bool begin(const Config& cfg);
    void end();
    void poll();

    size_t sessionCapacity() const;
    bool sessionInUse(size_t index) const;
    bool sessionConnected(size_t index) const;
    int sessionAvailable(size_t index) const;
    bool sessionMac(size_t index, uint8_t macOut[6]) const;
    bool hasSession(const uint8_t mac[6]) const;
};
```

### 12.2 Port

```cpp
class EspNowSerialPort : public Stream {
public:
    EspNowSerialPort() = default;

    bool attach(EspNowSerial& hub);
    void detach();

    bool bind(const uint8_t mac[6]);
    bool bindSession(size_t index);
    bool bindFirstAvailable();
    void unbind();

    bool connected() const;
    bool bound() const;
    int availableForWrite();
    size_t read(uint8_t* buffer, size_t size);
    size_t readBytes(uint8_t* buffer, size_t length);

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    size_t write(uint8_t b) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    size_t printf(const char* format, ...);
    size_t vprintf(const char* format, va_list args);

    using Print::write;
};
```

### 12.3 `Stream` / `Print` 互換
- `EspNowSerialPort` は `Stream` / `Print` 互換を提供する
- `print(...)` / `println(...)` 群は `Print` 継承により利用できる前提とする
- 出力系 API は `write`, `print`, `println`, `printf`, `vprintf`, `flush` を中心とする
- 入力系 API は `available`, `availableForWrite`, `peek`, `read`, `read(buffer, size)`, `readBytes(...)` を中心とする

### 12.4 Hub の最小管理 API
- `EspNowSerial` の公開管理機能は最小限に留める
- controller 側で必要な責務は「Session 一覧取得」と「Session 切り替え」とする
- 高度な管理機構や中央集約受信 API は持たない

用途:
- Session 一覧表示
- 接続状態確認
- データが溜まっている Session の確認
- `EspNowSerialPort` の bind 先切り替え

### 12.5 `printf` 系 API
- `EspNowSerialPort` は `Print` 互換に加えて、ESP32 の `Serial` に近い `printf` / `vprintf` を提供する
- フォーマット結果は内部で一時バッファ化し、`write()` と同じ送信経路へ流す
- 未接続時は `write()` と同様に 0 を返してよい
- 一時バッファ上限を超える場合は切り詰めまたは送信失敗のどちらかを実装方針として選べるが、仕様上は安全に失敗できることを優先する

### 12.6 UART 固有 API との線引き
- `EspNowSerialPort` は `HardwareSerial` の完全互換を目的としない
- UART ハードウェア固有の設定 API は持たない
- 例:
  - baud rate 設定
  - RX/TX pin 設定
  - FIFO しきい値設定
  - フロー制御設定
  - UART mode / clock source / inversion 設定
  - UART 割り込み前提の `onReceive` / `onReceiveError`

### 12.7 グローバル定義前提
以下のような利用を許容する。

```cpp
EspNowSerial serialHub;
EspNowSerialPort controlSerial;
EspNowSerialPort debugSerial;
```

```cpp
void setup() {
  EspNowSerial::Config cfg;
  cfg.groupName = "my-group";

  serialHub.begin(cfg);

  controlSerial.attach(serialHub);
  controlSerial.bindFirstAvailable();
}
```

controller 側の最小利用例:

```cpp
for (size_t i = 0; i < serialHub.sessionCapacity(); ++i) {
  if (!serialHub.sessionInUse(i)) continue;
  if (!serialHub.sessionConnected(i)) continue;
  if (serialHub.sessionAvailable(i) <= 0) continue;

  controlSerial.bindSession(i);
  while (controlSerial.available()) {
    Serial.write(controlSerial.read());
  }
}
```

## 13. `EspNowBus` との責務境界

### 13.1 `EspNowSerial` が再実装しないもの
- peer 探索
- JOIN
- 暗号化
- app-ACK
- packet 再送
- 物理送信失敗検知
- heartbeat

### 13.2 `EspNowSerial` / `EspNowSerialPort` が追加するもの
- peer ごとの Session 管理
- Session への Port bind
- Stream API
- sessionNonce による論理リンク世代管理
- RX/TX FIFO
- 将来 protocol multiplexing の入口

## 14. 将来の他プロトコルへの布石

### 14.1 protocolId の予約
- `0x01`: Serial
- `0x02` 以降: 将来予約

### 14.2 推奨アーキテクチャ
`EspNowBus` の callback を各上位プロトコルが直接奪い合う構成は避ける。  
`EspNowSerial` の Session / Port モデルは、将来の別プロトコルでも同様に「Hub + Port」構造へ展開しやすい。

## 15. 実装範囲
MVP としては以下を対象にする。

- `EspNowSerial` hub
- peer ごとの自動 Session 生成
- `EspNowSerialPort : public Stream`
- `attach`, `bind`, `bindFirstAvailable`, `unbind`
- 未接続時の no-op / invalid 振る舞い
- `Hello` / `HelloAck` / `Data` / `Close`
- Session ごとの RX/TX ring buffer

対象外:
- 1 Session への複数 Port bind
- 複数 endpoint の同時運用
- dispatcher の public 化
- 独自再送窓
- フロー制御
