#pragma once

#include <cstddef>
#include <cstdint>

#include "bthome_defs.h"

template <std::size_t Capacity>
class BTHomePacket
{
private:
    static constexpr std::size_t kHeaderBytes = BTHome::ServiceDataHeader::kByteCount; // [len][type][uuid lo][uuid hi][device-info]
    static_assert(Capacity >= kHeaderBytes, "Capacity must hold the BTHome header");   // Capacity must fit fixed header bytes.
    static_assert(Capacity <= 255, "AD element length byte limits Capacity to 255");
    static constexpr std::size_t kMaxItems = (Capacity - kHeaderBytes) / 2 + 1; // Worst-case entry count; exact fit is checked in insert().

    // The serialized AD element is the single source of truth: measurements are
    // inserted in canonical (ascending object-id) order directly into m_buf.
    // m_offsets tracks where each entry starts, so insertion positions can be
    // found without knowing the value width of already-stored object ids.
    BTHome::ServiceDataHeader m_header = {}; // Header model incl. device-info flags.
    std::uint8_t m_buf[Capacity] = {};       // Final AD element bytes.
    std::size_t m_size = kHeaderBytes;       // Current used bytes in m_buf.
    std::uint8_t m_offsets[kMaxItems] = {};  // Start offset of each entry in m_buf, ascending.
    std::size_t m_count = 0;                 // Number of entries.

public:
    /**
     * @brief Constructs an empty BTHome packet and initializes the fixed header bytes.
     */
    BTHomePacket()
    {
        m_header.writeTo(m_buf);
        m_buf[0] = static_cast<std::uint8_t>(m_size - 1);
    }

    /**
     * @brief Adds one measurement to the packet.
     * @param m Measurement to insert.
     * @return true if the measurement was inserted, false if capacity would be exceeded.
     */
    bool add(const BTHome::Measurement &m)
    {
        if (m.len > sizeof(m.data))
        {
            return false;
        }
        return insert(m.object_id, m.data, m.len);
    }

    /**
     * @brief Sets the trigger-based flag (device-info bit 2).
     * @param on true to set trigger-based behavior, false for regular updates.
     */
    void setTriggerBased(bool on)
    {
        m_header.m_device_info.m_trigger_based = on;
        m_buf[kHeaderBytes - 1] = m_header.m_device_info.toByte();
    }

    /**
     * @brief Sets the encryption flag in the device-info byte (bit 0).
     * @param on true to mark payload as encrypted, false otherwise.
     * @note This only sets the flag bit; payload encryption itself is external.
     */
    void setEncrypted(bool on)
    {
        m_header.m_device_info.m_encrypted = on;
        m_buf[kHeaderBytes - 1] = m_header.m_device_info.toByte();
    }

    /**
     * @brief Returns the full AD element bytes.
     * @return Pointer to bytes formatted as [len][0x16][uuid lo][uuid hi][device-info][measurements...].
     */
    const std::uint8_t *data() const
    {
        return m_buf;
    }

    /**
     * @brief Returns the current AD element size in bytes.
     * @return Number of bytes currently used in the packet buffer.
     */
    std::size_t size() const
    {
        return m_size;
    }

    /**
     * @brief Returns the service-data value bytes without AD length/type prefix.
     * @return Pointer to bytes formatted as [uuid lo][uuid hi][device-info][measurements...].
     */
    const std::uint8_t *serviceData() const
    {
        return m_buf + 2;
    }

    /**
     * @brief Returns the size of the service-data value (without AD length/type prefix).
     * @return Number of bytes in serviceData().
     */
    std::size_t serviceDataSize() const
    {
        return m_size - 2;
    }

private:
    /**
     * @brief Inserts one entry ([id][value bytes]) in canonical order into m_buf.
     * @param id Object ID byte.
     * @param value Little-endian value bytes.
     * @param vlen Number of value bytes.
     * @return true if insertion succeeded, false on capacity overflow.
     */
    bool insert(std::uint8_t id, const std::uint8_t *value, std::size_t vlen)
    {
        if (m_count >= kMaxItems)
        {
            return false;
        }

        const std::size_t need = 1 + vlen;
        if (m_size + need > Capacity)
        {
            return false;
        }

        // Find first entry whose object_id exceeds id (stable for equal ids).
        std::size_t k = 0;
        while (k < m_count && m_buf[m_offsets[k]] <= id)
        {
            ++k;
        }
        const std::size_t pos = (k == m_count) ? m_size : m_offsets[k];

        // Shift the buffer tail up by `need` bytes (backwards, regions overlap).
        for (std::size_t i = m_size; i > pos; --i)
        {
            m_buf[i + need - 1] = m_buf[i - 1];
        }

        // Shift the offsets of the moved entries and record the new one.
        for (std::size_t j = m_count; j > k; --j)
        {
            m_offsets[j] = static_cast<std::uint8_t>(m_offsets[j - 1] + need);
        }
        m_offsets[k] = static_cast<std::uint8_t>(pos);
        ++m_count;

        // Write the entry and finalize the AD length byte.
        m_buf[pos] = id;
        for (std::size_t i = 0; i < vlen; ++i)
        {
            m_buf[pos + 1 + i] = value[i];
        }
        m_size += need;
        m_buf[0] = static_cast<std::uint8_t>(m_size - 1);
        return true;
    }
};
