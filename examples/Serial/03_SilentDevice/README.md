# 03_SilentDevice

Device-side example for use with an advertising controller.

## What It Does

- Does not advertise on its own
- Binds to the first available controller-side session
- Periodically emits a short status line
- Echoes inbound bytes back to the controller

## Typical Pairing

Use this sketch on the device side together with `02_ControllerBridge` on the controller side.
