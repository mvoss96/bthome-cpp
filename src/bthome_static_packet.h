#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bthome_defs.h"
#include "bthome_objects.h"

// A packet whose object set is fixed at compile time.
//
// The common sensor node sends the same objects every cycle and only varies
// the values. Saying so up front removes everything Packet does at runtime:
// no insertion sort, no memmove, no offset table, and a buffer sized to the
// objects instead of to a worst case. On an ATmega328P that is ~1 KB of flash
// and 37 bytes of stack per packet; the emitted bytes are identical.
//
//   BTHome::StaticPacket<BTHome::ObjectId::PacketId,
//                        BTHome::ObjectId::Temperature,
//                        BTHome::ObjectId::Humidity> packet;
//   packet.put<BTHome::ObjectId::Temperature>(BTHome::temperature(22.4f));
//
// Not a replacement for Packet. Use Packet when the object set varies between
// packets, for Text/Raw/command events (their length is not a compile-time
// property), for several objects with the same id such as button padding, and
// for encrypted packets - EncryptedPacket wraps Packet.

namespace BTHome
{

    template <ObjectId... Ids>
    class StaticPacket
    {
        static constexpr size_t kHeaderBytes = ServiceDataHeader::kByteCount;

        static_assert(sizeof...(Ids) > 0, "StaticPacket needs at least one object id");

        // Every object must have a fixed width: the buffer layout is decided
        // at compile time, and Text, Raw and command events carry their length
        // in the payload.
        static constexpr bool allFixedWidth()
        {
            bool ok = true;
            ((ok = ok && !detail::object_layout(detail::oid(Ids)).variable &&
                   detail::object_layout(detail::oid(Ids)).kind != ObjectKind::Unknown), ...);
            return ok;
        }
        static_assert(allFixedWidth(),
                      "StaticPacket takes fixed-width objects only - Text, Raw and command "
                      "events have no compile-time width, and unknown ids no layout at all");

        // Offsets are derived from the id order, so the same id twice would
        // collapse onto one slot. Multi-instance objects need Packet.
        static constexpr bool idsDistinct()
        {
            bool ok = true;
            ((ok = ok && (occurrences(detail::oid(Ids)) == 1)), ...);
            return ok;
        }
        static constexpr size_t occurrences(uint8_t id)
        {
            size_t n = 0;
            ((n += (detail::oid(Ids) == id) ? 1u : 0u), ...);
            return n;
        }
        static_assert(idsDistinct(),
                      "StaticPacket takes each object id once - repeated ids (button padding, "
                      "several temperatures) need Packet");

        static constexpr size_t totalBytes()
        {
            size_t n = kHeaderBytes;
            ((n += 1u + detail::object_layout(detail::oid(Ids)).width), ...);
            return n;
        }
        static_assert(totalBytes() <= 255, "AD element length byte limits the packet to 255 bytes");

        // BTHome serializes objects in ascending id order, so an object's
        // offset is the header plus everything with a smaller id. No sorting
        // step is needed - the order falls out of the arithmetic, and the ids
        // may be listed in any order.
        static constexpr size_t offsetOf(uint8_t id)
        {
            size_t off = kHeaderBytes;
            ((off += (detail::oid(Ids) < id) ? (1u + detail::object_layout(detail::oid(Ids)).width)
                                             : 0u),
             ...);
            return off;
        }

        uint8_t m_buf[totalBytes()] = {};

    public:
        /**
         * @brief Builds the header and writes every object id into its slot.
         *
         * Value bytes stay zero until a put(); an object that is never written
         * therefore transmits as zero rather than being left out.
         */
        StaticPacket()
        {
            ServiceDataHeader header{};
            header.writeTo(m_buf);
            m_buf[0] = static_cast<uint8_t>(totalBytes() - 1);
            // A fold rather than a loop over a table: the ids are baked in as
            // constants, so nothing is emitted into RAM.
            ((m_buf[offsetOf(detail::oid(Ids))] = detail::oid(Ids)), ...);
        }

        /**
         * @brief Writes one object's value bytes into its compile-time slot.
         * @tparam Id Which object to write; must be one of the packet's ids.
         * @param m Measurement from the matching factory.
         * @return false when @p m carries a different object id than @p Id,
         *         which would otherwise write one object's bytes into
         *         another's slot. The check is a single byte compare and
         *         folds away when the factory call is inlined.
         */
        template <ObjectId Id>
        bool put(const Measurement &m)
        {
            static_assert(occurrences(detail::oid(Id)) == 1,
                          "this object id is not part of the packet");
            constexpr size_t off = offsetOf(detail::oid(Id));
            constexpr uint8_t width = detail::object_layout(detail::oid(Id)).width;

            if (m.object_id != detail::oid(Id))
            {
                return false;
            }
            for (uint8_t i = 0; i < width; ++i)
            {
                m_buf[off + 1 + i] = m.data[i];
            }
            return true;
        }

        /**
         * @brief Sets the trigger-based flag (device-info bit 2).
         * @param on true for trigger-based behaviour.
         */
        void setTriggerBased(bool on)
        {
            m_buf[kHeaderBytes - 1] = static_cast<uint8_t>(
                on ? (m_buf[kHeaderBytes - 1] | DeviceInfo::kTriggerBasedBit)
                   : (m_buf[kHeaderBytes - 1] & ~DeviceInfo::kTriggerBasedBit));
        }

        /** @brief Full AD element: [len][0x16][uuid lo][uuid hi][info][objects...]. */
        const uint8_t *data() const { return m_buf; }

        /** @brief Size of the AD element in bytes; a compile-time constant. */
        size_t size() const { return totalBytes(); }

        /** @brief Service-data value, without the AD length/type prefix. */
        const uint8_t *serviceData() const { return m_buf + 2; }

        /** @brief Size of serviceData() in bytes. */
        size_t serviceDataSize() const { return totalBytes() - 2; }
    };

} // namespace BTHome
