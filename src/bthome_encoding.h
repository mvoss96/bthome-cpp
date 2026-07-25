#pragma once

#include <float.h>
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
         * @param width Number of payload bytes, at least 1. The only callers
         *        are u() and s(), which take the width from object_layout()
         *        and static_assert that it is non-zero.
         * @return Encoded Measurement with little-endian value bytes.
         */
        inline Measurement make_u(uint8_t id, uint64_t raw,
                                  uint8_t width)
        {
            Measurement m;
            m.object_id = id;
            m.len = width;

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
         * @param width Number of payload bytes, at least 1 (see make_u()).
         * @return Encoded Measurement with little-endian value bytes.
         */
        inline Measurement make_s(uint8_t id, int64_t raw,
                                  uint8_t width)
        {
            Measurement m;
            m.object_id = id;
            m.len = width;

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

        // The object id is a template parameter, not a function parameter, on
        // purpose. Inside the function a parameter is not a constant
        // expression, so object_layout() would be an ordinary call - and at
        // -Os, which is what Arduino, ESP-IDF and Zephyr build with, gcc
        // declines to inline a ~90-case switch. As a template argument the
        // lookup is evaluated by the language: the width, factor and clamp
        // bounds are baked in, the switch is never emitted for the encode
        // path, and a wrong id is a compile error instead of a silent
        // zero-length payload.

        /**
         * @brief Encode a sensor value using the scaling rules of object_layout().
         * @tparam Id BTHome sensor object ID.
         * @param value Physical value to encode.
         * @return Encoded Measurement.
         */
        template <auto Id>
        inline Measurement make_sensor(float value)
        {
            constexpr uint8_t byte_id = oid(Id);
            constexpr ObjectLayout l = object_layout(byte_id);
            static_assert(l.scaled, "make_sensor() is for float-scaled objects only");

            // Representable raw range for this object - both bounds follow
            // from the table, so they are compile-time constants.
            constexpr int64_t max_v = l.is_signed ? (int64_t{1} << (8 * l.width - 1)) - 1
                                                  : (int64_t{1} << (8 * l.width)) - 1;
            constexpr int64_t min_v = l.is_signed ? -(int64_t{1} << (8 * l.width - 1))
                                                  : 0;

            Measurement m;
            m.object_id = byte_id;
            m.len = l.width;

            // NaN and the infinities have no BTHome representation. Encoding
            // them would emit an extreme that looks like a real reading, so
            // drop the object instead: a missing object means "no value this
            // cycle", which is exactly what an unavailable sensor is.
            // Packet::add() rejects a zero-length Measurement, so the caller
            // gets a false back rather than a silently mangled packet.
            // (A comparison rather than isfinite(): no <math.h> needed, and it
            // is false for NaN and for both infinities. Note that -ffast-math
            // permits the compiler to assume no NaNs and drop this check.)
            if (!(value >= -FLT_MAX && value <= FLT_MAX))
            {
                m.len = 0;
                return m;
            }

            // Scale to raw units, clamping in float space: converting an
            // out-of-range float to int64_t is undefined behaviour, and
            // value / factor leaves int64 range entirely for the 1e-6 factors.
            const float units = value / l.factor;
            int64_t raw;
            if (units <= static_cast<float>(min_v))
            {
                raw = min_v;
            }
            else if (units >= static_cast<float>(max_v))
            {
                raw = max_v;
            }
            else
            {
                // Round half away from zero; rounding can still push the value
                // one past the edge, so clamp again.
                raw = static_cast<int64_t>(units >= 0.0f ? units + 0.5f : units - 0.5f);
                if (raw > max_v)
                {
                    raw = max_v;
                }
                if (raw < min_v)
                {
                    raw = min_v;
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
         * @tparam Id Object ID.
         * @param raw Unsigned raw value.
         * @return Encoded Measurement.
         */
        template <auto Id>
        inline Measurement u(uint64_t raw)
        {
            constexpr uint8_t byte_id = oid(Id);
            constexpr ObjectLayout l = object_layout(byte_id);
            static_assert(l.width != 0 && !l.variable, "object id has no fixed width");
            static_assert(!l.scaled, "scaled objects must go through make_sensor()");
            return make_u(byte_id, raw, l.width);
        }

        /**
         * @brief Encode a signed value; the width comes from object_layout().
         * @tparam Id Object ID.
         * @param raw Signed raw value.
         * @return Encoded Measurement.
         */
        template <auto Id>
        inline Measurement s(int64_t raw)
        {
            constexpr uint8_t byte_id = oid(Id);
            constexpr ObjectLayout l = object_layout(byte_id);
            static_assert(l.width != 0 && !l.variable, "object id has no fixed width");
            static_assert(l.is_signed, "object id is not a signed object");
            static_assert(!l.scaled, "scaled objects must go through make_sensor()");
            return make_s(byte_id, raw, l.width);
        }

        /**
         * @brief Encode a boolean value; the width comes from object_layout().
         * @tparam Id Object ID.
         * @param on Boolean payload value.
         * @return Encoded Measurement.
         */
        template <auto Id>
        inline Measurement b(bool on)
        {
            constexpr uint8_t byte_id = oid(Id);
            constexpr ObjectLayout l = object_layout(byte_id);
            static_assert(l.kind == ObjectKind::Binary, "b() is for binary sensors only");

            Measurement m;
            m.object_id = byte_id;
            m.len = l.width;
            m.data[0] = on ? 1 : 0;
            return m;
        }

    } // namespace detail
} // namespace BTHome
