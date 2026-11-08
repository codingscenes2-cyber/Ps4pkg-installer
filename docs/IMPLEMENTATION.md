# Implementation notes

## Native installer boundary

The project intentionally defines `InstallerBridge` as an adapter. The current implementation returns `Unsupported`. Before implementing it, identify a documented and firmware-compatible handoff mechanism, confirm its required privileges/payload state, and test the acceptance/error callbacks on a real console. Do not call private or guessed symbols and do not claim that a queue entry is installing without authoritative status.

## Downloader design requirements

The transport milestone should:
- probe HEAD / byte-range support and fall back to a single stream if range requests are unsupported;
- use bounded workers and bounded buffers;
- persist ETag/Last-Modified, total length, chunk map and destination path atomically;
- validate Content-Range for every part and revalidate metadata on resume;
- write to a temporary `.part` file and rename only after complete length/hash verification;
- handle redirect loops and re-run URL/IP policy on each redirect;
- support pause/cancel with cooperative worker shutdown.

Do not assume more connections are always faster. Make connection count configurable and cap it conservatively for PS4 CPU, memory and server limits.

## Phone pairing service

The service should bind to the LAN interface only after the user enables it. Generate a cryptographically random one-time pairing secret, display it as a QR URL, expire it quickly, exchange it for a session token, and require that token for every mutating request. Never put a permanent token in a QR code or logs. Add CSRF/origin protections and request-size limits.

## Archive handling

Use a maintained archive library only after confirming its PS4 port and license. Extract into a dedicated staging directory. Reject absolute paths, `..`, symlinks, hardlinks and suspicious expansion ratios. Require the user to provide a password; do not ship a password-cracking dictionary.

## Firmware validation

A package's SFO minimum system version, the app's target firmware and GoldHEN payload support are separate concerns. Record exact console model, firmware, payload release and test result for every hardware validation run.
