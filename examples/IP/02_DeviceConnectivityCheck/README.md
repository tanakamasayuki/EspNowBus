# 02_DeviceConnectivityCheck

Device-side connectivity diagnostics example for `EspNowIP`.

## What It Shows

- ICMP ping to the `gateway`
- DNS resolution
- UDP communication to NTP
- TCP communication via HTTP GET

## Intended Use

Use this after `01_DeviceBasic` has obtained a lease, in order to check how far real IP connectivity works.

## Notes

- The targets are fixed in the sketch.
- The intended workflow is to copy this example into `examples/IP/temp/` before editing it for local testing.
- `gateway ping` uses `10.201.0.1`.
- Outbound checks use `example.com`, `pool.ntp.org`, and `http://example.com/`.
