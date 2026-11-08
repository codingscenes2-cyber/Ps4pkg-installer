# Runtime compatibility strategy

PKGMaster uses capability-based compatibility rather than a hard-coded GoldHEN version allow-list.

## Rules
1. The portable core assumes no specific firmware, GoldHEN release, console model, or exploit host.
2. Platform adapters probe only verified runtime interfaces and report capabilities they actually initialized.
3. Features are enabled only when every required capability is reported. Unknown capabilities disable that feature safely with a visible explanation.
4. Firmware and GoldHEN strings are diagnostic metadata, not proof that an API is present.
5. Never infer support from a firmware number alone. Record compatibility only after build and hardware tests.
6. Package-install handoff stays disabled until a supported installer integration is identified and validated.
7. Unknown versions may launch portable features, while unsupported platform features remain unavailable.

## Capability groups
- FileSystem: local package/library access
- NetworkClient: HTTP downloads
- NetworkServer: phone-to-console service
- ControllerInput and NativeUi: PS4 interface
- PackageInstallHandoff: native package installer integration
- UsbStorage: USB discovery
- ArchiveExtraction: archive adapter
- QrGeneration: pairing QR display

## Test reporting
Record exact firmware, GoldHEN version, console model, boot method, and initialized capabilities for every hardware test. Portable CI is not a PS4 compatibility test.

Current status: portable capability gate implemented; PS4 runtime probing and platform adapters pending.
