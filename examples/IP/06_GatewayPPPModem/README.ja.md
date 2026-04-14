# 06_GatewayPPPModem

AT コマンドに対応したセルラーモデムを使う PPP uplink gateway 側 `EspNowIP` サンプルです。

## このサンプルで確認できること

- Arduino ESP32 の `PPP.h` を使うモデム uplink 構成
- `PPP.netif()` を `EspNowIPGateway` の uplink に渡す構成
- APN / PIN / UART pin / flow control / modem model を使う AT モデム前提の PPP 例

## 想定用途

SIM7600、SIM7070、SIM7000、BG96、SIM800 など、`PPP.h` が想定する AT モデムを uplink に使いたい場合に使います。

## 補足

- このサンプルは `05_GatewayPPPSerial` と違って host PC raw PPP ではありません
- `PPP.h` は `esp_modem` 前提なので、AT コマンドと PPP をサポートするセルラーモデム用途向けです
- セルラーモデムには HTTP、MQTT、TCP などを個別の AT コマンドで扱えるものもありますが、このサンプルではそれらの個別 API ではなく PPP を使います
- PPP を使うことで uplink を `NetIF` として扱え、`EspNowIPGateway` の routing / NAT / DNS / socket 利用を Wi-Fi STA や Ethernet と同じ形に揃えられます
- 一方で PPP framing や制御手順のぶんだけオーバーヘッドが増えるため、AT コマンド直叩きより効率面で不利になる場合があります
- `PPP_MODEM_APN`、`PPP_MODEM_PIN`、UART pin、flow control、modem model は実機に合わせて調整してください
- `PPP.netif()` が得られれば、その先の NAT / routing は Wi-Fi STA や Ethernet uplink と同じです
