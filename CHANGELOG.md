# Changelog

User-facing changes per release. The release workflow publishes the matching
section as the GitHub release notes — every tagged version needs a section
here (in addition to the three version manifests) before tagging.

## v0.5.1 (2026-08-06)

- Fixes: encryption counter handling, service UUID, AD length, version
  reporting; CTest coverage gaps closed.

## v0.5.0 (2026-07-30)

- New `build_encrypted_service_data()` — encrypted BTHome payloads for
  transports that are not BLE (used by esphome-rf24-remote's nRF24 senders).

## v0.4.2 (2026-07-26)

- AVR compile gate in CI, third library manifest (`idf_component.yml`),
  two documentation fixes.

## v0.4.1 (2026-07-25)

- `lock()`'s parameter is named after the byte it produces.

## v0.4.0 (2026-07-25)

- New `BTHome::Decoder` — parse received BTHome service data.
- New `BTHome::Decryptor` — verify and decrypt received encrypted payloads.
- Object payload widths derive from a single `object_layout()` table.
- Non-finite sensor values (NaN, ±inf) are dropped instead of encoded.
- API change: the five object-id enums are merged into one `ObjectId`.

## v0.3.2 (2026-07-24)

- Fix: `dimmer_event(None)` encodes without the steps byte (`3C 00` instead
  of `3C 00 00`).

## v0.3.1 (2026-07-24)

- Published to the PlatformIO registry, with auto-publish on release and a
  weekly registry canary.
- Interop tests against bthome-ble (Home Assistant's parser).
- Toolchains without libstdc++ headers (avr-gcc) are supported.
- CI gate keeps the version manifests and release tags in sync.

## v0.3.0 (2026-07-22)

**BTHome events** — the spec's event table is now fully covered:

- `BTHome::button_event(ButtonEventType)` — all eight codes (`press` … `long_triple_press`, `hold_press`), multi-button devices via sequential entries padded with `None` (spec example `3A 00 3A 01`).
- `BTHome::command_event(CommandEventType, steps)` — `Off`/`On`/`Toggle`/`StepUp`/`StepDown` in the variable `[argc][opcode][args]` layout. Per spec advice, send commands only in encrypted advertisements.
- `BTHome::dimmer_event(DimmerEventType, steps)` — rotate events carry the step count; `None` stays a single byte.

**Lean release archives** — source ZIPs/tarballs of this and future tags contain only `src/`, `examples/` and the library metadata (no tests/CI/devcontainer).

All factories remain covered by spec-derived golden-byte tests (62 scalar, 28 binary, 16 event cases + completeness guard); CI green across g++/clang++ and the full 9-job examples matrix (Arduino, PlatformIO, ESP-IDF, Zephyr, nRF Connect SDK).

## v0.2.0 (2026-07-21)

- **AES-CCM encryption** (BTHome v2 spec): `BTHome::Encryptor` (counter-owning — CCM nonce reuse is structurally impossible), `BTHome::EncryptedPacket<N>` (reserves the 8-byte counter+MIC overhead in the type), `BTHome::build_encrypted_advertising(...)`.
- **Two pluggable crypto backends**, both reproducing the official bthome.io test vector byte-for-byte: `bthome_crypto_mbedtls.h` (ESP-IDF, Arduino, desktop) and `bthome_crypto_psa.h` (Zephyr, nRF Connect SDK/Oberon/CryptoCell, TF-M).
- **Encrypted examples for every framework**: generic, ESP-IDF (NVS counter persistence), Arduino NimBLE (Preferences counter), Zephyr (PSA backend, MAC byte-order handling).
- **Examples CI**: all eight examples compile-checked against their real frameworks on every change — arduino-cli, PlatformIO (pioarduino), ESP-IDF v5.4, Zephyr 4.1 and nRF Connect SDK v3.2.0.
- Portability hardening: headers use `<string.h>` with unqualified libc calls (Zephyr's minimal C++ library has no `<cstring>`), verified across all supported toolchains.
- Dev container matching the CI environment.

## v0.1.0 (2026-07-20)

Initial version (tagged, no GitHub release): header-only C++ builder for
BTHome v2 advertising payloads — scalar and binary object factories with
spec-derived golden-byte tests, usable from Arduino, PlatformIO, ESP-IDF
and Zephyr.
