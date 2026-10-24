# EspNowBus examples

> 日本語版: [README.ja.md](README.ja.md)

## Workflow and individual READMEs

Start with the [example workflow](../docs/GUIDE_EXAMPLES.md), then choose the relevant sketch.

A useful starting point is [01_Broadcast](01_Broadcast/README.md). Check its required hardware and choose another entry below if it does not match your setup.

| Example | Learn |
|---|---|
| [01_Broadcast](01_Broadcast/README.md) | Send and receive periodic group broadcasts |
| [02_JoinAndUnicast](02_JoinAndUnicast/README.md) | Join a peer and send unicast messages |
| [03_SendToAllPeers](03_SendToAllPeers/README.md) | Send a message to the known peers |
| [04_MasterSlave/Master](04_MasterSlave/Master/README.md) | Run the master side of a paired bus example |
| [04_MasterSlave/Slave](04_MasterSlave/Slave/README.md) | Run the slave side of a paired bus example |
| [05_SendStatusDemo](05_SendStatusDemo/README.md) | Observe queued, retry and completion status |
| [06_NoAppAck](06_NoAppAck/README.md) | Compare sending without application acknowledgements |
| [07_AutoPurge](07_AutoPurge/README.md) | Observe automatic peer removal |
| [08_ChannelOverride](08_ChannelOverride/README.md) | Select a common explicit Wi-Fi channel |
| [09_PhyRateOverride](09_PhyRateOverride/README.md) | Choose the physical radio rate |
| [10_LowFootprintBroadcast](10_LowFootprintBroadcast/README.md) | Use a reduced-resource broadcast configuration |
| [11_FullConfigTemplate](11_FullConfigTemplate/README.md) | Inspect an explicit bus configuration template |
| [12_ExplicitLeave](12_ExplicitLeave/README.md) | Leave and observe peer-state changes |
| [IP/01_DeviceBasic](IP/01_DeviceBasic/README.md) | Bring up an IP endpoint on the bus |
| [IP/02_DeviceConnectivityCheck](IP/02_DeviceConnectivityCheck/README.md) | Inspect device-side IP connectivity |
| [IP/03_GatewayWiFiSTA](IP/03_GatewayWiFiSTA/README.md) | Connect a bus gateway through Wi-Fi STA |
| [IP/04_GatewayEthernet](IP/04_GatewayEthernet/README.md) | Connect a bus gateway through Ethernet |
| [IP/05_GatewayPPPSerial](IP/05_GatewayPPPSerial/README.md) | Use a serial PPP gateway endpoint |
| [IP/06_GatewayPPPModem](IP/06_GatewayPPPModem/README.md) | Connect a gateway with a PPP modem |
| [Serial/01_BasicPair](Serial/01_BasicPair/README.md) | Run a pair of serial bus endpoints |
| [Serial/02_ControllerBridge](Serial/02_ControllerBridge/README.md) | Bridge a controller to a remote serial endpoint |
| [Serial/03_SilentDevice](Serial/03_SilentDevice/README.md) | Observe a device that sends no periodic text |
| [Serial/04_MultiSessionMonitor](Serial/04_MultiSessionMonitor/README.md) | Monitor multiple serial sessions |
| [Serial/05_ESP32SerialCtlDevice](Serial/05_ESP32SerialCtlDevice/README.md) | Expose a serial command device through the bus |
