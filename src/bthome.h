// bthome.h - facade header for BTHome v2 payload generation and parsing
// Reference: https://bthome.io/format/
#pragma once

// What this library can generate:
// 1) BTHome service-data AD element (type 0x16), via BTHome::Packet.
// 2) BTHome raw advertising payload [Flags AD + BTHome Service Data AD],
//    via BTHome::build_advertising(...).
// 3) AES-CCM encrypted advertising payload, via BTHome::EncryptedPacket +
//    BTHome::build_encrypted_advertising(...). The cipher itself is pluggable
//    (BTHome::Encryptor); include bthome_crypto_mbedtls.h separately for a
//    ready-made mbedtls backend (not part of this facade - it needs mbedtls).
// And parse:
// 4) BTHome service data back into typed objects, via BTHome::Decoder -
//    for receivers on transports without a BLE stack (ESP-NOW, nRF24, ...).

#include "bthome_defs.h"
#include "bthome_objects.h"
#include "bthome_encoding.h"
#include "bthome_factories.h"
#include "bthome_packet.h"
#include "bthome_static_packet.h"
#include "bthome_advertising.h"
#include "bthome_encryption.h"
#include "bthome_decode.h"
