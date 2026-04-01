# 02_GatewayWiFiSTA

uplink に Wi-Fi STA を使う gateway 側 `EspNowIP` サンプル予定です。

## このサンプルで確認できること

- `EspNowIPGateway` 初期化の基本形
- Wi-Fi STA uplink の準備
- `routing + IPv4 NAT` gateway の基本構成
- STA uplink が使っている channel の表示

## 制約

- このサンプルは試験しやすい一方で、実運用の本命 uplink としては扱いにくいです。
- `EspNowBus` は既定では `groupName` から channel を自動設定します。通常はその既定のまま使いますが、このサンプルでは実効 channel が STA uplink 側に引きずられるため、必要に応じて全端末で同じ channel を明示設定します。
- `ESP-NOW` と `Wi-Fi STA` を併用する場合、実効的な `ESP-NOW` channel は接続中の STA channel に制約されます。
- AP の channel が変わる、roaming する、複数 AP で channel が異なる、といった状況では device 側 channel が揃わず正常に link できなくなる可能性があります。

## 想定用途

`EspNowIP` の初期実装で、gateway 側の bring-up や試験に使うサンプルです。

## テスト手順

1. gateway 側に Wi-Fi STA の SSID / パスワードを設定します。
2. gateway を起動し、シリアルログに表示される STA channel を確認します。
3. device 側 `EspNowBus` の channel を、その表示された channel と同じ値に設定します。
4. device 側サンプルを起動し、`Bus session`、続いて `IP session` が張れることを確認します。

設定例（通常はコメントアウトのままで使う想定）:

```cpp
// EspNowIPGateway::Config cfg;
// cfg.groupName = "espnow-ip-demo";
// cfg.channel = 6;                  // Optional
// cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional
```
