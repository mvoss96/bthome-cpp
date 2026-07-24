#pragma once

#include <stdint.h>

#include "bthome_defs.h"
#include "bthome_objects.h"

namespace BTHome
{
    namespace detail
    {

        /**
         * @brief Encode an unsigned raw integer into a Measurement payload.
         * @param id Object ID byte.
         * @param raw Unsigned raw value.
         * @param width Number of payload bytes.
         * @return Encoded Measurement with little-endian value bytes.
         */
        inline Measurement make_u(uint8_t id, uint64_t raw,
                                  uint8_t width)
        {
            Measurement m;
            m.object_id = id;
            m.len = width;
            if (width == 0)
            {
                return m; // unknown object id - nothing to encode
            }

            const uint64_t umax =
                (width >= 8) ? ~uint64_t{0}
                             : ((uint64_t{1} << (8 * width)) - 1);
            if (raw > umax)
            {
                raw = umax;
            }

            for (uint8_t i = 0; i < width; ++i)
            {
                m.data[i] = static_cast<uint8_t>(raw & 0xFF);
                raw >>= 8;
            }
            return m;
        }

        /**
         * @brief Encode a signed raw integer into a Measurement payload.
         * @param id Object ID byte.
         * @param raw Signed raw value.
         * @param width Number of payload bytes.
         * @return Encoded Measurement with little-endian value bytes.
         */
        inline Measurement make_s(uint8_t id, int64_t raw,
                                  uint8_t width)
        {
            Measurement m;
            m.object_id = id;
            m.len = width;
            if (width == 0)
            {
                return m; // unknown object id - nothing to encode
            }

            const int64_t max_v = (int64_t{1} << (8 * width - 1)) - 1;
            const int64_t min_v = -(int64_t{1} << (8 * width - 1));
            if (raw > max_v)
            {
                raw = max_v;
            }
            if (raw < min_v)
            {
                raw = min_v;
            }

            uint64_t u = static_cast<uint64_t>(raw);
            for (uint8_t i = 0; i < width; ++i)
            {
                m.data[i] = static_cast<uint8_t>(u & 0xFF);
                u >>= 8;
            }
            return m;
        }

        /**
         * @brief Encode a sensor value using the scaling rules of object_layout().
         * @param id BTHome sensor object ID.
         * @param value Physical value to encode.
         * @return Encoded Measurement.
         */
        inline Measurement make_sensor(SensorObjectId id, float value)
        {
            const ObjectLayout l = object_layout(oid(id));
            Measurement m;
            m.object_id = oid(id);
            m.len = l.width;
            if (l.width == 0)
            {
                return m; // unknown object id - nothing to encode
            }

            // Scale to raw integer units and round half away from zero.
            const float units = value / l.factor;
            int64_t raw = static_cast<int64_t>(units >= 0.0f ? units + 0.5f : units - 0.5f);

            // Clamp to the representable range for the selected width/sign.
            if (l.is_signed)
            {
                const int64_t max_v = (int64_t{1} << (8 * l.width - 1)) - 1;
                const int64_t min_v = -(int64_t{1} << (8 * l.width - 1));
                if (raw > max_v)
                {
                    raw = max_v;
                }
                if (raw < min_v)
                {
                    raw = min_v;
                }
            }
            else
            {
                if (raw < 0)
                {
                    raw = 0;
                }
                const uint64_t umax =
                    (l.width >= 8) ? ~uint64_t{0}
                                   : ((uint64_t{1} << (8 * l.width)) - 1);
                if (static_cast<uint64_t>(raw) > umax)
                {
                    raw = static_cast<int64_t>(umax);
                }
            }

            // Write payload in little-endian byte order.
            uint64_t u = static_cast<uint64_t>(raw);
            for (uint8_t i = 0; i < l.width; ++i)
            {
                m.data[i] = static_cast<uint8_t>(u & 0xFF);
                u >>= 8;
            }

            return m;
        }

        // The payload width of an object is stated once, in object_layout().
        // These wrappers look it up, so a factory only picks the signedness -
        // which its own parameter type already declares.

        /**
         * @brief Encode an unsigned value; the width comes from object_layout().
         * @tparam ObjectId Enum type of the object ID.
         * @param id Object ID.
         * @param raw Unsigned raw value.
         * @return Encoded Measurement.
         */
        template <typename ObjectId>
        inline Measurement u(ObjectId id, uint64_t raw)
        {
            return make_u(oid(id), raw, object_layout(oid(id)).width);
        }

        /**
         * @brief Encode a signed value; the width comes from object_layout().
         * @tparam ObjectId Enum type of the object ID.
         * @param id Object ID.
         * @param raw Signed raw value.
         * @return Encoded Measurement.
         */
        template <typename ObjectId>
        inline Measurement s(ObjectId id, int64_t raw)
        {
            return make_s(oid(id), raw, object_layout(oid(id)).width);
        }

        /**
         * @brief Encode a boolean value; the width comes from object_layout().
         * @tparam ObjectId Enum type of the object ID.
         * @param id Object ID.
         * @param on Boolean payload value.
         * @return Encoded Measurement.
         */
        template <typename ObjectId>
        inline Measurement b(ObjectId id, bool on)
        {
            Measurement m;
            m.object_id = oid(id);
            m.len = object_layout(oid(id)).width;
            m.data[0] = on ? 1 : 0;
            return m;
        }

    } // namespace detail
} // namespace BTHome
