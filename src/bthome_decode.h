#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bthome_defs.h"
#include "bthome_objects.h"

// Decoder for BTHome v2 service data — the inverse of Packet/factories.
// Header-only, no heap, no exceptions; iterates the objects of one
// service-data buffer in place. Written for receivers that get BTHome
// payloads over transports without a BLE stack (ESP-NOW, nRF24, serial),
// but works on any raw service-data bytes.
//
// Widths, scaling, signedness and object family all come from
// detail::object_layout() - the same table the factories encode with - so
// this file states no layout of its own. The one exception is the command
// event, whose length depends on its own payload.
//
// Usage:
//   BTHome::Decoder dec(service_data, len); // [uuid lo][uuid hi][info][objects...]
//   if (!dec.valid() || dec.encrypted()) { ... }
//   BTHome::Decoded obj;
//   while (dec.next(obj)) { switch (obj.kind) { ... } }
//   if (!dec.ok()) { /* malformed or unknown object id: rest was skipped */ }

namespace BTHome
{

    /**
     * @brief One decoded object, as a view into the caller's buffer.
     *
     * Which fields carry meaning follows from @c kind:
     *  - Sensor          @c value (physical value) and @c raw (wire bits)
     *  - Binary          @c on, plus @c value as 1.0/0.0 for convenience
     *  - ButtonEvent     @c event
     *  - DimmerEvent     @c event and @c steps
     *  - CommandEvent    @c event (opcode) and @c steps (first argument)
     *  - Text / Raw      @c bytes and @c length, pointing into the buffer
     *  - everything else @c raw
     *
     * @note For the exact 32-bit integer objects - CountU32 (0x3E), CountS32
     *       (0x5B) and Timestamp (0x50) - a float cannot represent every
     *       value, since its mantissa is 24 bits. @c raw is authoritative
     *       there; @c value is only an approximation.
     */
    struct Decoded
    {
        uint8_t object_id = 0;
        ObjectKind kind = ObjectKind::Unknown;
        float value = 0.0f;          // Sensor: physical value; Binary: 1.0/0.0; else 0
        uint32_t raw = 0;            // fixed-width objects: unsigned little-endian bits
        bool on = false;             // Binary
        uint8_t event = 0;           // Button/Dimmer/Command code
        uint8_t steps = 0;           // Dimmer step count / first Command argument
        const uint8_t *bytes = nullptr; // Text/Raw payload view (nullptr otherwise)
        uint8_t length = 0;          // Text/Raw payload length
    };

    class Decoder
    {
    public:
        // service_data: [uuid lo][uuid hi][device info][objects...] — exactly
        // what Packet::serviceData() emits and BLE service data carries.
        Decoder(const uint8_t *service_data, size_t len)
            : m_data(service_data), m_len(len), m_pos(3), m_info(0), m_valid(false), m_ok(false)
        {
            m_valid = (service_data != nullptr) && (len >= 3) &&
                      (service_data[0] == static_cast<uint8_t>(kServiceUuid & 0xFFu)) &&
                      (service_data[1] == static_cast<uint8_t>(kServiceUuid >> 8));
            if (m_valid)
            {
                m_info = service_data[2];
            }
            m_ok = m_valid;
        }

        bool valid() const { return m_valid; }
        // Encrypted payloads carry ciphertext objects: next() refuses to
        // iterate them. Decrypt first (see bthome_encryption.h layout).
        bool encrypted() const { return m_valid && (m_info & DeviceInfo::kEncryptedBit) != 0; }
        bool triggerBased() const { return m_valid && (m_info & DeviceInfo::kTriggerBasedBit) != 0; }
        uint8_t version() const { return static_cast<uint8_t>(m_info >> 5); }

        // False once all objects were consumed OR parsing had to stop.
        // ok() distinguishes: true = clean end, false = malformed/unknown id.
        bool next(Decoded &out)
        {
            if (!m_ok || encrypted() || m_pos >= m_len)
            {
                return false;
            }

            const uint8_t id = m_data[m_pos];
            const detail::ObjectLayout layout = detail::object_layout(id);

            out = Decoded();
            out.object_id = id;
            out.kind = layout.kind;

            // An unknown id makes the remaining length unknowable, so the rest
            // of the buffer cannot be trusted - stop rather than guess.
            if (layout.kind == ObjectKind::Unknown)
            {
                return fail();
            }

            if (layout.variable)
            {
                return nextVariable(out, layout);
            }

            if (m_pos + 1u + layout.width > m_len)
            {
                return fail();
            }

            uint32_t raw = 0;
            for (uint8_t i = 0; i < layout.width; ++i)
            {
                raw |= static_cast<uint32_t>(m_data[m_pos + 1 + i]) << (8 * i);
            }
            out.raw = raw;

            switch (layout.kind)
            {
            case ObjectKind::Binary:
                out.on = (raw != 0);
                out.value = out.on ? 1.0f : 0.0f;
                break;
            case ObjectKind::ButtonEvent:
                out.event = static_cast<uint8_t>(raw);
                break;
            case ObjectKind::DimmerEvent:
                out.event = static_cast<uint8_t>(raw & 0xFFu);
                out.steps = static_cast<uint8_t>(raw >> 8);
                break;
            case ObjectKind::Sensor:
                out.value = physical(raw, layout);
                break;
            default: // PacketId, DeviceTypeId, FirmwareVersion - raw carries it
                break;
            }

            m_pos += 1u + layout.width;
            return true;
        }

        bool ok() const { return m_ok; }

    private:
        bool fail()
        {
            m_ok = false;
            return false;
        }

        // Variable-length objects. Text/Raw are [id][len][bytes...]; the
        // command event is [id][argument count][opcode][arguments...] and is
        // the one layout object_layout() cannot describe, because its length
        // depends on its own payload (see command_event()).
        bool nextVariable(Decoded &out, const detail::ObjectLayout &layout)
        {
            if (layout.kind == ObjectKind::CommandEvent)
            {
                if (m_pos + 3 > m_len || m_pos + 3 + m_data[m_pos + 1] > m_len)
                {
                    return fail();
                }
                const uint8_t argc = m_data[m_pos + 1];
                out.event = m_data[m_pos + 2];
                out.steps = (argc >= 1) ? m_data[m_pos + 3] : 0;
                m_pos += 3u + argc;
                return true;
            }

            if (m_pos + 2 > m_len || m_pos + 2 + m_data[m_pos + 1] > m_len)
            {
                return fail();
            }
            out.length = m_data[m_pos + 1];
            out.bytes = m_data + m_pos + 2;
            m_pos += 2u + out.length;
            return true;
        }

        // Raw wire bits to physical value, per the table's width/sign/factor.
        static float physical(uint32_t raw, const detail::ObjectLayout &layout)
        {
            if (!layout.is_signed)
            {
                return static_cast<float>(raw) * layout.factor;
            }

            // Sign-extend in uint32_t. Not `~0u`: that is `unsigned int`, which
            // is 16 bits on avr-gcc, so it can neither reach bit 31 nor be
            // shifted by 16 or 24 without undefined behaviour.
            uint32_t bits = raw;
            if (layout.width < 4 &&
                (bits & (uint32_t{1} << (8u * layout.width - 1u))) != 0)
            {
                bits |= ~uint32_t{0} << (8u * layout.width);
            }
            return static_cast<float>(static_cast<int32_t>(bits)) * layout.factor;
        }

        const uint8_t *m_data;
        size_t m_len;
        size_t m_pos;
        uint8_t m_info;
        bool m_valid;
        bool m_ok;
    };

} // namespace BTHome
