# 04_GatewayEthernet

有線 Ethernet を uplink に使う gateway 側 `EspNowIP` サンプルです。

現時点の標準機候補は `LilyGo T-Internet-COM` です。`ETH.h` と `LAN8720 RMII` 構成を前提にしています。

## このサンプルで確認できること

- `ETH.h` で取得した Ethernet `esp_netif` を gateway uplink に渡す構成
- 有線 uplink でも同じ `routing + IPv4 NAT` モデルを使う構成
- `Wi-Fi STA` よりチャネル制約を避けやすい gateway 例

## 想定用途

有線 Ethernet uplink を使える環境で、より実運用向きの gateway サンプルとして使います。

## 補足

- 現行の標準機候補は `T-Internet-COM` です
- `T-Internet-COM` では `M5_Ethernet.h` ではなく Arduino ESP32 標準の `ETH.h` を使い、`ETH.netif()` を `EspNowIPGateway` に渡す方針です
- `02_DeviceConnectivityCheck` と組み合わせることで、Ethernet uplink 経路の疎通確認ができます
