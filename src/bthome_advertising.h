#pragma once

#include <stddef.h>
#include <stdint.h>
// C headers (<stdint.h>/<stddef.h>/<string.h>) instead of the C++ wrappers:
// avr-gcc ships no libstdc++ wrapper headers at all, and Zephyr's minimal C++
// library lacks <cstring>. The C headers exist on every supported toolchain,
// so types and calls stay unqualified (uint8_t, memcpy - no std::).
#include <string.h>

#include "bthome_packet.h"

namespace BTHome
{

/**
 * @brief Build a raw BLE advertising payload for BTHome.
 * @tparam PacketT Any packet type exposing data() and size() - Packet or
 *         StaticPacket.
 * @param packet Source BTHome service-data AD element.
 * @param out Destination byte buffer for the final raw advertising payload.
 * @param out_capacity Capacity of @p out in bytes.
 * @param local_name Optional local name; pass nullptr to omit a name AD element.
 * @param complete_local_name true for AD type 0x09 (Complete Local Name), false for 0x08 (Shortened Local Name).
 * @return Payload size in bytes on success, or -1 on error.
 */
template <typename PacketT>
int build_advertising(const PacketT &packet,
                      uint8_t *out,
                      size_t out_capacity,
                      const char *local_name = nullptr,
                      bool complete_local_name = true)
{
    constexpr size_t kFlagsBytes = 3; // 02 01 06
    if (out == nullptr)
    {
        return -1;
    }

    const size_t local_name_len = (local_name != nullptr) ? strlen(local_name) : 0;
    if (local_name_len > 0xFE)
    {
        return -1;
    }

    const size_t total = kFlagsBytes + packet.size() + ((local_name_len > 0) ? (2 + local_name_len) : 0);
    if (total > out_capacity)
    {
        return -1;
    }

    out[0] = 0x02; // length of Flags AD
    out[1] = 0x01; // AD type: Flags
    out[2] = 0x06; // Flags value: LE General Discoverable Mode, BR/EDR Not Supported

    // Copy full BTHome service-data AD element directly after Flags AD.
    memcpy(out + kFlagsBytes, packet.data(), packet.size());

    size_t p = kFlagsBytes + packet.size();
    if (local_name_len > 0)
    {
        out[p++] = static_cast<uint8_t>(1 + local_name_len); // length of Local Name AD
        out[p++] = complete_local_name ? 0x09 : 0x08;             // AD type: Complete or Shortened Local Name
        memcpy(out + p, local_name, local_name_len);         // Copy local name
        p += local_name_len;
    }

    return static_cast<int>(p);
}

} // namespace BTHome
