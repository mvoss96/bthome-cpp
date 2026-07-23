// Host-side factory coverage test: every factory is checked against the BTHome
// v2 object table (https://bthome.io/format/) — object id, payload width and
// little-endian value bytes for one representative input each.
// Build & run from project root:
//   g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -I .\src .\tests\test_factories.cpp -o .\build\test_factories.exe
//   .\build\test_factories.exe
#include "bthome.h"
#include "test_utils.h"

struct Case
{
    const char *name;
    BTHome::Measurement m;
    uint8_t want_id;
    uint8_t want_len;
    uint8_t want_bytes[6];
};

static void check_case(const Case &c)
{
    const bool id_ok = c.m.object_id == c.want_id;
    const bool len_ok = c.m.len == c.want_len;
    const bool bytes_ok = memcmp(c.m.data, c.want_bytes, c.want_len) == 0;
    const bool ok = id_ok && len_ok && bytes_ok;

    printf("[%s] %s", ok ? "PASS" : "FAIL", c.name);
    if (!ok)
    {
        printf("  got id=%02X len=%u bytes=", c.m.object_id, c.m.len);
        for (uint8_t i = 0; i < c.m.len; ++i)
        {
            printf("%02X ", c.m.data[i]);
        }
        printf(" want id=%02X len=%u bytes=", c.want_id, c.want_len);
        for (uint8_t i = 0; i < c.want_len; ++i)
        {
            printf("%02X ", c.want_bytes[i]);
        }
        ++g_failures;
    }
    printf("\n");
}

int main()
{
    // Scalar sensor factories. Values chosen from the BTHome spec examples
    // where available; raw = round(value / factor), little-endian.
    const Case scalar_cases[] = {
        {"packet_id(7)", BTHome::packet_id(7), 0x00, 1, {0x07}},
        {"battery(87)", BTHome::battery(87), 0x01, 1, {0x57}},
        {"temperature(21.53)", BTHome::temperature(21.53f), 0x02, 2, {0x69, 0x08}},
        {"humidity(54.2)", BTHome::humidity(54.2f), 0x03, 2, {0x2C, 0x15}},
        {"pressure(1013.25)", BTHome::pressure(1013.25f), 0x04, 3, {0xCD, 0x8B, 0x01}},
        {"illuminance(13460.67)", BTHome::illuminance(13460.67f), 0x05, 3, {0x13, 0x8A, 0x14}},
        {"mass_kg(80.3)", BTHome::mass_kg(80.3f), 0x06, 2, {0x5E, 0x1F}},
        {"mass_lb(71.42)", BTHome::mass_lb(71.42f), 0x07, 2, {0xE6, 0x1B}},
        {"dewpoint(17.38)", BTHome::dewpoint(17.38f), 0x08, 2, {0xCA, 0x06}},
        {"count(96)", BTHome::count(96), 0x09, 1, {0x60}},
        {"energy(12.345)", BTHome::energy(12.345f), 0x0A, 3, {0x39, 0x30, 0x00}},
        {"power(69.14)", BTHome::power(69.14f), 0x0B, 3, {0x02, 0x1B, 0x00}},
        {"voltage(3.074)", BTHome::voltage(3.074f), 0x0C, 2, {0x02, 0x0C}},
        {"pm2_5(561)", BTHome::pm2_5(561.0f), 0x0D, 2, {0x31, 0x02}},
        {"pm10(1188)", BTHome::pm10(1188.0f), 0x0E, 2, {0xA4, 0x04}},
        {"co2(1250)", BTHome::co2(1250.0f), 0x12, 2, {0xE2, 0x04}},
        {"tvoc(307)", BTHome::tvoc(307.0f), 0x13, 2, {0x33, 0x01}},
        {"moisture(30.74)", BTHome::moisture(30.74f), 0x14, 2, {0x02, 0x0C}},
        {"humidity_u8(35)", BTHome::humidity_u8(35), 0x2E, 1, {0x23}},
        {"moisture_u8(30)", BTHome::moisture_u8(30), 0x2F, 1, {0x1E}},
        {"count_u16(4884)", BTHome::count_u16(4884), 0x3D, 2, {0x14, 0x13}},
        {"count_u32(300000)", BTHome::count_u32(300000), 0x3E, 4, {0xE0, 0x93, 0x04, 0x00}},
        {"rotation(307.4)", BTHome::rotation(307.4f), 0x3F, 2, {0x02, 0x0C}},
        {"distance_mm(12)", BTHome::distance_mm(12.0f), 0x40, 2, {0x0C, 0x00}},
        {"distance_m(7.8)", BTHome::distance_m(7.8f), 0x41, 2, {0x4E, 0x00}},
        {"duration(13.39)", BTHome::duration(13.39f), 0x42, 3, {0x4E, 0x34, 0x00}},
        {"current(13.39)", BTHome::current(13.39f), 0x43, 2, {0x4E, 0x34}},
        {"speed(133.9)", BTHome::speed(133.9f), 0x44, 2, {0x4E, 0x34}},
        {"temperature_c1(25.1)", BTHome::temperature_c1(25.1f), 0x45, 2, {0xFB, 0x00}},
        {"uv_index(5.0)", BTHome::uv_index(5.0f), 0x46, 1, {0x32}},
        {"volume_l(2215.1)", BTHome::volume_l(2215.1f), 0x47, 2, {0x87, 0x56}},
        {"volume_ml(34780)", BTHome::volume_ml(34780.0f), 0x48, 2, {0xDC, 0x87}},
        {"volume_flow_rate(2.022)", BTHome::volume_flow_rate(2.022f), 0x49, 2, {0xE6, 0x07}},
        {"voltage_c1(23.1)", BTHome::voltage_c1(23.1f), 0x4A, 2, {0xE7, 0x00}},
        {"gas(12.345)", BTHome::gas(12.345f), 0x4B, 3, {0x39, 0x30, 0x00}},
        {"gas_u32(12.345)", BTHome::gas_u32(12.345f), 0x4C, 4, {0x39, 0x30, 0x00, 0x00}},
        {"energy_u32(12.345)", BTHome::energy_u32(12.345f), 0x4D, 4, {0x39, 0x30, 0x00, 0x00}},
        {"volume_u32(12.345)", BTHome::volume_u32(12.345f), 0x4E, 4, {0x39, 0x30, 0x00, 0x00}},
        {"water(12.345)", BTHome::water(12.345f), 0x4F, 4, {0x39, 0x30, 0x00, 0x00}},
        {"timestamp(1684093277)", BTHome::timestamp(1684093277u), 0x50, 4, {0x5D, 0x39, 0x61, 0x64}},
        {"acceleration(22.151)", BTHome::acceleration(22.151f), 0x51, 2, {0x87, 0x56}},
        {"gyroscope(22.151)", BTHome::gyroscope(22.151f), 0x52, 2, {0x87, 0x56}},
        {"volume_storage(12.345)", BTHome::volume_storage(12.345f), 0x55, 4, {0x39, 0x30, 0x00, 0x00}},
        {"conductivity(3120)", BTHome::conductivity(3120.0f), 0x56, 2, {0x30, 0x0C}},
        {"temperature_s8(25)", BTHome::temperature_s8(25), 0x57, 1, {0x19}},
        {"temperature_s8_035(8.05)", BTHome::temperature_s8_035(8.05f), 0x58, 1, {0x17}},
        {"count_s8(-25)", BTHome::count_s8(-25), 0x59, 1, {0xE7}},
        {"count_s16(-22)", BTHome::count_s16(-22), 0x5A, 2, {0xEA, 0xFF}},
        {"count_s32(-105)", BTHome::count_s32(-105), 0x5B, 4, {0x97, 0xFF, 0xFF, 0xFF}},
        {"power_s32(-69.14)", BTHome::power_s32(-69.14f), 0x5C, 4, {0xFE, 0xE4, 0xFF, 0xFF}},
        // Spec: 0x5D is sint16 (that is its whole point vs. 0x43 current/uint16).
        {"current_s16(-1.234)", BTHome::current_s16(-1.234f), 0x5D, 2, {0x2E, 0xFB}},
        {"direction(90.0)", BTHome::direction(90.0f), 0x5E, 2, {0x28, 0x23}},
        {"precipitation(12.5)", BTHome::precipitation(12.5f), 0x5F, 2, {0x7D, 0x00}},
        {"channel(5)", BTHome::channel(5), 0x60, 1, {0x05}},
        {"rotational_speed(1500)", BTHome::rotational_speed(1500.0f), 0x61, 2, {0xDC, 0x05}},
        {"speed_s32(1.0)", BTHome::speed_s32(1.0f), 0x62, 4, {0x40, 0x42, 0x0F, 0x00}},
        {"acceleration_s32(-1.0)", BTHome::acceleration_s32(-1.0f), 0x63, 4, {0xC0, 0xBD, 0xF0, 0xFF}},
        {"light_level(200)", BTHome::light_level(200), 0x64, 1, {0xC8}},
        {"settings_revision(3)", BTHome::settings_revision(3), 0x65, 1, {0x03}},
        {"device_type_id(1)", BTHome::device_type_id(1), 0xF0, 2, {0x01, 0x00}},
        {"firmware_version_u32(0x04020100)", BTHome::firmware_version_u32(0x04020100u), 0xF1, 4, {0x00, 0x01, 0x02, 0x04}},
        {"firmware_version_u24(0x060100)", BTHome::firmware_version_u24(0x060100u), 0xF2, 3, {0x00, 0x01, 0x06}},
    };

    // Binary sensor factories: id per spec table, payload 0x01 (true) / 0x00 (false).
    const Case binary_cases[] = {
        {"generic_boolean(true)", BTHome::generic_boolean(true), 0x0F, 1, {0x01}},
        {"power_state(true)", BTHome::power_state(true), 0x10, 1, {0x01}},
        {"opening(false)", BTHome::opening(false), 0x11, 1, {0x00}},
        {"battery_low(true)", BTHome::battery_low(true), 0x15, 1, {0x01}},
        {"battery_charging(false)", BTHome::battery_charging(false), 0x16, 1, {0x00}},
        {"carbon_monoxide(true)", BTHome::carbon_monoxide(true), 0x17, 1, {0x01}},
        {"cold(false)", BTHome::cold(false), 0x18, 1, {0x00}},
        {"connectivity(true)", BTHome::connectivity(true), 0x19, 1, {0x01}},
        {"door(false)", BTHome::door(false), 0x1A, 1, {0x00}},
        {"garage_door(true)", BTHome::garage_door(true), 0x1B, 1, {0x01}},
        {"gas_detected(false)", BTHome::gas_detected(false), 0x1C, 1, {0x00}},
        {"heat(true)", BTHome::heat(true), 0x1D, 1, {0x01}},
        {"light(false)", BTHome::light(false), 0x1E, 1, {0x00}},
        {"lock(true)", BTHome::lock(true), 0x1F, 1, {0x01}},
        {"moisture_detected(false)", BTHome::moisture_detected(false), 0x20, 1, {0x00}},
        {"motion(true)", BTHome::motion(true), 0x21, 1, {0x01}},
        {"moving(false)", BTHome::moving(false), 0x22, 1, {0x00}},
        {"occupancy(true)", BTHome::occupancy(true), 0x23, 1, {0x01}},
        {"plug(false)", BTHome::plug(false), 0x24, 1, {0x00}},
        {"presence(true)", BTHome::presence(true), 0x25, 1, {0x01}},
        {"problem(false)", BTHome::problem(false), 0x26, 1, {0x00}},
        {"running(true)", BTHome::running(true), 0x27, 1, {0x01}},
        {"safety(false)", BTHome::safety(false), 0x28, 1, {0x00}},
        {"smoke(true)", BTHome::smoke(true), 0x29, 1, {0x01}},
        {"sound(false)", BTHome::sound(false), 0x2A, 1, {0x00}},
        {"tamper(true)", BTHome::tamper(true), 0x2B, 1, {0x01}},
        {"vibration(false)", BTHome::vibration(false), 0x2C, 1, {0x00}},
        {"window(true)", BTHome::window(true), 0x2D, 1, {0x01}},
    };

    // Event factories: button = 1 event byte; dimmer = event byte plus a step
    // count for rotate events (None stays 1 byte, per spec example 3C00).
    const Case event_cases[] = {
        {"button_event(none)", BTHome::button_event(BTHome::ButtonEventType::None), 0x3A, 1, {0x00}},
        {"button_event(press)", BTHome::button_event(BTHome::ButtonEventType::Press), 0x3A, 1, {0x01}},
        {"button_event(double_press)", BTHome::button_event(BTHome::ButtonEventType::DoublePress), 0x3A, 1, {0x02}},
        {"button_event(triple_press)", BTHome::button_event(BTHome::ButtonEventType::TriplePress), 0x3A, 1, {0x03}},
        {"button_event(long_press)", BTHome::button_event(BTHome::ButtonEventType::LongPress), 0x3A, 1, {0x04}},
        {"button_event(long_double_press)", BTHome::button_event(BTHome::ButtonEventType::LongDoublePress), 0x3A, 1, {0x05}},
        {"button_event(long_triple_press)", BTHome::button_event(BTHome::ButtonEventType::LongTriplePress), 0x3A, 1, {0x06}},
        {"button_event(hold_press)", BTHome::button_event(BTHome::ButtonEventType::HoldPress), 0x3A, 1, {0x80}},
        {"command_event(off)", BTHome::command_event(BTHome::CommandEventType::Off), 0x3B, 2, {0x00, 0x00}},
        {"command_event(on)", BTHome::command_event(BTHome::CommandEventType::On), 0x3B, 2, {0x00, 0x01}},
        {"command_event(toggle)", BTHome::command_event(BTHome::CommandEventType::Toggle), 0x3B, 2, {0x00, 0x02}},
        {"command_event(step_up, 5)", BTHome::command_event(BTHome::CommandEventType::StepUp, 5), 0x3B, 3, {0x01, 0x03, 0x05}},
        {"command_event(step_down, 5)", BTHome::command_event(BTHome::CommandEventType::StepDown, 5), 0x3B, 3, {0x01, 0x04, 0x05}},
        {"dimmer_event(none)", BTHome::dimmer_event(BTHome::DimmerEventType::None), 0x3C, 1, {0x00}},
        {"dimmer_event(rotate_left, 3)", BTHome::dimmer_event(BTHome::DimmerEventType::RotateLeft, 3), 0x3C, 2, {0x01, 0x03}},
        {"dimmer_event(rotate_right, 10)", BTHome::dimmer_event(BTHome::DimmerEventType::RotateRight, 10), 0x3C, 2, {0x02, 0x0A}},
    };

    for (const Case &c : scalar_cases)
    {
        check_case(c);
    }
    for (const Case &c : binary_cases)
    {
        check_case(c);
    }
    for (const Case &c : event_cases)
    {
        check_case(c);
    }

    // Completeness guard: update these counts when adding factories, so a new
    // factory without a test case fails loudly here.
    const size_t n_scalar = sizeof(scalar_cases) / sizeof(scalar_cases[0]);
    const size_t n_binary = sizeof(binary_cases) / sizeof(binary_cases[0]);
    const size_t n_event = sizeof(event_cases) / sizeof(event_cases[0]);
    const bool counts_ok = (n_scalar == 62) && (n_binary == 28) && (n_event == 16);
    printf("[%s] factory case counts (%zu scalar, %zu binary, %zu event)\n",
                counts_ok ? "PASS" : "FAIL", n_scalar, n_binary, n_event);
    if (!counts_ok)
    {
        ++g_failures;
    }

    return test_summary();
}
