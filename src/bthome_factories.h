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
    inline Measurement packet_id(uint8_t v) { return detail::u8(MiscObjectId::PacketId, v); }
    inline Measurement battery(uint8_t pct) { return detail::u8(SensorObjectId::Battery, pct); }
    inline Measurement temperature(float c) { return detail::make_sensor(SensorObjectId::Temperature, c); }
    inline Measurement humidity(float pct) { return detail::make_sensor(SensorObjectId::Humidity, pct); }
    inline Measurement pressure(float hpa) { return detail::make_sensor(SensorObjectId::Pressure, hpa); }
    inline Measurement illuminance(float lux) { return detail::make_sensor(SensorObjectId::Illuminance, lux); }
    inline Measurement mass_kg(float kg) { return detail::make_sensor(SensorObjectId::MassKg, kg); }
    inline Measurement mass_lb(float lb) { return detail::make_sensor(SensorObjectId::MassLb, lb); }
    inline Measurement dewpoint(float c) { return detail::make_sensor(SensorObjectId::Dewpoint, c); }
    inline Measurement count(uint8_t v) { return detail::u8(SensorObjectId::Count, v); }
    inline Measurement energy(float kwh) { return detail::make_sensor(SensorObjectId::Energy, kwh); }
    inline Measurement power(float w) { return detail::make_sensor(SensorObjectId::Power, w); }
    inline Measurement voltage(float v) { return detail::make_sensor(SensorObjectId::Voltage, v); }
    inline Measurement pm2_5(float ugm3) { return detail::make_sensor(SensorObjectId::Pm2_5, ugm3); }
    inline Measurement pm10(float ugm3) { return detail::make_sensor(SensorObjectId::Pm10, ugm3); }
    inline Measurement co2(float ppm) { return detail::make_sensor(SensorObjectId::Co2, ppm); }
    inline Measurement tvoc(float ugm3) { return detail::make_sensor(SensorObjectId::Tvoc, ugm3); }
    inline Measurement moisture(float pct) { return detail::make_sensor(SensorObjectId::Moisture, pct); }
    inline Measurement humidity_u8(uint8_t pct) { return detail::u8(SensorObjectId::HumidityU8, pct); }
    inline Measurement moisture_u8(uint8_t pct) { return detail::u8(SensorObjectId::MoistureU8, pct); }
    inline Measurement count_u16(uint16_t v) { return detail::u16(SensorObjectId::CountU16, v); }
    inline Measurement count_u32(uint32_t v) { return detail::u32(SensorObjectId::CountU32, v); }
    inline Measurement rotation(float deg) { return detail::make_sensor(SensorObjectId::Rotation, deg); }
    inline Measurement distance_mm(float mm) { return detail::make_sensor(SensorObjectId::DistanceMm, mm); }
    inline Measurement distance_m(float m) { return detail::make_sensor(SensorObjectId::DistanceM, m); }
    inline Measurement duration(float s) { return detail::make_sensor(SensorObjectId::Duration, s); }
    inline Measurement current(float a) { return detail::make_sensor(SensorObjectId::Current, a); }
    inline Measurement speed(float mps) { return detail::make_sensor(SensorObjectId::Speed, mps); }
    inline Measurement temperature_c1(float c) { return detail::make_sensor(SensorObjectId::TemperatureC1, c); }
    inline Measurement uv_index(float v) { return detail::make_sensor(SensorObjectId::UvIndex, v); }
    inline Measurement volume_l(float l) { return detail::make_sensor(SensorObjectId::VolumeL, l); }
    inline Measurement volume_ml(float ml) { return detail::make_sensor(SensorObjectId::VolumeMl, ml); }
    inline Measurement volume_flow_rate(float m3h) { return detail::make_sensor(SensorObjectId::VolumeFlowRate, m3h); }
    inline Measurement voltage_c1(float v) { return detail::make_sensor(SensorObjectId::VoltageCenti, v); }
    inline Measurement gas(float m3) { return detail::make_sensor(SensorObjectId::Gas, m3); }
    inline Measurement gas_u32(float m3) { return detail::make_sensor(SensorObjectId::GasU32, m3); }
    inline Measurement energy_u32(float kwh) { return detail::make_sensor(SensorObjectId::EnergyU32, kwh); }
    inline Measurement volume_u32(float l) { return detail::make_sensor(SensorObjectId::VolumeU32, l); }
    inline Measurement water(float l) { return detail::make_sensor(SensorObjectId::Water, l); }
    inline Measurement timestamp(uint32_t epoch_s) { return detail::u32(SensorObjectId::Timestamp, epoch_s); }
    inline Measurement acceleration(float mps2) { return detail::make_sensor(SensorObjectId::Acceleration, mps2); }
    inline Measurement gyroscope(float dps) { return detail::make_sensor(SensorObjectId::Gyroscope, dps); }
    inline Measurement volume_storage(float l) { return detail::make_sensor(SensorObjectId::VolumeStorage, l); }
    inline Measurement conductivity(float us_cm) { return detail::make_sensor(SensorObjectId::Conductivity, us_cm); }
    inline Measurement temperature_s8(int8_t c) { return detail::i8(SensorObjectId::TemperatureS8, c); }
    inline Measurement temperature_s8_035(float c) { return detail::make_sensor(SensorObjectId::TemperatureS8_035, c); }
    inline Measurement count_s8(int8_t v) { return detail::i8(SensorObjectId::CountS8, v); }
    inline Measurement count_s16(int16_t v) { return detail::i16(SensorObjectId::CountS16, v); }
    inline Measurement count_s32(int32_t v) { return detail::i32(SensorObjectId::CountS32, v); }
    inline Measurement power_s32(float w) { return detail::make_sensor(SensorObjectId::PowerS32, w); }
    inline Measurement current_s16(float a) { return detail::make_sensor(SensorObjectId::CurrentS16, a); }
    inline Measurement direction(float deg) { return detail::make_sensor(SensorObjectId::Direction, deg); }
    inline Measurement precipitation(float mm) { return detail::make_sensor(SensorObjectId::Precipitation, mm); }
    inline Measurement channel(uint8_t v) { return detail::u8(SensorObjectId::Channel, v); }
    inline Measurement rotational_speed(float rpm) { return detail::make_sensor(SensorObjectId::RotationalSpeed, rpm); }
    inline Measurement speed_s32(float mps) { return detail::make_sensor(SensorObjectId::SpeedS32, mps); }
    inline Measurement acceleration_s32(float mps2) { return detail::make_sensor(SensorObjectId::AccelerationS32, mps2); }
    inline Measurement light_level(uint8_t v) { return detail::u8(SensorObjectId::LightLevel, v); }
    inline Measurement settings_revision(uint8_t v) { return detail::u8(SensorObjectId::SettingsRevision, v); }

    // Variable-length objects. Input longer than VarMeasurement::kMaxBytes is
    // truncated (consistent with the clamping behavior of the scalar factories).
    inline VarMeasurement text(const char *s)
    {
        VarMeasurement m;
        m.object_id = static_cast<uint8_t>(SensorObjectId::Text);
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
        m.object_id = static_cast<uint8_t>(SensorObjectId::Raw);
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
        return detail::u8(EventObjectId::ButtonEvent, static_cast<uint8_t>(e));
    }

    // Value layout: [argument count][opcode][arguments...] - spec examples
    // 3B0002 (toggle) and 3B010305 (step up 5 steps).
    inline Measurement command_event(CommandEventType e, uint8_t steps = 1)
    {
        Measurement m;
        m.object_id = static_cast<uint8_t>(EventObjectId::CommandEvent);
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
        Measurement m;
        m.object_id = static_cast<uint8_t>(EventObjectId::DimmerEvent);
        m.data[0] = static_cast<uint8_t>(e);
        m.data[1] = (e == DimmerEventType::None) ? 0 : steps;
        m.len = 2;
        return m;
    }

    // Device objects
    inline Measurement device_type_id(uint16_t v) { return detail::u16(DeviceObjectId::DeviceTypeId, v); }
    inline Measurement firmware_version_u32(uint32_t v) { return detail::u32(DeviceObjectId::FirmwareVersionU32, v); }
    inline Measurement firmware_version_u24(uint32_t v) { return detail::make_u(static_cast<uint8_t>(DeviceObjectId::FirmwareVersionU24), v, 3); }

    // Binary sensors
    inline Measurement generic_boolean(bool on) { return detail::b(BinaryObjectId::GenericBoolean, on); }
    inline Measurement power_state(bool on) { return detail::b(BinaryObjectId::PowerState, on); }
    inline Measurement opening(bool open) { return detail::b(BinaryObjectId::Opening, open); }
    inline Measurement battery_low(bool low) { return detail::b(BinaryObjectId::BatteryLow, low); }
    inline Measurement battery_charging(bool on) { return detail::b(BinaryObjectId::BatteryCharging, on); }
    inline Measurement carbon_monoxide(bool on) { return detail::b(BinaryObjectId::CarbonMonoxide, on); }
    inline Measurement cold(bool on) { return detail::b(BinaryObjectId::Cold, on); }
    inline Measurement connectivity(bool on) { return detail::b(BinaryObjectId::Connectivity, on); }
    inline Measurement door(bool open) { return detail::b(BinaryObjectId::Door, open); }
    inline Measurement garage_door(bool open) { return detail::b(BinaryObjectId::GarageDoor, open); }
    inline Measurement gas_detected(bool on) { return detail::b(BinaryObjectId::GasDetected, on); }
    inline Measurement heat(bool on) { return detail::b(BinaryObjectId::Heat, on); }
    inline Measurement light(bool on) { return detail::b(BinaryObjectId::Light, on); }
    inline Measurement lock(bool locked) { return detail::b(BinaryObjectId::Lock, locked); }
    inline Measurement moisture_detected(bool on) { return detail::b(BinaryObjectId::MoistureDetected, on); }
    inline Measurement motion(bool on) { return detail::b(BinaryObjectId::Motion, on); }
    inline Measurement moving(bool on) { return detail::b(BinaryObjectId::Moving, on); }
    inline Measurement occupancy(bool on) { return detail::b(BinaryObjectId::Occupancy, on); }
    inline Measurement plug(bool on) { return detail::b(BinaryObjectId::Plug, on); }
    inline Measurement presence(bool on) { return detail::b(BinaryObjectId::Presence, on); }
    inline Measurement problem(bool on) { return detail::b(BinaryObjectId::Problem, on); }
    inline Measurement running(bool on) { return detail::b(BinaryObjectId::Running, on); }
    inline Measurement safety(bool on) { return detail::b(BinaryObjectId::Safety, on); }
    inline Measurement smoke(bool on) { return detail::b(BinaryObjectId::Smoke, on); }
    inline Measurement sound(bool on) { return detail::b(BinaryObjectId::Sound, on); }
    inline Measurement tamper(bool on) { return detail::b(BinaryObjectId::Tamper, on); }
    inline Measurement vibration(bool on) { return detail::b(BinaryObjectId::Vibration, on); }
    inline Measurement window(bool open) { return detail::b(BinaryObjectId::Window, open); }

} // namespace BTHome
