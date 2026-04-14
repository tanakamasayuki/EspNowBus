# IP サンプル

`EspNowIP` 向けサンプル一式です。

## サンプル一覧

- [`01_DeviceBasic`](01_DeviceBasic): device 側 `EspNowIP` の最小構成。仮想 IP インターフェイスを起動し、link / lease 状態を表示する土台サンプルです。
- [`02_DeviceConnectivityCheck`](02_DeviceConnectivityCheck): `gateway ping`、DNS、NTP、HTTP をまとめて確認する device 側疎通診断サンプルです。
- [`03_GatewayWiFiSTA`](03_GatewayWiFiSTA): uplink に Wi-Fi STA を使う gateway 側サンプル。主に試験や検証向けです。
- [`04_GatewayEthernet`](04_GatewayEthernet): uplink に有線 Ethernet を使う gateway 側サンプル。現時点の標準機候補は `LilyGo T-Internet-COM` です。
- [`05_GatewayPPPSerial`](05_GatewayPPPSerial): 物理 UART の PPP を使って PC と uplink を張る gateway 側サンプル。USB Serial 接続の host を uplink にする構成を想定しています。
- [`06_GatewayPPPModem`](06_GatewayPPPModem): `PPP.h` を使うセルラーモデム向け PPP uplink gateway 側サンプルです。

## 補足

- 現在のサンプルはビルドでき、Wi-Fi STA と Ethernet uplink 経路は PoC として確認済みです。
- device 側サンプルは、gateway が `EspNowBus` で募集し、その結果得られた `Bus session` に対して `EspNowIP` が `IP session` を試す前提です。

## どうやって IP が成立するか

- `EspNowIP` は `EspNowBus` の上に載る IP レイヤで、device 側では `esp_netif` custom I/O driver を使った仮想 `NetIF` として動きます。
- つまり device 側の `lwIP` から見ると、`EspNowIP` は通常のネットワークインターフェイスの 1 つとして見え、ARP、ICMP、UDP、TCP などの packet をそのまま受け取って送受信できます。
- device は、まず `EspNowBus` の `Bus session` を作り、その上で `Hello` / `Lease` によって `IP session` を確立します。lease が適用されると、device 側の仮想 `NetIF` に IPv4、gateway、DNS が設定されます。
- gateway 側でも `EspNowIPGateway` が bus 側の `esp_netif` を持ち、device から受けた `IpData` をその bus 側 `NetIF` に流し込みます。device から見ると `10.x.x.1` のような gateway に packet を送っている形になります。
- gateway はさらに uplink 側にも `esp_netif` を持ちます。Wi-Fi STA、Ethernet、PPP など、`esp_netif` として扱える uplink であれば、bus 側と uplink 側を同じ ESP32 の IP スタック上でつなげられます。
- この構成により、gateway は bus 側を内側インターフェイス、uplink 側を外側インターフェイスとして `routing + NAT` を行えます。`EspNowIP` が実現できる理由は、ESP-NOW 自体を Ethernet bridge として透過延長しているのではなく、ESP32 上で仮想 `NetIF` を作って L3 ルータとして扱っているためです。
- そのため本方式は `L2 bridge` ではなく `L3 gateway` です。ARP や ICMP は device と gateway の間で完結し、外向き通信は gateway で uplink 側へルーティングされ、必要に応じて NAT されます。
- uplink が `esp_netif` で統合されていれば、socket、DNS、routing、NAT の経路を共通化できます。一方で、各 uplink の packet は ESP32 上の `lwIP` を通るため、コピーや制御処理のオーバーヘッドは増えます。
- 逆に、モデムや通信チップの独自 IP スタックを直接使えば ESP32 の負荷は下がることがありますが、その場合は `esp_netif` として統合されないことが多く、`EspNowIPGateway` のように複数 IF を同じ IP スタックでつなぐ構成には向きません。

## uplink の選び方

- gateway 側 uplink は、`esp_netif` として扱えることを前提に選びます。
- `esp_netif` に乗る uplink であれば、`EspNowIPGateway` の bus 側 `NetIF` と同じ ESP32 の IP スタックで扱えます。
- そのため uplink 種別が変わっても、gateway 側では同じ `routing + NAT` の構成を使えます。
- 現時点では以下の 4 種類を対象とします。
- `Wi-Fi STA`: テストしやすいが、`ESP-NOW` channel が STA 側に引きずられるため実運用向きではありません。
- `Ethernet`: 実運用しやすい有力候補です。
- `UART PPP`: 手元機材で試しやすい一方、PPP peer 側設定が必要です。
- `PPP.h` 対応セルラーモデム: `06_GatewayPPPModem` を用意しています。
