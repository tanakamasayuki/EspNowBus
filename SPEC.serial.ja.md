# EspNowSerial 仕様

## 1. 目的
`EspNowSerial` は、`EspNowBus` を内部 transport として利用し、peer ごとの Serial Session を管理する Hub クラスである。  
各 Session は `EspNowSerialPort` として公開され、Arduino の `Stream` / `Print` に近い使い勝手で 1:1 のバイトストリーム通信を提供する。

この仕様は以下を重視する。

- `EspNowBus` の既存機能を最大限そのまま活かす
- peer が増えたら Serial Session も自動的に増える
- `EspNowSerialPort` をグローバル変数で安全に定義できる
- 実装を過剰に複雑化しない

## 2. 基本モデル

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
  - peer ごとの Session 管理
  - Session 一覧管理
  - `EspNowSerialPort` の bind 先解決
  - Serial payload の分割・送信
- `EspNowSerialPort`
  - 1 Session に bind される `Stream` / `Print` 互換インターフェイス
  - RX/TX FIFO を通じた byte stream API

### 2.2 利用トポロジ
- `EspNowSerial` は 1 つの hub として動作する
- hub は複数 peer を持てる
- peer ごとに 1 つの Serial Session を持つ
- `EspNowSerialPort` は hub 内の 1 Session に bind される
- controller 側では Session を切り替えて複数 peer を扱える
- device 側では `bindFirstAvailable()` や `bind(mac)` で特定 Session に接続できる

### 2.3 Hub / Session / Port の関係
- hub
  - Session 一覧を管理する
- Session
  - peer ごとの通信文脈と RX/TX FIFO を保持する
- Port
  - Session を利用者に `Stream` / `Print` として見せる

利用イメージ:

```cpp
EspNowSerial serialHub;
EspNowSerialPort controlSerial;
EspNowSerialPort debugSerial;
```

- `serialHub` は複数 Session を保持できる
- `controlSerial` は現在選択中の 1 Session を扱う
- `debugSerial` も別の Session に bind できる

### 2.2 非目標
以下を対象外とする。

- mesh / multi-hop / routing
- 1 `EspNowSerialPort` が複数 session を同時に扱うこと
- Bus とは別実装の再送・再順序化レイヤ
- 厳密な baud rate 再現
- `HardwareSerial` 完全互換

## 3. Session モデル

### 3.1 Session の生成
- `EspNowBus` で peer が ready になった時点で Session を自動生成する
- Session は peer と 1:1 に対応する
- 1 peer あたり 1 Serial Session

### 3.2 Session slot と index
- Session は固定長 slot 配列で管理する
- 各 Session は stable な index を持つ
- peer 切断時も slot は維持し、接続状態のみ更新する
- 同じ peer が再接続した場合は、可能な限り同じ slot を再利用する

### 3.3 Session と Port の関係
- Session は hub 内部の通信単位
- `EspNowSerialPort` は Session を参照するハンドル
- 1 Port は 1 Session のみ参照する
- 1 Session に複数 Port を bind することは想定しない

## 4. Port バインド

### 4.1 bind 方法
以下を持つ。

- `bind(mac)`
  - 指定 peer の Session に bind
- `bindSession(index)`
  - 指定 index の Session に bind
- `bindFirstAvailable()`
  - 最初に利用可能になった Session に bind
- `unbind()`
  - Port と Session の関連を解除

### 4.2 遅延 bind
- `EspNowSerialPort` は bind 先がまだ存在しなくても生成可能
- `bind(mac)` を先に呼んでおき、該当 Session が生成された後で bind を成立させる
- `bindFirstAvailable()` は該当 Session が見つかるまで待機する

### 4.3 未接続時の挙動
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

## 5. Serial プロトコル

### 5.1 上位ヘッダ
Bus 上の user payload 先頭には Serial 用の上位ヘッダを置く。

```cpp
struct EspNowBusAppHeader {
    uint8_t protocolId;   // 0x01 = Serial
    uint8_t protocolVer;  // 1
    uint8_t packetType;   // 1 = SerialData
    uint8_t flags;
};
```

### 5.2 パケット種別

```cpp
enum EspNowSerialPacketType : uint8_t {
    SerialData = 1,
};
```

### 5.3 `SerialData`
用途:
- 実データ搬送

```cpp
struct SerialDataHeader {
    uint8_t  endpointId;
    uint8_t  reserved;
    uint32_t sessionNonce;
};
```

- `sessionNonce` は Session ごとの識別値として保持する
- 独自 seq は持たない
- 順序性・再送・重複抑止は `EspNowBus` の unicast + app-ACK に委譲する

## 6. データモデル

### 6.1 Stream 抽象
- 利用者に見えるのは byte stream
- `write()` は Session の TX FIFO に積まれる
- packet 境界は `EspNowSerialPort` 利用者に見せない

### 6.2 フラグメンテーション
- 1 Bus payload に載る Serial 生データ量は以下で決まる

```text
serialPayloadMax =
  EspNowBus.Config.maxPayloadBytes
  - EspNowBus header (6 bytes)
  - EspNowBusAppHeader (4 bytes)
  - SerialDataHeader (6 bytes)
```

- 現行の unicast data では `serialPayloadMax = Config.maxPayloadBytes - 16` として扱う
- `EspNowSerial` は `write()` されたデータを `serialPayloadMax` 単位で分割して送信する
- 受信側は受け取った `SerialData` payload をそのまま RX FIFO に連結する

### 6.3 順序保証
- `EspNowBus` の「1 in-flight / app-ACK / retry」モデルを前提に、同一 peer 間の到着順維持を期待する
- `EspNowSerial` 自身は out-of-order 再整列バッファを持たない

## 7. バッファリング

### 7.1 TX
- 各 Session は独立した TX FIFO を持つ
- `EspNowSerialPort::write()` は bind 中 Session の TX FIFO に積む
- bind 先が無効なら 0 を返す

### 7.2 RX
- 各 Session は独立した RX FIFO を持つ
- `available()`, `read()`, `peek()` は bind 中 Session の RX FIFO を読む
- overflow 時は新規データ拒否を基本方針とする

## 8. 実行モデル

### 8.1 `poll()`
- `EspNowSerial::poll()` を `loop()` から呼ぶ
- `poll()` は non-blocking を前提とする
- `poll()` は以下を進める
  - Session の TX FIFO flush
  - Pending bind の解決
  - Session 一覧の更新

### 8.2 自動補助 poll
- `available()`, `read()`, `peek()`, `write()`, `flush()` などの `EspNowSerialPort` API は、必要に応じて hub 側の軽量 poll を内部で呼び出す
- 明示 `poll()` を基本経路とし、自動 poll は補助機構とする

## 9. API 仕様

### 9.1 Hub

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
        bool advertise = true;

        bool useEncryption = true;
        bool enablePeerAuth = true;
        bool enableAppAck = true;
        int8_t channel = -1;
        wifi_phy_rate_t phyRate = WIFI_PHY_RATE_11M_L;
        uint16_t maxQueueLength = 16;
        uint16_t maxPayloadBytes = EspNowBus::kMaxPayloadDefault;
        uint32_t sendTimeoutMs = 50;
        uint8_t maxRetries = 1;
        uint16_t retryDelayMs = 0;
        uint32_t txTimeoutMs = 120;
        uint32_t autoJoinIntervalMs = 30000;
        uint32_t heartbeatIntervalMs = 10000;
        int8_t taskCore = ARDUINO_RUNNING_CORE;
        UBaseType_t taskPriority = 3;
        uint16_t taskStackSize = 4096;
        uint16_t replayWindowBcast = 32;
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

### 9.2 Port

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

### 9.3 `Stream` / `Print` 互換
- `EspNowSerialPort` は `Stream` / `Print` 互換を提供する
- `print(...)` / `println(...)` 群は `Print` 継承により利用できる前提とする
- 出力系 API は `write`, `print`, `println`, `printf`, `vprintf`, `flush` を中心とする
- 入力系 API は `available`, `availableForWrite`, `peek`, `read`, `read(buffer, size)`, `readBytes(...)` を中心とする

### 9.4 UART 固有 API との線引き
- `EspNowSerialPort` は `HardwareSerial` の完全互換を目的としない
- UART ハードウェア固有の設定 API は持たない

### 9.5 グローバル定義前提
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

## 10. `EspNowBus` との責務境界

### 10.1 `EspNowSerial` が再実装しないもの
- peer 探索
- JOIN
- 暗号化
- app-ACK
- packet 再送
- 物理送信失敗検知
- heartbeat

### 10.2 `EspNowSerial` / `EspNowSerialPort` が追加するもの
- peer ごとの Session 管理
- Session への Port bind
- `Stream` / `Print` API
- RX/TX FIFO

## 11. 現在の実装範囲
現在の実装は以下を含む。

- `EspNowSerial` hub
- peer ごとの自動 Session 生成
- `EspNowSerialPort : public Stream`
- `attach`, `bind`, `bindSession`, `bindFirstAvailable`, `unbind`
- 未接続時の no-op / invalid 振る舞い
- `Data`
- Session ごとの RX/TX ring buffer

現在の実装に含まれないもの:
- 1 Session への複数 Port bind
- 複数 endpoint の同時運用
- 独自再送窓
- フロー制御
