# Portable phone-service host

The portable host runner is a development harness for exercising the same phone-service runtime from a desktop on a trusted local network. It is not a PS4 executable or a substitute for the eventual Orbis app startup/UI.

## Build

```sh
cmake -S . -B build -DPKGMASTER_BUILD_TESTS=ON -DPKGMASTER_BUILD_HOST_SERVICE=ON
cmake --build build --target pkgmaster_service_host
```

## Run

```sh
./build/pkgmaster_service_host 192.168.1.20 8080 ./pkgmaster-data
```

Use the private IPv4 address actually assigned to the host machine. The phone must be on the same trusted LAN. The runner creates a `downloads/` directory, a persistent `queue.idx` snapshot and a `library.idx` path under the data directory, and writes a one-time `pairing.svg` QR asset. Open the pairing URL on the phone or display the SVG on another screen. Press Ctrl+C for an orderly stop and queue snapshot.

**Security:** this development service uses plaintext HTTP. Run it only on an isolated/trusted LAN, do not port-forward it, and do not share the pairing URL, QR SVG or generated session token. A production PS4 UI should render the QR in-process rather than writing it to a broadly readable desktop directory. The host runner validates the same portable API/service composition but cannot certify Orbis SDK compatibility, firmware 14.00, GoldHEN behavior, or PS4 hardware.
