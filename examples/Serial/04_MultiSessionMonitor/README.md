# 04_MultiSessionMonitor

Controller-side monitor that scans all sessions and forwards any readable data to USB Serial.

## What It Shows

- `sessionCapacity()`
- `sessionInUse()`
- `sessionConnected()`
- `sessionAvailable()`
- `bindSession(index)`

Use this example when you want to inspect traffic from multiple sessions without manually switching them one by one.

## Typical Pairing

This sketch pairs well with one or more boards running `03_SilentDevice`, because those devices periodically emit status lines and echo input back to the controller.
