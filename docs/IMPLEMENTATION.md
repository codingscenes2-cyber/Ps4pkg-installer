# Implementation notes

## Queue persistence

QueueStore writes a versioned, tab-delimited snapshot. String fields are hex encoded so tabs, newlines and arbitrary bytes cannot corrupt record boundaries. Loading is transactional: it parses into a temporary vector, validates IDs, states and byte counters, and only replaces caller state after the complete file succeeds. Saving writes a sibling temporary file and renames it over the target on the same filesystem.

This stores queue metadata only. It does not yet checkpoint partial download files, ETags, Last-Modified values, per-range completion maps, or fsync the containing directory. Those are required before claiming crash-safe download resume. Callers must choose a writable private data path and serialize concurrent save operations.



## Range file assembly

The range assembler creates a bounded `.part` staging file from a validated contiguous range plan. It accepts out-of-order chunk writes, checks each chunk's exact assigned length, serializes file access, flushes before marking a chunk complete, and publishes only after every range is present. It refuses to truncate an existing staging file so a future recovery implementation can inspect it. The current flush is a C++ stream flush, not an fsync or power-loss durability guarantee.

This is a storage primitive, not a parallel HTTP downloader. It currently has no API to reopen and reconcile an existing partial file with a resume manifest, and chunk writes are serialized through one file stream. Do not claim interrupted downloads can yet be resumed automatically.

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


## HTTP resource capability probe

The HTTP probe performs a HEAD request and then a one-byte GET with Range: bytes=0-0. It accepts range support only when the server returns HTTP 206, an exact Content-Range for the discovered object length, and exactly one body byte. Redirects are rejected rather than followed. The probe reports resumability only when a syntactically strong ETag is available; when present, If-Match is sent on the validation request to prevent silently probing a changed object. This is capability discovery only: it does not yet download parallel ranges, persist checkpoints, or resume transfers.


## Parallel HTTP range transport

A bounded worker-pool transport now probes the object before starting and requires a validated byte-range response plus a strong ETag. It partitions the object into at most eight ranges, sends If-Match on every request, rejects redirects, validates HTTP 206 and exact Content-Range/body length, requires the response ETag to match the probed identity, and writes each validated part through RangeFileAssembler. Cancellation is cooperative between requests only: in-flight curl requests do not yet have a progress abort callback. Failures/cancellation leave the private .part staging file in place, but no manifest is yet connected to this transport, so automatic restart recovery is not available and a later run will refuse to overwrite that staging file. The current worker pool uses separate libcurl easy handles and threads; it is not a libcurl multi event loop. No live-origin integration test or PS4 build has been performed.


## Connected range resume checkpointing

The parallel downloader now creates a versioned resume manifest beside the .part staging file before scheduling ranges. It records the exact source URL, strong ETag, object length, fixed range partition and completion flags. On restart, both files must exist; the downloader re-probes the source and requires the same strong ETag and length before reopening the staging file. The assembler checks the staging file's exact length, restores the completion map, and only schedules incomplete ranges. Each completed range is flushed by the assembler before its manifest flag is saved. A cancellation or failed transfer retains both files for a later attempt. After all ranges complete, the file is atomically published and the manifest is removed.

Durability limitation: stream flush and rename are not fsync guarantees, so sudden power loss can still lose recently acknowledged data or metadata. The current manifest does not contain per-range cryptographic hashes; it relies on the strong remote ETag and successful local write/flush. A local corruption audit and final package checksum/signature verification remain future work. Resume also depends on the origin preserving the same strong ETag across requests.


## Resume chunk integrity

Resume manifest version 2 stores a CRC32 for each completed range. On restart, PKGMaster verifies completed ranges against the checksum before trusting them. A completed range with a missing checksum (including a version-1 legacy checkpoint) or a checksum mismatch is invalidated and scheduled for re-download; it is never silently treated as valid. CRC32 detects accidental staging-file corruption but is not a cryptographic authenticity mechanism. The strong ETag still binds the range requests to one remote object version, and a trusted package hash/signature check remains a separate future requirement.


## Download telemetry and bandwidth controls

HTTP download options now expose a live metrics callback with received/total bytes, percent, smoothed bytes-per-second and ETA when enough samples exist. The speed tracker uses monotonic elapsed time, handles counter resets and avoids division by zero. A maximum receive-rate option can cap the single-stream curl transfer. Parallel mode divides the configured cap across workers and applies a per-connection libcurl receive limit; this is an approximate aggregate cap, not a strict shared token-bucket limiter. Zero leaves bandwidth unlimited.


## Queue download coordinator

DownloadCoordinator connects queue jobs to the HTTP engines. It moves jobs through Resolving, Downloading, Verifying and ReadyToInstall, persists byte progress through Queue updates, selects parallel range mode only when the probe confirms range support and a strong ETag, and otherwise uses the single-stream HTTP path. It owns the active cancellation flag and exposes pause/cancel/resume controls. Pause is offered only while a resumable parallel transfer is active; single-stream transfers do not claim pause/resume support. The coordinator is synchronous and must be invoked on a worker thread by the future UI. ReadyToInstall means the file is downloaded and ready for a separately validated native installer handoff; it does not mean installation occurred.


## Trusted package SHA-256 verification

Callers may optionally provide a trusted 64-character hexadecimal SHA-256 digest in HttpDownloadOptions. Both single-stream and parallel transfers compute SHA-256 over the completed staging file and refuse to publish it if the digest differs. The parallel resume manifest still uses per-range CRC32 for efficient local recovery; the final SHA-256 is a separate whole-file integrity check. No digest is guessed or fetched from an untrusted mirror: the caller must obtain the expected value from a trusted source. If no expected digest is supplied, the downloader performs the existing length and range checks but cannot claim cryptographic authenticity.


## Safe archive extraction

An optional libarchive-backed extractor now handles supported archive formats when libarchive is present in the build. It extracts into a new staging directory, rejects absolute paths, parent/current/empty path components, backslashes, drive-like paths, symlinks, hardlinks and special files, and enforces file-count, per-file, total-size and expansion-ratio limits. Passwords are supplied explicitly by the caller; no password guessing is performed. PKG detection is extension-based after extraction and is not package authenticity validation. If libarchive is absent, the feature reports Unsupported. The host CI installs libarchive; a compatible PS4 port and real-console extraction test are still required before enabling this capability on PS4. Multi-volume archive behavior is dependent on the linked libarchive build and has not been validated.


## Parallel download telemetry

Parallel range downloads now emit the same DownloadMetrics callback used by the single-stream transport. The tracker receives aggregate completed bytes across all workers, reports percent and smoothed throughput, and exposes ETA only after a positive speed sample exists. The callback is serialized so UI consumers do not need to handle concurrent invocations. A configured bandwidth cap remains an approximate per-worker allocation rather than a strict aggregate token bucket.


## PARAM.SFO metadata parsing

A bounded portable parser reads PSF/PARAM.SFO tables and extracts the common package fields TITLE, TITLE_ID, APP_VER, CATEGORY and SYSTEM_VER. It validates the PSF magic, table offsets, entry count, key termination, string lengths and data bounds before publishing parsed metadata. The parser does not validate package signatures, installability, minimum-firmware policy, or the authenticity of metadata. It is a library building block; recursive package discovery, installed-title reconciliation and UI integration remain future work.
