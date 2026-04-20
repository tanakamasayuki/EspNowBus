# 01_DeviceBasic

device 側 `EspNowIP` の最小構成サンプルです。

## このサンプルで確認できること

- `EspNowIP` device 初期化の基本形
- 仮想 IP インターフェイスの起動
- link / lease 状態の確認
- 後続の device 側サンプルの土台構成

## 想定用途

最初に device 側の `Bus session -> IP session` 確立と lease 適用を確認するためのベースに使います。

## 補足

- `EspNowBus` は既定では `groupName` から channel を自動設定します。通常はこの既定のまま使い、環境によって必要な場合のみ全端末で同じ channel を明示設定します。
- `03_GatewayWiFiSTA` と組み合わせて使う場合は、gateway 側が表示する STA channel に device 側 `EspNowBus` channel を合わせる必要があります。
- このサンプルは device 側からは募集せず、`cfg.autoJoinIntervalMs = 0` にして gateway 側の募集へ乗る前提です。
- 設定例（通常はコメントアウトのままで使う想定）:

```cpp
// EspNowIP::Config cfg;
// cfg.groupName = "espnow-ip-demo";
// cfg.autoJoinIntervalMs = 0;      // Device stays silent; gateway advertises
// cfg.channel = 6;                  // Optional
// cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional
```
