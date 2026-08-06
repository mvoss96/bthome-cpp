#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bthome_defs.h"
#include "bthome_objects.h"

// Decoder for BTHome v2 service data — the inverse of Packet/factories.
// Header-only, no heap, no exceptions; iterates the objects of one
// service-data buffer in place. It works on BTHome bytes from any source:
// a BLE scan on an ESP32, or a transport with no BLE stack at all
// (ESP-NOW, nRF24 broadcasts, serial links).
//
// Two entry points, because BLE stacks disagree on where service data
// starts. Both are the same bytes, only cut differently:
//
//   Decoder(sd, len)              [uuid lo][uuid hi][info][objects...]
//     Packet::serviceData(), esp_ble_resolve_adv_data() for AD type 0x16,
//     and anything an ESP-NOW or nRF24 sender forwards verbatim.
//
//   Decoder::fromPayload(p, len)  [info][objects...]
//     NimBLE's getServiceData(): you asked for the data by UUID, so it
//     returns what follows the UUID and there is nothing left to check.
//
// Widths, scaling, signedness and object family all come from
// detail::object_layout() - the same table the factories encode with - so
// this file states no layout of its own. The one exception is the command
// event, whose length depends on its own payload.
//
// Usage:
//   BTHome::Decoder dec(service_data, len); // [uuid lo][uuid hi][info][objects...]
//   BTHome::Decoded obj;
//   while (dec.next(obj)) { switch (obj.kind) { ... } }
//   if (dec.status() != BTHome::DecodeStatus::End) { /* status says why */ }

namespace BTHome
{
    namespace detail
    {

        /**
         * @brief Widen the low @p width two's-complement bytes of @p bits to
         *        a full int32_t.
         *
         * constexpr on purpose: it lets the static_asserts below evaluate the
         * arithmetic with the *target's* integer widths, which is the whole
         * point. The failure mode this guards against exists only where int is
         * 16 bits - `~0u` is `unsigned int`, so on avr-gcc it can neither reach
         * bit 31 nor be shifted by 16 or 24 without undefined behaviour, while
         * the same expression is perfectly fine on every 32-bit host. A host
         * test can therefore never catch that regression, and neither can a
         * warning: `width` is a runtime value here, so the compiler cannot
         * diagnose the shift. Only constant evaluation on the real target can,
         * and that is what the assertions below do.
         *
         * @param bits Raw wire bits, little-endian assembled.
         * @param width Number of value bytes, 1..4.
         * @return The value as a signed 32-bit integer.
         */
        constexpr int32_t sign_extend(uint32_t bits, uint8_t width)
        {
            if (width < 4 && (bits & (uint32_t{1} << (8u * width - 1u))) != 0)
            {
                bits |= ~uint32_t{0} << (8u * width);
            }
            return static_cast<int32_t>(bits);
        }

        // Evaluated by every compiler that includes this header, so avr-gcc
        // checks these on a 16-bit-int target during the ordinary AVR build.
        static_assert(sign_extend(0xD8u, 1) == -40, "sint8 sign extension");
        static_assert(sign_extend(0xFB2Eu, 2) == -1234, "sint16 sign extension");
        static_assert(sign_extend(0xFFFFFFu, 3) == -1, "sint24 sign extension");
        static_assert(sign_extend(0xFFFFFFFFu, 4) == -1, "sint32 passes through");
        static_assert(sign_extend(0x7FFFu, 2) == 32767, "positive sint16 unchanged");
        static_assert(sign_extend(0x00u, 1) == 0, "zero stays zero");

    } // namespace detail

    /**
     * @brief Why iteration is over - the decoder's single result.
     *
     * A receiver wants these apart: an unknown id means this library version
     * is older than the sender, a truncated buffer means the transport lost
     * bytes, and an encrypted payload means a key is missing. They call for
     * three different fixes.
     */
    enum class DecodeStatus : uint8_t
    {
        Ok,        // iteration in progress, nothing wrong so far
        End,       // every object was read - the only clean outcome
        BadHeader, // service UUID does not match, or fewer than 3 bytes
        Encrypted, // device-info encrypted bit is set; decrypt first
        Truncated, // an object announces more bytes than the buffer holds
        UnknownId, // object id unknown here; out.object_id names it
        UnsupportedVersion, // device-info names a BTHome version other than 2
    };

    /**
     * @brief One decoded object, as a view into the caller's buffer.
     *
     * Three fields carry the payload, and which one applies follows from
     * @c kind:
     *  - @c value   the physical value, for Sensor objects
     *  - @c raw     the wire bits, for every fixed-width object and for
     *               events; on(), event() and steps() read it for you
     *  - @c bytes / @c length  for Text and Raw, pointing into the buffer
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
        float value = 0.0f;             // Sensor only
        uint32_t raw = 0;               // fixed-width objects and events
        const uint8_t *bytes = nullptr; // Text/Raw payload view
        uint8_t length = 0;             // Text/Raw payload length

        /** @brief Binary sensor state. */
        bool on() const { return raw != 0; }

        /** @brief Button/dimmer event code, or a command event's opcode. */
        uint8_t event() const { return static_cast<uint8_t>(raw & 0xFFu); }

        /** @brief Dimmer step count, or a command event's first argument. */
        uint8_t steps() const { return static_cast<uint8_t>((raw >> 8) & 0xFFu); }

        /**
         * @brief Compares against a BTHome object id.
         * @param id Object id to test for. Only ObjectId is accepted, so an
         *        event code cannot be passed here by mistake.
         * @return true when this object carries that id.
         */
        bool is(ObjectId id) const { return object_id == detail::oid(id); }
    };

    class Decoder
    {
        static constexpr size_t kUuidBytes = 2; // the 16-bit service UUID

    public:
        /**
         * @brief Decodes BTHome service data as BLE carries it.
         * @param service_data [uuid lo][uuid hi][device info][objects...].
         *        The service UUID is verified; a mismatch is BadHeader.
         * @param len Number of bytes in @p service_data.
         */
        Decoder(const uint8_t *service_data, size_t len)
            : Decoder(service_data, len, kUuidBytes)
        {
        }

        /**
         * @brief Decodes service data whose UUID the BLE stack already
         *        matched and stripped.
         * @param payload [device info][objects...]. NimBLE's
         *        getServiceData() returns this shape - the data was looked up
         *        by UUID, so the UUID is not repeated and there is nothing
         *        left for the decoder to verify.
         * @param len Number of bytes in @p payload.
         */
        static Decoder fromPayload(const uint8_t *payload, size_t len)
        {
            return Decoder(payload, len, 0);
        }

        // Header information, readable even when the payload cannot be
        // iterated - an encrypted packet still tells you its version.
        // m_info stays 0 for a bad header, so these read false / 0 there.
        bool encrypted() const { return (m_info & DeviceInfo::kEncryptedBit) != 0; }
        bool triggerBased() const { return (m_info & DeviceInfo::kTriggerBasedBit) != 0; }
        uint8_t version() const { return static_cast<uint8_t>(m_info >> 5); }

        /**
         * @brief Why iteration ended. Sticky once it leaves Ok.
         * @return DecodeStatus::End after a complete pass, otherwise the
         *         reason the decoder stopped.
         */
        DecodeStatus status() const { return m_status; }

        /**
         * @brief Reads the next object.
         * @param out Filled when the call returns true. On a false return it
         *        is unspecified, except after DecodeStatus::UnknownId, where
         *        out.object_id names the id that stopped the parse.
         * @return true while objects remain; false once status() leaves Ok.
         */
        bool next(Decoded &out)
        {
            if (m_status != DecodeStatus::Ok)
            {
                return false; // terminal status is sticky
            }
            if (m_pos >= m_len)
            {
                m_status = DecodeStatus::End;
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
                return fail(DecodeStatus::UnknownId);
            }

            if (layout.variable)
            {
                return nextVariable(out, layout);
            }

            if (m_pos + 1u + layout.width > m_len)
            {
                return fail(DecodeStatus::Truncated);
            }

            // The wire bits go into raw for every fixed-width object; on(),
            // event() and steps() read them back. Only sensors additionally
            // need scaling into a physical value.
            uint32_t raw = 0;
            for (uint8_t i = 0; i < layout.width; ++i)
            {
                raw |= static_cast<uint32_t>(m_data[m_pos + 1 + i]) << (8 * i);
            }
            out.raw = raw;
            if (layout.kind == ObjectKind::Sensor)
            {
                out.value = physical(raw, layout);
            }

            m_pos += 1u + layout.width;
            return true;
        }

    private:
        // Shared by both entry points. uuid_bytes is 2 when the buffer still
        // carries the service UUID and 0 when the caller's BLE stack already
        // matched and removed it.
        Decoder(const uint8_t *data, size_t len, size_t uuid_bytes)
            : m_data(data), m_len(len), m_pos(uuid_bytes + 1), m_info(0),
              m_status(DecodeStatus::BadHeader)
        {
            if (data == nullptr || len < uuid_bytes + 1)
            {
                return; // stays BadHeader
            }
            if (uuid_bytes != 0 &&
                (data[0] != static_cast<uint8_t>(kServiceUuid & 0xFFu) ||
                 data[1] != static_cast<uint8_t>(kServiceUuid >> 8)))
            {
                return; // not BTHome service data
            }

            m_info = data[uuid_bytes];
            // A version other than 2 may lay out the whole payload differently,
            // so it is refused rather than decoded as if it were v2. m_info is
            // already set: version() still names the version that was refused.
            // The reserved device-info bits are deliberately tolerated - they
            // do not change the object format, and rejecting them would break
            // both forward compatibility and senders using them privately.
            if (version() != 2)
            {
                m_status = DecodeStatus::UnsupportedVersion;
                return;
            }
            // Encrypted payloads carry ciphertext objects, so refuse to
            // iterate them right away. Decrypt first, then feed the plaintext
            // to a new Decoder (see bthome_encryption.h for the layout).
            m_status = encrypted() ? DecodeStatus::Encrypted : DecodeStatus::Ok;
        }

        bool fail(DecodeStatus reason)
        {
            m_status = reason;
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
                    return fail(DecodeStatus::Truncated);
                }
                const uint8_t argc = m_data[m_pos + 1];
                const uint8_t opcode = m_data[m_pos + 2];
                const uint8_t first_arg = (argc >= 1) ? m_data[m_pos + 3] : 0;
                // Packed the way the dimmer event's two wire bytes already
                // land, so event() and steps() work here too.
                out.raw = static_cast<uint32_t>(opcode) |
                          (static_cast<uint32_t>(first_arg) << 8);
                m_pos += 3u + argc;
                return true;
            }

            if (m_pos + 2 > m_len || m_pos + 2 + m_data[m_pos + 1] > m_len)
            {
                return fail(DecodeStatus::Truncated);
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
            return static_cast<float>(detail::sign_extend(raw, layout.width)) *
                   layout.factor;
        }

        const uint8_t *m_data;
        size_t m_len;
        size_t m_pos;
        uint8_t m_info;
        DecodeStatus m_status;
    };

} // namespace BTHome
