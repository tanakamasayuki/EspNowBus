# 04_GatewayPPPSerial

物理 UART PPP を uplink に使う gateway 側 `EspNowIP` サンプル予定です。

## このサンプルで確認できること

- gateway 側 uplink としての PPP over UART
- USB Serial で接続された host PC を PPP peer にする構成
- 他の uplink 種別と同じ NAT モデルを流用する gateway 例

## 想定用途

Wi-Fi uplink を使わない、または使いにくい環境での代替 gateway サンプルとして使います。

