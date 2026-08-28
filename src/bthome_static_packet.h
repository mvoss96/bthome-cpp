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
//   BTHome::StaticPacket packet(BTHome::packet_id(7),
//                               BTHome::temperature(22.4f),
//                               BTHome::humidity(54.3f));
//
// The ids come from the values: the factories return Typed<Id>, which carries
// the object id in its type, so the layout is deduced and an id can never be
// paired with the wrong value.
//
// A StaticPacket always transmits its whole set - the layout is fixed, so an
// object cannot be left out. A value the factory could not encode (make_sensor
// drops NaN and the infinities) therefore goes on the air as zero rather than
// being omitted. Use Packet where a sensor can be unavailable.
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
         * @brief Builds the packet from one value per object.
         *
         * The object ids are deduced from the arguments, so the class template
         * arguments need not be written out:
         *
         *   BTHome::StaticPacket packet(BTHome::temperature(22.4f),
         *                               BTHome::humidity(54.3f));
         *
         * Order does not matter - objects are placed by id, as BTHome
         * requires. Every object gets a value; there is no half-filled packet.
         *
         * @param ms One measurement per object id, from the factories.
         */
        explicit StaticPacket(const Typed<Ids> &...ms)
        {
            ServiceDataHeader header{};
            header.writeTo(m_buf);
            m_buf[0] = static_cast<uint8_t>(totalBytes() - 1);
            // A fold rather than a loop over a table: ids and offsets are
            // constants here, so nothing is emitted into RAM.
            (writeObject<Ids>(ms), ...);
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

    private:
        // Writes one object at its compile-time offset. Takes the Typed
        // directly rather than converting to a Measurement first, which would
        // materialize six value bytes per object for nothing. No id check is
        // needed: a Typed<Id> could not have been built for another object.
        template <ObjectId Id>
        void writeObject(const Typed<Id> &t)
        {
            constexpr size_t off = offsetOf(detail::oid(Id));
            m_buf[off] = detail::oid(Id);
            for (uint8_t i = 0; i < Typed<Id>::kWidth; ++i)
            {
                m_buf[off + 1 + i] = t.data[i];
            }
        }
    };

} // namespace BTHome
