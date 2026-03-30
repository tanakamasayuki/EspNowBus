# 01_Broadcast

Minimal periodic broadcast example for `EspNowBus`.

## What It Shows

- Basic `EspNowBus::begin()`
- Broadcast send with `bus.broadcast(...)`
- Receive callback logging on every node

## How to Use

1. Flash the same sketch to two or more ESP32 boards.
2. Open serial monitors on the boards.
3. Wait for periodic `hello broadcast` messages.

Nodes using the same `groupName` can hear each other's broadcast traffic.
