# Implementation notes

## Queue persistence

QueueStore writes a versioned, tab-delimited snapshot. String fields are hex encoded so tabs, newlines and arbitrary bytes cannot corrupt record boundaries. Loading is transactional: it parses into a temporary vector, validates IDs, states and byte counters, and only replaces caller state after the complete file succeeds. Saving writes a sibling temporary file and renames it over the target on the same filesystem.

This stores queue metadata only. It does not yet checkpoint partial download files, ETags, Last-Modified values, per-range completion maps, or fsync the containing directory. Those are required before claiming crash-safe download resume. Callers must choose a writable private data path and serialize concurrent save operations.


## Resume manifest milestone

A version-1 resume manifest now records the source URL, entity tag, total object length and a contiguous partition of byte ranges with per-range completion flags. The loader parses transactionally and rejects malformed numbers, gaps, overlaps, out-of-bounds ranges and excessive chunk counts. Saving uses a sibling temporary file followed by rename.

This is deliberately only a validated checkpoint format. The HTTP downloader does not yet populate it, write independent ranges into a shared partial file, revalidate ETags with If-Range, or recover incomplete chunks after process restart. Do not present resume as operational until those pieces and end-to-end interruption tests exist. Manifest files should be treated as untrusted input and kept in the app's private data directory.

## Native installer boundary

The project intentionally defines InstallerBridge as an adapter. The current implementation returns Unsupported. Before implementing it, identify a documented and firmware-compatible handoff mechanism, confirm its required privileges/payload state, and test the acceptance/error callbacks on a real console. Do not call private or guessed symbols and do not claim a queue entry is installing without authoritative status.

## Downloader design requirements

The transport milestone should:
- probe HEAD / byte-range support and fall back to a single stream if range requests are unsupported;
- use bounded workers and bounded buffers;
- persist ETag/Last-Modified, total length, chunk map and destination path atomically;
- validate Content-Range for every part and revalidate metadata on resume;
- write to a temporary .part file and rename only after complete length/hash verification;
- handle redirect loops and re-run URL/IP policy on each redirect;
- support pause/cancel with cooperative worker shutdown.

Do not assume more connections are always faster. Make connection count configurable and cap it conservatively for PS4 CPU, memory and server limits.

## Phone pairing service

The service should bind to the LAN interface only after the user enables it. Generate a cryptographically random one-time pairing secret, display it as a QR URL, expire it quickly, exchange it for a session token, and require that token for every mutating request. Never put a permanent token in a QR code or logs. Add CSRF/origin protections and request-size limits.

## Archive handling

Use a maintained archive library only after confirming its PS4 port and license. Extract into a dedicated staging directory. Reject absolute paths, .., symlinks, hardlinks and suspicious expansion ratios. Require the user to provide a password; do not ship a password-cracking dictionary.

## Firmware validation

A package's SFO minimum system version, the app's target firmware and GoldHEN payload support are separate concerns. Record exact console model, firmware, payload release and test result for every hardware validation run.
