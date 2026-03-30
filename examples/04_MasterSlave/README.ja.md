# 04_MasterSlave

受信中心の master と、送信中心の slave に役割を分けた 2 スケッチ構成のサンプルです。

## 含まれるスケッチ

- `Master/Master.ino`
- `Slave/Slave.ino`

## 役割

- Master: 登録を受け付け、主に受信を担当
- Slave: 自分では募集せず、既知ピアへ定期送信

1 台に `Master`、1 台以上に `Slave` を書き込んで利用します。
