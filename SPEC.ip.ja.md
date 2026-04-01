# EspNowIP 仕様書

## 1. 用語
- `device`
  - `EspNowIP` を使って gateway に接続する端末。
- `gateway`
  - `EspNowIPGateway` を使って device を収容し、uplink 側へ `IPv4 NAT` で中継する端末。
- `uplink`
  - gateway が接続する上位 IP ネットワーク。インターネットを含む。
- `gateway service`
  - gateway 自身が提供する HTTP API、診断 API、設定 UI などのローカル IP サービス。
- `gateway candidate`
  - `ESP-NOW` 上で募集を出しており、device が `EspNowBus` session を確立できる接続先候補になりうる gateway。
- `active gateway`
  - device が現在 `EspNowIP` の `IP session` を張っている gateway。
- `Bus session`
  - `EspNowBus` で成立した下位レイヤーの接続状態。
- `IP session`
  - `EspNowIP` で成立した上位レイヤーの論理リンク状態。
- `link context`
  - 1 device と 1 gateway の間の `IP session`、lease、送受信状態を保持する文脈。
- `control plane`
  - lease、MTU、keepalive、接続状態の管理に使う制御通信。
- `data plane`
  - IPv4 packet 本体を搬送する通信。

## 2. 目的
`EspNowIP` は、`EspNowBus` を内部 transport として利用し、`ESP-NOW` 上に `IP` 通信を載せるレイヤである。  
device 側では `esp_netif` / `lwIP` から見える仮想 `NetIF` を提供し、gateway 側ではその仮想リンクを uplink 側 IP ネットワークへ `IPv4 NAT` で中継する。

この仕様では以下を前提とする。

- `EspNowBus` の既存機能を最大限そのまま活かす
- device 側は `socket` / `HTTP` / `MQTT` など既存の IP API をそのまま使う
- gateway 側は複数 device を収容し、uplink へ `IPv4 NAT` で中継する
- gateway 側は `routing + IPv4 NAT` を行う

この仕様の目標は以下とする。

- Wi-Fi を通常利用しにくい端末や、ノイズが多い環境下でも `ESP-NOW` を使って uplink 側 IP ネットワークへ到達できること
- 低速であっても、`socket` など通常のネットワーク IF に近い形でアプリケーションを実装できること
- 数秒から数十秒単位の再接続を許容する用途で、通常の IP ベースのアプリケーションを動作させること

この仕様が主に対象とする用途は以下とする。

- MQTT など再接続前提のクライアント通信
- 数十秒から数分周期のデータ収集やアップロード
- gateway service を用いた設定、診断、保守通信

この仕様は以下の用途を主対象としない。

- 常時低遅延を要求するリアルタイム制御
- 連続ストリーミング
- 数秒未満の断時間も許容しにくい通信

## 3. 基本モデル

### 3.1 レイヤ分離
- `EspNowBus`
  - gateway candidate の発見
  - JOIN / 再JOIN
  - 接続先 MAC の管理
  - ESP-NOW 送受信
  - unicast の送信直列化
  - app-ACK ベースの再送
  - 重複抑止
  - heartbeat と切断検知
- `EspNowIP`
  - `Bus session` ごとの `IP session` 試行
  - device ごとの IP link 管理
  - `esp_netif` custom I/O driver の提供
  - IP packet の分割・再構成
  - address lease 管理
  - gateway との control plane
- `esp_netif` / `lwIP`
  - IPv4 処理
  - socket API
  - routing
  - DNS などの上位ネットワーク機能
- `EspNowIPGateway`
  - device 側論理 link の収容
  - uplink 側 interface への転送
  - `IPv4 NAT` の実行

### 3.2 利用トポロジ
- gateway は `EspNowBus` の募集を出す
- device は gateway の募集を受けて `EspNowBus` の接続を行う
- gateway は `ESP-NOW` 側の複数 device を収容できる
- device は複数の `Bus session` を持てる
- device 側は `Bus session` ごとに `IP session` を張れるか順に試す
- device 側は成功した 1 つの `IP session` のみを active にする
- device から到達できる宛先は gateway 自身と gateway uplink 側ネットワークのみとする
- gateway は device 間の IPv4 転送を行わない

### 3.3 データ面と制御面
- データ面
  - IPv4 packet を `ESP-NOW` unicast payload に載せる
- 制御面
  - address lease
  - MTU 通知
  - keepalive / link-state 更新
  - gateway 存在確認
  - gateway service への到達性

### 3.4 非対象
以下を対象外とする。

- IPv6
- mesh / multi-hop / routing protocol
- 透過 Ethernet bridge
- 802.11 / Ethernet の L2 フレーム転送
- DHCP 透過
- broadcast / multicast 転送
- STP / LLDP / mDNS reflector / SSDP relay
- `ESP-NOW` 上での汎用 VPN 相当の実装

## 4. 設計前提

### 4.1 前提条件
- `ESP-NOW` は connectionless であり、アプリ層到達保証は別途必要である
- `ESP-NOW` の送信 callback / 受信 callback は Wi-Fi task 上で動くため、重い処理は下位 task へ逃がす
- `ESP-NOW` は 1 packet あたり最大 1470 bytes を前提とする
- 250 bytes 上限の古い `ESP-NOW` 環境は非推奨とする
- `esp_netif` custom I/O driver では `transmit`, `driver_free_rx_buffer`, `esp_netif_receive()` の接続が必要である
- `lwIP` の packet 処理は `esp_netif` 経由で扱う

### 4.2 採用する構成
- 論理リンクは `L3 point-to-point IPv4 link` とする
- gateway 側の uplink 接続は `routing + IPv4 NAT` のみとする
- `L2 透過 bridge` は対象外とする

理由:
- `ESP-NOW` は Ethernet のような共有 L2 ではなく、payload 長と broadcast の扱いに制約がある
- device 側で必要なのは IP 利用であり、Ethernet segment の完全再現ではない
- `esp_netif` / `lwIP` は `routing + NAT` の方が構成を素直に組みやすい

## 5. 論理リンクモデル

### 5.1 device 側
- `EspNowIP` は `esp_netif` custom I/O driver を持つ
- `esp_netif` から見れば、`EspNowIP` は packet 入出力を持つ仮想 interface である
- interface 種別は Ethernet 互換ではなく point-to-point IPv4 link とする
- `EspNowIP` は成立済みの `Bus session` に対して `IP session` 確立を試みる
- 相手ノードは `active gateway` のみとする

### 5.2 gateway 側
- gateway は device ごとに論理 link context を持つ
- 各 context は以下を保持する
  - device MAC
  - link state
  - device に払い出した IPv4 address
  - 再構成中 fragment 情報
  - NAT 転送状態

### 5.3 `Bus session` と `IP session`
- `Bus session` は `EspNowBus` で成立した下位レイヤー接続である
- `IP session` は `EspNowIP` の control plane が成功して利用可能になった上位レイヤー接続である
- `Bus session` が成立していても `IP session` は未成立でありうる
- `EspNowIP` は複数の `Bus session` を候補として持てる
- `EspNowIP` は最初に成功した `IP session` を active にする

### 5.4 接続シーケンス
1. gateway が `EspNowBus` の募集を出す
2. device が募集を受けて `EspNowBus` の接続を行う
3. device に 1 つ以上の `Bus session` ができる
4. `EspNowIP` は `Bus session` ごとに `IpControlHello` と `IpControlLease` の確立を試す
5. 最初に成功した `Bus session` を使って `IP session` を確立する
6. 成功した gateway を `active gateway` にする
7. `IP session` の確立に失敗した `Bus session` は候補として保持したまま、次の `Bus session` を試す

gateway 冗長化時の運用方針:
- 冗長化する場合は 2 台の gateway を用意し、device は利用可能な `Bus session` に対して上記シーケンスを順に試す
- `active gateway` を失った場合、device は他の `Bus session` を使って再接続を試みる
- uplink 側への接続復帰はおおむね 1 分以内を目標とする
- 再接続中は uplink 通信が一時的に途切れることを前提とする
- この挙動はリアルタイム性の高い通信には向かず、再接続を許容できる用途を対象とする

### 5.5 link up/down
- `EspNowBus` JOIN 完了時に `Bus session` 成立とみなす
- `IpControlHello` と `IpControlLease` 完了時に `IP session` を link up とする
- `active gateway` に対応する `Bus session` が `EspNowBus` の auto purge により削除された時点で `IP session` を link down とする
- `EspNowBus` の auto purge 条件は `onJoinEvent(mac, false, false)` で通知される timeout 離脱または `ControlLeave` 受信とする
- `active gateway` を失った場合、device は現在の `IP session` 状態を全て破棄する
- 破棄対象は lease、未完 fragment、`active gateway` 選択状態、`esp_netif` link state とする
- 状態破棄後、device は初回接続時と同じシーケンスに戻り、利用可能な `Bus session` に対して順に `IP session` 確立を再試行する

## 6. アドレスモデル

### 6.1 方針
- IPv4 のみを扱う
- device 側アドレス取得は gateway 独自の lease protocol を使う
- DHCP は使わない
- subnet は `/24` を固定とする
- 通常運用ではアドレス設定を変更しなくても使えることを前提とする
- uplink 側ネットワークとの衝突を避ける必要がある場合のみ、gateway 設定で上位 3 オクテットを変更できるものとする

### 6.2 lease 内容
- device IPv4 address
- gateway IPv4 address
- netmask
- default gateway
- DNS server 1
- DNS server 2
- link MTU
- lease 時間

### 6.3 アドレス構成
- 既定 subnet は `10.201.0.0/24` とする
- gateway の既定アドレスは `10.201.0.1` とする
- device のアドレスは gateway が同一 `/24` 内から一意に lease する
- host 部の割り当て方針は gateway 側で固定とし、device ごとの個別設定は持たない
- subnet を変更する場合は、`10.a.b.0/24` のように上位 3 オクテットのみを変更対象とする
- 通常運用では既定 subnet のまま利用し、uplink 側ネットワークと衝突する場合のみ設定変更する

## 7. IP プロトコル

### 7.1 上位ヘッダ
Bus 上の user payload 先頭には IP 用の上位ヘッダを置く。

```cpp
struct EspNowBusAppHeader {
    uint8_t protocolId;   // 0x02 = IP
    uint8_t protocolVer;  // 1
    uint8_t packetType;
    uint8_t flags;
};
```

### 7.2 パケット種別

```cpp
enum EspNowIpPacketType : uint8_t {
    IpControlHello = 1,
    IpControlLease = 2,
    IpControlKeepAlive = 3,
    IpData = 4,
};
```

### 7.3 `IpControlHello`
用途:
- protocol version 確認
- gateway 存在確認
- MTU / reassembly 能力通知

```cpp
struct IpControlHello {
    uint16_t maxReassemblyBytes;
    uint16_t mtu;
    uint8_t  capabilityFlags;
    uint8_t  reserved;
};
```

### 7.4 `IpControlLease`
用途:
- gateway から device へ IPv4 link 情報を通知

```cpp
struct IpControlLease {
    uint32_t deviceIpv4;
    uint32_t gatewayIpv4;
    uint32_t netmaskIpv4;
    uint32_t dns1Ipv4;
    uint32_t dns2Ipv4;
    uint16_t mtu;
    uint16_t leaseSeconds;
};
```

### 7.5 `IpData`
用途:
- IPv4 packet 本体の搬送

```cpp
struct IpDataHeader {
    uint16_t packetId;
    uint16_t fragmentOffset;
    uint16_t totalLength;
    uint8_t  fragmentIndex;
    uint8_t  fragmentCount;
    uint8_t  reserved0;
    uint8_t  reserved1;
};
```

- `packetId` は再構成単位
- `fragmentOffset` は元 IP packet 内の byte offset
- `totalLength` は元 IP packet 全長
- `fragmentCount` は 1 packet を構成する総 fragment 数

## 8. データモデル

### 8.1 netif 抽象
- 利用者に見えるのは IP packet を送受信できる `esp_netif`
- packet 境界は保持する
- stream 抽象にはしない

### 8.2 フラグメンテーション
- 1 Bus payload に載る IP 生データ量は以下で決まる

```text
ipPayloadMax =
  EspNowBus.Config.maxPayloadBytes
  - EspNowBus header (6 bytes)
  - EspNowBusAppHeader (4 bytes)
  - IpDataHeader (10 bytes)
```

- 現行想定では `ipPayloadMax = Config.maxPayloadBytes - 20`
- `EspNowIP` は `esp_netif` / `lwIP` から渡された IPv4 packet を `ipPayloadMax` 単位で分割して送信する
- 受信側は `packetId` ごとに再構成し、全 fragment が揃ったら 1 packet として `esp_netif_receive()` に渡す
- 250 bytes 上限の古い `ESP-NOW` 環境では fragment 数が大きく増えるため、本仕様の対象外に近い運用とする

### 8.3 MTU
- default MTU は `1420 bytes` とする
- gateway と device は `1420 bytes` を前提に動作する
- MTU negotiation は行わない
- `ESP-NOW` の下位 transport は 1470 bytes 上限を前提に構成する
- 通常運用では IP fragmentation をできるだけ発生させないことを目標とする
- `1420 bytes` を超える IPv4 packet は `IpData` fragment に分割して送信する

### 8.4 順序保証
- `EspNowBus` の「1 in-flight / app-ACK / retry」モデルを前提に、同一 device-gateway 間では fragment 到着順の大きな乱れは起きにくい
- `EspNowIP` は fragment 再構成バッファを持ち、順不同 fragment を受け入れる
- 再構成 timeout を超えた packet は破棄する

## 9. `esp_netif` 接続仕様

### 9.1 custom I/O driver
- `EspNowIP` は `esp_netif` custom I/O driver を実装する
- driver 側は以下を接続する
  - `transmit`
  - `driver_free_rx_buffer`
  - `post_attach`

### 9.2 送信経路
- `lwIP` / `esp_netif` から `transmit()` が呼ばれる
- `EspNowIP` は渡された IPv4 packet を必要なら分割し、`EspNowBus.sendTo()` で gateway に送る
- callback では重い処理を行わず、必要な状態遷移は内部 queue / task で処理する

### 9.3 受信経路
- `EspNowBus` 受信 callback で `IpData` を受け取る
- 必要なら fragment を再構成する
- 完成した packet を `esp_netif_receive()` に渡す

### 9.4 netif 状態
- lease 取得完了までは carrier down 相当で扱う
- `IpControlHello` と `IpControlLease` 完了で link up とする
- `active gateway` に対応する `Bus session` が `EspNowBus` の auto purge で削除された場合は link down とする
- link down 時は `esp_netif` 側へ状態反映する

## 10. gateway 側 forwarding 仕様

### 10.1 forwarding モード
- gateway は `routing + IPv4 NAT` のみを行う
- `routing only` は行わない
- `L2 bridge` は行わない

### 10.2 NAT
- gateway は uplink interface に対して `IPv4 NAPT` を有効化する
- `lwIP` では `IP forwarding` と `IPv4 NAPT` を有効化する
- inbound 側の port forwarding は行わない

### 10.3 gateway の責務
- device 側から見た default gateway になる
- uplink 側 DNS を device に配布する
- gateway 自身の gateway service endpoint を提供できる
- uplink 側疎通が無い場合も local link 状態は保持する

### 10.4 broadcast / multicast
- IPv4 broadcast は転送しない
- ARP は point-to-point link 前提で link 内に閉じる
- multicast は転送しない

## 11. バッファリング

### 11.1 TX
- device ごとに送信中 packet context を持つ
- 送信 queue 長を制限する
- MTU 超過 packet の fragment は同一 packet 単位で順次送信する

### 11.2 RX
- device ごとに fragment 再構成バッファを持つ
- 再構成中 packet 数には上限を設ける
- timeout / overflow 時は packet 単位で破棄する

## 12. 実行モデル

### 12.1 `poll()`
- `EspNowIP::poll()` を `loop()` から呼ぶ
- `poll()` は non-blocking とする
- `poll()` は以下を進める
  - gateway/device state machine
  - 再構成 timeout の掃除
  - lease 更新
  - pending TX の flush

### 12.2 自動補助 poll
- 明示 `poll()` を基本経路とする
- `EspNowIP` の状態参照 API やデータ操作 API は、必要に応じて内部で軽量 poll を呼び出してよい
- 軽量 poll は以下に限定する
  - `active gateway` 喪失確認
  - lease / timeout 状態確認
  - 接続シーケンスの 1 ステップ進行
- fragment 再構成の大きな掃除や複数候補の連続試行など、重い処理は通常の `poll()` で進める
- 自動 poll は補助機構であり、明示 `poll()` の代替とはしない

### 12.3 task 分離
- `ESP-NOW` callback では queue 投入までに留める
- `esp_netif_receive()` 呼び出しや lease/state 更新は通常 task 文脈で行う

## 13. API 仕様

### 13.1 device 側

```cpp
class EspNowIP {
public:
    struct Config {
        const char* groupName = nullptr;
        const char* ifKey = "enip0";
        const char* ifDesc = "ESP-NOW IP";
        bool useEncryption = true;
        bool enablePeerAuth = true;
        bool enableAppAck = true;
        int8_t channel = -1;
        wifi_phy_rate_t phyRate = WIFI_PHY_RATE_11M_L;
        uint16_t maxPayloadBytes = EspNowBus::kMaxPayloadDefault;
        uint16_t mtu = 1420;
        uint16_t maxReassemblyBytes = 1536;
        uint8_t maxReassemblyPackets = 4;
        uint32_t leaseTimeoutMs = 60000;
    };

    bool begin(const Config& cfg);
    void end();
    void poll();

    esp_netif_t* netif();
    bool linkUp() const;
    bool hasLease() const;
};
```

### 13.2 gateway 側

```cpp
class EspNowIPGateway {
public:
    struct Config {
        const char* groupName = nullptr;
        esp_netif_t* uplink = nullptr;
        uint16_t mtu = 1420;
        uint8_t maxDevices = 6;
        uint32_t leaseSeconds = 3600;
        uint8_t subnetOctet1 = 10;
        uint8_t subnetOctet2 = 201;
        uint8_t subnetOctet3 = 0;
    };

    bool begin(const Config& cfg);
    void end();
    void poll();
};
```

## 14. `EspNowBus` との責務境界

### 14.1 `EspNowIP` が再実装しないもの
- gateway candidate 探索
- JOIN
- 暗号化
- app-ACK
- packet 再送
- 物理送信失敗検知
- heartbeat

### 14.2 `EspNowIP` が追加するもの
- `esp_netif` custom driver
- IPv4 packet fragmentation / reassembly
- lease / MTU / keepalive control plane
- gateway 側 NAT forwarding 文脈

## 15. 実装対象
- IPv4 のみ
- device 側 `esp_netif` custom I/O driver
- gateway 側 `routing + IPv4 NAT`
- 独自 lease protocol
- unicast のみ
- 単一 gateway 前提

## 16. 参考資料
- ESP-NETIF Developer's manual  
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_netif_driver.html
- ESP-IDF Programming Guide: ESP-NOW  
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
- ESP-IDF Programming Guide: lwIP  
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-guides/lwip.html
- ESP-IDF Project Configuration: `CONFIG_LWIP_IP_FORWARD` / `CONFIG_LWIP_IPV4_NAPT`  
  https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s2/api-reference/kconfig.html
