# IP サンプル

`EspNowIP` 向けサンプル一式です。

## サンプル一覧

- [`01_DeviceBasic`](01_DeviceBasic): device 側 `EspNowIP` の最小構成。仮想 IP インターフェイスを起動し、link / lease 状態を表示する土台サンプルです。
- [`02_DeviceConnectivityCheck`](02_DeviceConnectivityCheck): `gateway ping`、DNS、NTP、HTTP をまとめて確認する device 側疎通診断サンプルです。
- [`03_GatewayWiFiSTA`](03_GatewayWiFiSTA): uplink に Wi-Fi STA を使う gateway 側サンプル。主に試験や検証向けです。
- [`04_GatewayEthernet`](04_GatewayEthernet): uplink に有線 Ethernet を使う gateway 側サンプル。現時点の標準機候補は `LilyGo T-Internet-COM` です。
- [`05_GatewayPPPSerial`](05_GatewayPPPSerial): 物理 UART の PPP を使って PC と uplink を張る gateway 側サンプル。USB Serial 接続の host を uplink にする構成を想定しています。

## 補足

- 現在のサンプルはビルドでき、Wi-Fi STA と Ethernet uplink 経路は PoC として確認済みです。
- device 側サンプルは、gateway が `EspNowBus` で募集し、その結果得られた `Bus session` に対して `EspNowIP` が `IP session` を試す前提です。

## uplink の選び方

- gateway 側 uplink は、`esp_netif` として扱えることを前提に選びます。
- まずは以下の 3 種類を対象とします。
  - `Wi-Fi STA`: テストしやすいが、`ESP-NOW` channel が STA 側に引きずられるため実運用向きではありません。
  - `Ethernet`: 実運用しやすい有力候補です。
  - `UART PPP`: 手元機材で試しやすい一方、PPP peer 側設定が必要です。
- セルラー系 uplink も `esp_modem` などを使って `esp_netif` に乗せられれば候補になりますが、初期実装としてはハードルが高いため対象外とします。
