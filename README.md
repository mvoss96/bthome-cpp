# BTHome-cpp

Dependency-free C++17 BTHome v2 payload builder.

- Header-only library
- No heap allocation
- No exceptions
- No RTTI
- Builds BTHome bytes only (you use your BLE stack for advertising)

## What this library provides

1. BTHome service-data AD element via `BTHome::Packet<Capacity>`.
2. Full raw advertising payload (Flags + BTHome service data) via `BTHome::build_advertising(...)`.
3. All BTHome v2 object types, including variable-length Text (0x53) and Raw (0x54) via `BTHome::text(...)` / `BTHome::raw(...)`.
4. AES-CCM encrypted payloads via `BTHome::EncryptedPacket<Capacity>` + `BTHome::build_encrypted_advertising(...)` with a pluggable cipher backend.

## Install

### PlatformIO

Published in the [PlatformIO Registry](https://registry.platformio.org/libraries/mvoss96/bthome-cpp):

```ini
lib_deps = mvoss96/bthome-cpp@^0.3.0
```

Pinning a git tag works as well:
`lib_deps = https://github.com/mvoss96/bthome-cpp.git#v0.3.0`

### Arduino

Install **bthome-cpp** via the IDE's Library Manager (or put this repository
in your Arduino libraries folder), add a BLE stack library such as
NimBLE-Arduino, include `bthome.h`.

### ESP-IDF

Add the repo under your project's `components/` folder (it ships an
`idf_component.yml`) — see [examples/esp_idf](examples/esp_idf) for a
complete project.

### Zephyr / nRF Connect SDK

Add the repo as a Zephyr module (west manifest or workspace checkout) — see
[examples/zephyr/README.md](examples/zephyr/README.md) for the `west.yml`
snippet, build commands and board notes.

## Minimal usage

```cpp
#include "bthome.h"

BTHome::Packet<31> packet;
packet.add(BTHome::temperature(22.4f));
packet.add(BTHome::humidity(54.3f));
packet.add(BTHome::battery(92));

const std::uint8_t* ad_element = packet.data();
std::size_t ad_size = packet.size();
```

`packet.data()` / `packet.size()` returns one AD element in this format:

`[len][0x16][uuid lo][uuid hi][device-info][measurements...]`

### Events

Button, command and dimmer events use the same `add()` path. Receivers only
process an event when the packet id changes, so include a fresh `packet_id()`
and feel free to advertise the same event packet repeatedly for reliability:

```cpp
BTHome::Packet<31> packet;
packet.setTriggerBased(true);  // only for devices that broadcast solely on events
packet.add(BTHome::packet_id(next_id++));
packet.add(BTHome::button_event(BTHome::ButtonEventType::Press));
packet.add(BTHome::command_event(BTHome::CommandEventType::Toggle));
packet.add(BTHome::dimmer_event(BTHome::DimmerEventType::RotateLeft, 3));
```

With several buttons, the k-th `button_event` entry addresses button k — pad
earlier buttons with `ButtonEventType::None` (spec example `3A 00 3A 01` =
press on button 2). The spec advises sending command events only in encrypted
advertisements — anyone in radio range can observe or spoof plaintext
commands.

## Build full advertising payload

```cpp
std::uint8_t adv[31] = {};
int n = BTHome::build_advertising(packet, adv, sizeof(adv));
if (n < 0)
{
    // buffer too small or invalid args
}
```

Result format:

`[Flags AD][BTHome Service Data AD][optional Local Name AD]`

## Encryption (AES-CCM)

The library implements the BTHome v2 encryption scheme (nonce construction,
counter handling, payload layout) but stays dependency-free: the AES-128-CCM
primitive is supplied as a callback. Two ready-made adapters ship as separate
headers (include the one your platform provides):

- `bthome_crypto_mbedtls.h` — [mbedtls](https://github.com/Mbed-TLS/mbedtls);
  bundled with ESP-IDF and the ESP32 Arduino core, on desktop link
  `-lmbedcrypto`.
- `bthome_crypto_psa.h` — PSA Crypto API; vanilla Zephyr (mbedtls-backed),
  nRF Connect SDK (Oberon/CryptoCell — the legacy mbedtls API is deprecated
  there), and TF-M environments.

Both adapters are tested against the official spec vector and produce
byte-identical output.

```cpp
#include "bthome.h"
#include "bthome_crypto_mbedtls.h"

// EncryptedPacket<28> reserves the 8-byte overhead (counter + MIC) internally:
// size() already includes it, so fill logic can never overflow the advertisement.
BTHome::EncryptedPacket<28> packet;
packet.add(BTHome::temperature(22.4f));

BTHome::Encryptor encryptor(&BTHome::mbedtls_ccm_backend);
encryptor.setKey(key);  // 16 bytes, shared with Home Assistant
encryptor.setMac(mac);  // the MAC your BLE stack advertises with
encryptor.setCounter(restored_counter);  // restore after reboot!

std::uint8_t adv[31] = {};
int n = BTHome::build_encrypted_advertising(packet, encryptor, adv, sizeof(adv));
```

The counter is owned by the `Encryptor` and consumed exactly once per
successful build, so CCM nonce reuse is structurally impossible. It must
survive reboots: persist `encryptor.counter()` periodically and restore it
with a safety margin via `setCounter()` — receivers reject non-increasing
counters as replays. Encryption costs 8 bytes of advertisement budget.

## Local checks

Build and run host tests from project root:

```powershell
g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I .\src .\tests\test_bthome.cpp -o .\build\test_bthome.exe
.\build\test_bthome.exe
```

Expected output ends with:

`ALL TESTS PASSED`

### Interop check against Home Assistant's parser

`tests/interop/` cross-checks every factory, event and encrypted packet
against [bthome-ble](https://github.com/Bluetooth-Devices/bthome-ble) — the
parser Home Assistant uses. The C++ generator emits advertisements built via
the public API; the Python checker decodes them with bthome-ble and asserts
the values (and the bindkey decryption) round-trip. Runs in CI; locally:

```bash
g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I src tests/interop/generate_corpus.cpp -lmbedcrypto -o build/generate_corpus
pip install bthome-ble
./build/generate_corpus | python3 tests/interop/check_with_bthome_ble.py
```

## Examples

- Arduino NimBLE: `examples/arduino_nimble/arduino_nimble.ino`
- Arduino NimBLE encrypted (MAC + Preferences counter): `examples/arduino_nimble_encrypted/arduino_nimble_encrypted.ino`
- ESP-IDF: `examples/esp_idf/main/main.cpp`
- ESP-IDF encrypted (MAC + NVS counter persistence): `examples/esp_idf_encrypted/main/main.cpp`
- Generic C++: `examples/generic/main.cpp`
- Generic encrypted (prints the official spec vector): `examples/generic_encrypted/main.cpp`
- Zephyr: `examples/zephyr/src/main.cpp`
- Zephyr encrypted (PSA Crypto backend, MAC byte-order handling): `examples/zephyr_encrypted/src/main.cpp`

## Notes

- BTHome object IDs and encodings are implemented in `src/bthome_defs.h` and `src/bthome_encoding.h`.
- Factories such as `BTHome::temperature(...)` are in `src/bthome_factories.h`.
- Packet assembly is in `src/bthome_packet.h`.
