# EspNowBus サンプル

> English: [README.md](README.md)

## 実行ガイドと個別README

最初に[サンプル入門ガイド](../docs/GUIDE_EXAMPLES.ja.md)を読み、目的に合う例へ進んでください。

最初の候補は[01_Broadcast](01_Broadcast/README.ja.md)です。必要な機器を確認し、対象が異なる場合は下の一覧から選び直してください。

| 例 | 学ぶこと |
|---|---|
| [01_Broadcast](01_Broadcast/README.ja.md) | グループのBroadcastを周期的に送受信する |
| [02_JoinAndUnicast](02_JoinAndUnicast/README.ja.md) | 相手へJoinしてUnicastを送る |
| [03_SendToAllPeers](03_SendToAllPeers/README.ja.md) | 登録された相手へメッセージを送る |
| [04_MasterSlave/Master](04_MasterSlave/Master/README.ja.md) | 組になるBus例のMaster側を動かす |
| [04_MasterSlave/Slave](04_MasterSlave/Slave/README.ja.md) | 組になるBus例のSlave側を動かす |
| [05_SendStatusDemo](05_SendStatusDemo/README.ja.md) | Queue受理・再送・完了の状態を確認する |
| [06_NoAppAck](06_NoAppAck/README.ja.md) | Application ACKなしの送信を確認する |
| [07_AutoPurge](07_AutoPurge/README.ja.md) | 相手の自動削除を確認する |
| [08_ChannelOverride](08_ChannelOverride/README.ja.md) | 共通のWi-Fi Channelを明示する |
| [09_PhyRateOverride](09_PhyRateOverride/README.ja.md) | 無線のPHY Rateを指定する |
| [10_LowFootprintBroadcast](10_LowFootprintBroadcast/README.ja.md) | 資源を抑えたBroadcast設定を使う |
| [11_FullConfigTemplate](11_FullConfigTemplate/README.ja.md) | Bus設定を明示したTemplateを確認する |
| [12_ExplicitLeave](12_ExplicitLeave/README.ja.md) | 明示的にLeaveして相手状態の変化を見る |
| [IP/01_DeviceBasic](IP/01_DeviceBasic/README.ja.md) | Bus上のIP Endpointを開始する |
| [IP/02_DeviceConnectivityCheck](IP/02_DeviceConnectivityCheck/README.ja.md) | Device側のIP疎通を確認する |
| [IP/03_GatewayWiFiSTA](IP/03_GatewayWiFiSTA/README.ja.md) | Wi-Fi STAでBus Gatewayを接続する |
| [IP/04_GatewayEthernet](IP/04_GatewayEthernet/README.ja.md) | EthernetでBus Gatewayを接続する |
| [IP/05_GatewayPPPSerial](IP/05_GatewayPPPSerial/README.ja.md) | Serial PPPのGateway Endpointを使う |
| [IP/06_GatewayPPPModem](IP/06_GatewayPPPModem/README.ja.md) | PPP ModemでGatewayを接続する |
| [Serial/01_BasicPair](Serial/01_BasicPair/README.ja.md) | Serial Bus Endpointの組を動かす |
| [Serial/02_ControllerBridge](Serial/02_ControllerBridge/README.ja.md) | Controllerから遠隔Serial Endpointへ渡す |
| [Serial/03_SilentDevice](Serial/03_SilentDevice/README.ja.md) | 定期文字列を送らないDeviceを確認する |
| [Serial/04_MultiSessionMonitor](Serial/04_MultiSessionMonitor/README.ja.md) | 複数のSerial Sessionを監視する |
| [Serial/05_ESP32SerialCtlDevice](Serial/05_ESP32SerialCtlDevice/README.ja.md) | Serial Command DeviceをBus越しに使う |
