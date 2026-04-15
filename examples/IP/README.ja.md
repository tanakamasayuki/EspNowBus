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

## 制約と運用上の注意

- `EspNowIP` のネットワーク形状は `device -> gateway -> uplink` のスター型です。gateway が `EspNowBus` で募集し、device はその結果できた `Bus session` の上で `IP session` を張ります。
- device が到達できる先は、基本的に gateway 自身と gateway の先にある uplink 側ネットワークです。device 間の IP 転送は行いません。
- 複数 gateway を同じ環境に置くことはできますが、どの gateway に接続されるかは固定ではありません。device は利用可能な `Bus session` に対して順に `IP session` を試し、先に成立した gateway を使います。
- active gateway の接続性が失われると、`EspNowBus` の timeout / auto purge を契機に `IP session` を破棄し、再び利用可能な gateway 候補を探して再接続します。再接続先は、ちょうどその時点で見えている募集や `Bus session` の状態に依存します。
- そのため、複数 gateway を置くことで冗長化はできますが、どちらに再接続するかは保証されません。無停止切替ではなく、いったん切れてから再接続する前提です。
- `EspNowBus` 側でも再送と app-ack を行い、その上で `TCP` も再送や handshake を行うため、`TCP` はオーバーヘッドが大きくなります。`HTTP` や `MQTT` のような低速で再接続前提の用途には向きますが、低遅延や高スループットを求める用途には向きません。
- デバッグログを多く出すと、特にシリアル出力がボトルネックになり、実効スループットが大きく落ちます。サンプルのログ量は切り分けを優先しているため、そのまま実運用に使う前提ではありません。
- 現在の実装は `EspNowIP` の構成例と PoC を示すことを主目的としています。Wi-Fi STA と Ethernet uplink の基本疎通は確認済みですが、スループット最適化、長時間運転、failover の詳細、fragmentation / reassembly の本格検証はまだ十分ではありません。
- そのため、現状のサンプルや既定設定は「この構成で IP 通信が成立する」ことを示すためのものであり、そのまま実運用に投入するより、用途に合わせたログ削減、再接続設計、トラフィック量の見積もりを行った上で使う前提です。

## uplink の選び方

- gateway 側 uplink は、`esp_netif` として扱えることを前提に選びます。
- `esp_netif` に乗る uplink であれば、`EspNowIPGateway` の bus 側 `NetIF` と同じ ESP32 の IP スタックで扱えます。
- そのため uplink 種別が変わっても、gateway 側では同じ `routing + NAT` の構成を使えます。
- 現時点では以下の 4 種類を対象とします。
- `Wi-Fi STA`: テストしやすいが、`ESP-NOW` channel が STA 側に引きずられるため実運用向きではありません。
- `Ethernet`: 実運用しやすい有力候補です。
- `UART PPP`: 手元機材で試しやすい一方、PPP peer 側設定が必要です。
- `PPP.h` 対応セルラーモデム: `06_GatewayPPPModem` を用意しています。
