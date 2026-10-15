# Encrypted BTHome on a classic AVR

AES-CCM encrypted BTHome on an ATmega328P (Uno/Nano/Pro Mini), for transports
that are not BLE advertising.

Two things set this apart from the ESP32 and Zephyr encrypted examples:

**No advertisement to build.** An AVR has no BLE stack, so the bytes go out over
whatever radio the board has — nRF24L01, a UART-attached BLE module, LoRa. What
those want is the service-data value, which
[`build_encrypted_service_data()`](../../README.md#encrypting-for-something-other-than-ble-advertising)
produces: `[uuid lo][uuid hi][device-info][ciphertext][counter][MIC]`, exactly
what a receiver hands to `Decryptor::decryptServiceData()`.

**No platform crypto.** ESP-IDF ships mbedtls and Zephyr ships PSA; an ATmega
ships neither, and this library contains no cipher by design — it owns the
nonce, the layout and the counter, and takes the block cipher from you. So the
backend here comes from an external library.

## The cipher: micro-AES

[micro-AES](https://github.com/polfosol/micro-AES) (Apache-2.0) is the one that
fits: ANSI-C, explicitly aimed at 8-bit microcontrollers, and it implements CCM
with a configurable nonce and tag length. That last part matters — most compact
AES libraries stop at CTR/CBC, and BTHome mandates CCM, so GCM or EAX are not
substitutes.

It is not in the Arduino Library Manager, so install it by hand:

```bash
git clone --depth 1 https://github.com/polfosol/micro-AES ~/Arduino/libraries/micro-AES
```

Then **edit `micro_aes.h`** — BTHome needs a 13-byte nonce and a 4-byte MIC, and
micro-AES defaults to 11 and 16:

```c
    CCM_NONCE_LEN    = 13,
    CCM_TAG_LEN      = 4,
```

These are enum constants, not `#define`s, so a `-D` on the command line will not
override them; the header has to be edited. Get it wrong and every packet fails
its MIC check at the receiver — indistinguishable from a wrong key, so it is
worth verifying against a known vector once.

Then turn the modes you are not using off — `ECB`, `CBC`, `CFB`, `OFB`, `XEX`,
`KWA`, `FPE`, `CMAC`, `GCM`, `EAX`, `SIV`, `GCM_SIV`, `OCB`, `POLY1305`, `CTS`,
`XTS` to `0`, leaving `CTR` and `CCM` at `1` (CCM is built on CTR):

```c
#define ECB          0
#define CBC          0
/* … */
```

This one is not tidiness. Measured with `arduino-cli` on `arduino:avr:uno`:

| micro-AES configuration | Flash | RAM |
|---|---|---|
| defaults, all modes on | 23218 B (71 %) | **5572 B (272 %)** |
| CCM only | 6424 B (19 %) | 1450 B (70 %) |

At the defaults this sketch wants 5572 bytes of RAM on a part that has 2048, so
it cannot run — and `arduino-cli` prints that as `272%` **without failing the
build**. A green compile for a binary that cannot boot is the worst shape a
dependency problem can take, hence the emphasis.

Even configured down, 1450 bytes of 2048 is a lot for an ATmega328P, and about
430 of them are micro-AES itself: a 176-byte round-key schedule in `.bss`, and
its 256-byte S-box in `.rodata`, which avr-gcc copies into RAM at startup
because the library is portable C and knows nothing of `PROGMEM`. If RAM is
what runs out on your board, that is where it went.

## What encryption adds on the device side

1. **A nonce MAC both ends derive identically.** BTHome takes six bytes from the
   BLE advertiser address; off BLE there is none, so the two sides have to agree
   on what stands in for it. Whatever identifies a device on your transport is
   the right answer — an nRF24 receiver keyed by a 4-byte sender id, for
   instance, uses that id zero-extended to six. Nothing on the wire carries or
   checks this agreement.

2. **A counter that survives reboots.** Receivers reject a counter that does not
   advance, which is what makes a captured packet worthless to replay. This
   example resumes at last-stored + 1024 on boot and rewrites only once the
   counter has advanced by another 1024 — EEPROM endures about 100,000 cycles,
   and writing per packet would spend that in a month. A counter that went
   backwards does not corrupt anything; it mutes the device until the receiver
   is restarted, which is the more confusing failure of the two.

3. **One build per update.** Each consumes exactly one counter value, and a
   build at the end of the 32-bit counter space fails instead of wrapping —
   together that is what rules out CCM nonce reuse. The margin addition above
   saturates for the same reason.

## Build

```bash
arduino-cli core install arduino:avr
arduino-cli compile --fqbn arduino:avr:uno \
  --build-property compiler.cpp.extra_flags=-std=gnu++17 \
  examples/arduino_avr_encrypted
```

The `-std=gnu++17` is for the stock AVR core, which defaults to `gnu++11`; cores
like MiniCore already default to `gnu++17` and need nothing.
