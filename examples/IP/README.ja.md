# IP サンプル

`EspNowIP` 向けサンプル一式です。

## サンプル一覧

- [`01_DeviceBasic`](01_DeviceBasic): device 側 `EspNowIP` の最小構成。仮想 IP インターフェイスを起動し、link / lease 状態を表示する土台サンプルです。
- [`02_GatewayWiFiSTA`](02_GatewayWiFiSTA): uplink に Wi-Fi STA を使う gateway 側サンプル。主に bring-up や試験向けの想定です。
- [`03_GatewayW5500Ethernet`](03_GatewayW5500Ethernet): uplink に W5500 Ethernet を使う gateway 側サンプル。有線 uplink を使う実運用寄りの構成を想定しています。
- [`04_GatewayPPPSerial`](04_GatewayPPPSerial): 物理 UART の PPP を使って PC と uplink を張る gateway 側サンプル。USB Serial 接続の host を uplink にする構成を想定しています。
- [`05_DeviceConnectivityCheck`](05_DeviceConnectivityCheck): `gateway ping`、DNS、NTP、HTTP をまとめて確認する device 側疎通診断サンプルです。

## 補足

- 現在のサンプルはビルドできますが、`EspNowIP` の data plane や uplink NAT はまだ最小実装です。
- device 側サンプルは、gateway が `EspNowBus` で募集し、その結果得られた `Bus session` に対して `EspNowIP` が `IP session` を試す前提です。

## uplink の選び方

- gateway 側 uplink は、`esp_netif` として扱えることを前提に選びます。
- まずは以下の 3 種類を対象とします。
  - `Wi-Fi STA`: テストしやすいが、`ESP-NOW` channel が STA 側に引きずられるため実運用向きではありません。
  - `W5500 / Ethernet`: 実運用しやすい有力候補です。
  - `UART PPP`: 手元機材で試しやすい一方、PPP peer 側設定が必要です。
- セルラー系 uplink も `esp_modem` などを使って `esp_netif` に乗せられれば候補になりますが、初期実装としてはハードルが高いため対象外とします。
