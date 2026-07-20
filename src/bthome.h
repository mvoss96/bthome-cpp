// bthome.h - facade header for BTHome v2 payload generation
// Reference: https://bthome.io/format/
#pragma once

// What this library can generate:
// 1) BTHome service-data AD element (type 0x16), via BTHome::Packet.
// 2) BTHome raw advertising payload [Flags AD + BTHome Service Data AD],
//    via BTHome::build_advertising(...).

#include "bthome_defs.h"
#include "bthome_encoding.h"
#include "bthome_factories.h"
#include "bthome_packet.h"
#include "bthome_advertising.h"
