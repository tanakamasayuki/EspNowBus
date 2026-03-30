# 02_ControllerBridge

Controller-side transparent USB bridge for `EspNowSerial`.

## What It Does

- Bridges USB Serial to one selected `EspNowSerial` session
- Auto-selects session `0` when it becomes available
- Intercepts only local commands prefixed with `///`

## Local Commands

- `///list`
- `///use <index>`
- `///help`

Any line that does not start with `///` is forwarded to the selected remote session.
