#pragma once

#include <stdint.h>
// C headers (<stdint.h>/<stddef.h>/<string.h>) instead of the C++ wrappers:
// avr-gcc ships no libstdc++ wrapper headers at all, and Zephyr's minimal C++
// library lacks <cstring>. The C headers exist on every supported toolchain,
// so types and calls stay unqualified (uint8_t, memcpy - no std::).
#include <string.h>

#include "bthome_defs.h"
#include "bthome_encoding.h"

namespace BTHome
{
    // Scalar measurements
    inline Measurement packet_id(uint8_t v) { return detail::u<ObjectId::PacketId>(v); }
    inline Measurement battery(uint8_t pct) { return detail::u<ObjectId::Battery>(pct); }
    inline Measurement temperature(float c) { return detail::make_sensor<ObjectId::Temperature>(c); }
    inline Measurement humidity(float pct) { return detail::make_sensor<ObjectId::Humidity>(pct); }
    inline Measurement pressure(float hpa) { return detail::make_sensor<ObjectId::Pressure>(hpa); }
    inline Measurement illuminance(float lux) { return detail::make_sensor<ObjectId::Illuminance>(lux); }
    inline Measurement mass_kg(float kg) { return detail::make_sensor<ObjectId::MassKg>(kg); }
    inline Measurement mass_lb(float lb) { return detail::make_sensor<ObjectId::MassLb>(lb); }
    inline Measurement dewpoint(float c) { return detail::make_sensor<ObjectId::Dewpoint>(c); }
    inline Measurement count(uint8_t v) { return detail::u<ObjectId::Count>(v); }
    inline Measurement energy(float kwh) { return detail::make_sensor<ObjectId::Energy>(kwh); }
    inline Measurement power(float w) { return detail::make_sensor<ObjectId::Power>(w); }
    inline Measurement voltage(float v) { return detail::make_sensor<ObjectId::Voltage>(v); }
    inline Measurement pm2_5(float ugm3) { return detail::make_sensor<ObjectId::Pm2_5>(ugm3); }
    inline Measurement pm10(float ugm3) { return detail::make_sensor<ObjectId::Pm10>(ugm3); }
    inline Measurement co2(float ppm) { return detail::make_sensor<ObjectId::Co2>(ppm); }
    inline Measurement tvoc(float ugm3) { return detail::make_sensor<ObjectId::Tvoc>(ugm3); }
    inline Measurement moisture(float pct) { return detail::make_sensor<ObjectId::Moisture>(pct); }
    inline Measurement humidity_u8(uint8_t pct) { return detail::u<ObjectId::HumidityU8>(pct); }
    inline Measurement moisture_u8(uint8_t pct) { return detail::u<ObjectId::MoistureU8>(pct); }
    inline Measurement count_u16(uint16_t v) { return detail::u<ObjectId::CountU16>(v); }
    inline Measurement count_u32(uint32_t v) { return detail::u<ObjectId::CountU32>(v); }
    inline Measurement rotation(float deg) { return detail::make_sensor<ObjectId::Rotation>(deg); }
    inline Measurement distance_mm(float mm) { return detail::make_sensor<ObjectId::DistanceMm>(mm); }
    inline Measurement distance_m(float m) { return detail::make_sensor<ObjectId::DistanceM>(m); }
    inline Measurement duration(float s) { return detail::make_sensor<ObjectId::Duration>(s); }
    inline Measurement current(float a) { return detail::make_sensor<ObjectId::Current>(a); }
    inline Measurement speed(float mps) { return detail::make_sensor<ObjectId::Speed>(mps); }
    inline Measurement temperature_c1(float c) { return detail::make_sensor<ObjectId::TemperatureC1>(c); }
    inline Measurement uv_index(float v) { return detail::make_sensor<ObjectId::UvIndex>(v); }
    inline Measurement volume_l(float l) { return detail::make_sensor<ObjectId::VolumeL>(l); }
    inline Measurement volume_ml(float ml) { return detail::make_sensor<ObjectId::VolumeMl>(ml); }
    inline Measurement volume_flow_rate(float m3h) { return detail::make_sensor<ObjectId::VolumeFlowRate>(m3h); }
    inline Measurement voltage_c1(float v) { return detail::make_sensor<ObjectId::VoltageCenti>(v); }
    inline Measurement gas(float m3) { return detail::make_sensor<ObjectId::Gas>(m3); }
    inline Measurement gas_u32(float m3) { return detail::make_sensor<ObjectId::GasU32>(m3); }
    inline Measurement energy_u32(float kwh) { return detail::make_sensor<ObjectId::EnergyU32>(kwh); }
    inline Measurement volume_u32(float l) { return detail::make_sensor<ObjectId::VolumeU32>(l); }
    inline Measurement water(float l) { return detail::make_sensor<ObjectId::Water>(l); }
    inline Measurement timestamp(uint32_t epoch_s) { return detail::u<ObjectId::Timestamp>(epoch_s); }
    inline Measurement acceleration(float mps2) { return detail::make_sensor<ObjectId::Acceleration>(mps2); }
    inline Measurement gyroscope(float dps) { return detail::make_sensor<ObjectId::Gyroscope>(dps); }
    inline Measurement volume_storage(float l) { return detail::make_sensor<ObjectId::VolumeStorage>(l); }
    inline Measurement conductivity(float us_cm) { return detail::make_sensor<ObjectId::Conductivity>(us_cm); }
    inline Measurement temperature_s8(int8_t c) { return detail::s<ObjectId::TemperatureS8>(c); }
    inline Measurement temperature_s8_035(float c) { return detail::make_sensor<ObjectId::TemperatureS8_035>(c); }
    inline Measurement count_s8(int8_t v) { return detail::s<ObjectId::CountS8>(v); }
    inline Measurement count_s16(int16_t v) { return detail::s<ObjectId::CountS16>(v); }
    inline Measurement count_s32(int32_t v) { return detail::s<ObjectId::CountS32>(v); }
    inline Measurement power_s32(float w) { return detail::make_sensor<ObjectId::PowerS32>(w); }
    inline Measurement current_s16(float a) { return detail::make_sensor<ObjectId::CurrentS16>(a); }
    inline Measurement direction(float deg) { return detail::make_sensor<ObjectId::Direction>(deg); }
    inline Measurement precipitation(float mm) { return detail::make_sensor<ObjectId::Precipitation>(mm); }
    inline Measurement channel(uint8_t v) { return detail::u<ObjectId::Channel>(v); }
    inline Measurement rotational_speed(float rpm) { return detail::make_sensor<ObjectId::RotationalSpeed>(rpm); }
    inline Measurement speed_s32(float mps) { return detail::make_sensor<ObjectId::SpeedS32>(mps); }
    inline Measurement acceleration_s32(float mps2) { return detail::make_sensor<ObjectId::AccelerationS32>(mps2); }
    inline Measurement light_level(uint8_t v) { return detail::u<ObjectId::LightLevel>(v); }
    inline Measurement settings_revision(uint8_t v) { return detail::u<ObjectId::SettingsRevision>(v); }

    // Variable-length objects. Input longer than VarMeasurement::kMaxBytes is
    // truncated (consistent with the clamping behavior of the scalar factories).
    inline VarMeasurement text(const char *s)
    {
        VarMeasurement m;
        m.object_id = static_cast<uint8_t>(ObjectId::Text);
        if (s != nullptr)
        {
            size_t n = strlen(s);
            if (n > VarMeasurement::kMaxBytes)
            {
                n = VarMeasurement::kMaxBytes;
            }
            memcpy(m.data, s, n);
            m.len = static_cast<uint8_t>(n);
        }
        return m;
    }

    inline VarMeasurement raw(const uint8_t *bytes, size_t count)
    {
        VarMeasurement m;
        m.object_id = static_cast<uint8_t>(ObjectId::Raw);
        if (bytes != nullptr)
        {
            if (count > VarMeasurement::kMaxBytes)
            {
                count = VarMeasurement::kMaxBytes;
            }
            memcpy(m.data, bytes, count);
            m.len = static_cast<uint8_t>(count);
        }
        return m;
    }

    // Events. Receivers process an event only when the packet id changes, so
    // the same event packet may be advertised repeatedly for reliability.
    inline Measurement button_event(ButtonEventType e)
    {
        return detail::u<ObjectId::ButtonEvent>(static_cast<uint8_t>(e));
    }

    // Value layout: [argument count][opcode][arguments...] - spec examples
    // 3B0002 (toggle) and 3B010305 (step up 5 steps). The only object whose
    // length cannot come from object_layout(): it depends on the payload
    // itself, so this structure has to be known here and in the decoder.
    inline Measurement command_event(CommandEventType e, uint8_t steps = 1)
    {
        Measurement m;
        m.object_id = static_cast<uint8_t>(ObjectId::CommandEvent);
        const bool has_steps = (e == CommandEventType::StepUp) || (e == CommandEventType::StepDown);
        m.data[0] = has_steps ? 1 : 0;
        m.data[1] = static_cast<uint8_t>(e);
        m.len = 2;
        if (has_steps)
        {
            m.data[2] = steps;
            m.len = 3;
        }
        return m;
    }

    // The dimmer object is a fixed 2-value-byte object: [event][steps], the
    // steps byte is always present (spec example 3C0000 for None). Receivers
    // parse it with a fixed length - omitting the byte desynchronises them
    // and the whole advertisement gets discarded.
    inline Measurement dimmer_event(DimmerEventType e, uint8_t steps = 1)
    {
        constexpr uint8_t id = detail::oid(ObjectId::DimmerEvent);
        constexpr detail::ObjectLayout layout = detail::object_layout(id);

        Measurement m;
        m.object_id = id;
        m.data[0] = static_cast<uint8_t>(e);
        m.data[1] = (e == DimmerEventType::None) ? 0 : steps;
        m.len = layout.width;
        return m;
    }

    // Device objects
    inline Measurement device_type_id(uint16_t v) { return detail::u<ObjectId::DeviceTypeId>(v); }
    inline Measurement firmware_version_u32(uint32_t v) { return detail::u<ObjectId::FirmwareVersionU32>(v); }
    inline Measurement firmware_version_u24(uint32_t v) { return detail::u<ObjectId::FirmwareVersionU24>(v); }

    // Binary sensors
    inline Measurement generic_boolean(bool on) { return detail::b<ObjectId::GenericBoolean>(on); }
    inline Measurement power_state(bool on) { return detail::b<ObjectId::PowerState>(on); }
    inline Measurement opening(bool open) { return detail::b<ObjectId::Opening>(open); }
    inline Measurement battery_low(bool low) { return detail::b<ObjectId::BatteryLow>(low); }
    inline Measurement battery_charging(bool on) { return detail::b<ObjectId::BatteryCharging>(on); }
    inline Measurement carbon_monoxide(bool on) { return detail::b<ObjectId::CarbonMonoxide>(on); }
    inline Measurement cold(bool on) { return detail::b<ObjectId::Cold>(on); }
    inline Measurement connectivity(bool on) { return detail::b<ObjectId::Connectivity>(on); }
    inline Measurement door(bool open) { return detail::b<ObjectId::Door>(open); }
    inline Measurement garage_door(bool open) { return detail::b<ObjectId::GarageDoor>(open); }
    inline Measurement gas_detected(bool on) { return detail::b<ObjectId::GasDetected>(on); }
    inline Measurement heat(bool on) { return detail::b<ObjectId::Heat>(on); }
    inline Measurement light(bool on) { return detail::b<ObjectId::Light>(on); }
    inline Measurement lock(bool locked) { return detail::b<ObjectId::Lock>(locked); }
    inline Measurement moisture_detected(bool on) { return detail::b<ObjectId::MoistureDetected>(on); }
    inline Measurement motion(bool on) { return detail::b<ObjectId::Motion>(on); }
    inline Measurement moving(bool on) { return detail::b<ObjectId::Moving>(on); }
    inline Measurement occupancy(bool on) { return detail::b<ObjectId::Occupancy>(on); }
    inline Measurement plug(bool on) { return detail::b<ObjectId::Plug>(on); }
    inline Measurement presence(bool on) { return detail::b<ObjectId::Presence>(on); }
    inline Measurement problem(bool on) { return detail::b<ObjectId::Problem>(on); }
    inline Measurement running(bool on) { return detail::b<ObjectId::Running>(on); }
    inline Measurement safety(bool on) { return detail::b<ObjectId::Safety>(on); }
    inline Measurement smoke(bool on) { return detail::b<ObjectId::Smoke>(on); }
    inline Measurement sound(bool on) { return detail::b<ObjectId::Sound>(on); }
    inline Measurement tamper(bool on) { return detail::b<ObjectId::Tamper>(on); }
    inline Measurement vibration(bool on) { return detail::b<ObjectId::Vibration>(on); }
    inline Measurement window(bool open) { return detail::b<ObjectId::Window>(open); }

} // namespace BTHome
