// ESP-IDF (Bluedroid) example: advertise a BTHome v2 payload.
//
// packet.data()/size() is a complete AD element. We prepend a Flags AD
// structure and hand the raw buffer to esp_ble_gap_config_adv_data_raw().
#include "bthome.h"

#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "nvs_flash.h"

static void gap_cb(esp_gap_ble_cb_event_t event,
                   esp_ble_gap_cb_param_t* param) {
    if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT) {
        esp_ble_adv_params_t adv_params = {};
        adv_params.adv_int_min = 0x100;  // 160 ms
        adv_params.adv_int_max = 0x100;
        adv_params.adv_type = ADV_TYPE_NONCONN_IND;  // BTHome is connectionless
        adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
        adv_params.channel_map = ADV_CHNL_ALL;
        adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
        esp_ble_gap_start_advertising(&adv_params);
    }
}

extern "C" void app_main(void) {
    nvs_flash_init();
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_ble_gap_register_callback(gap_cb);

    BTHome::Packet<31> packet;
    packet.add(BTHome::temperature(22.4f));
    packet.add(BTHome::humidity(54.3f));
    packet.add(BTHome::battery(92));

    // Flags AD (0x02,0x01,0x06) + the BTHome AD element.
    static uint8_t raw_adv[31];
    size_t n = 0;
    raw_adv[n++] = 0x02;
    raw_adv[n++] = 0x01;
    raw_adv[n++] = 0x06;
    memcpy(&raw_adv[n], packet.data(), packet.size());
    n += packet.size();

    esp_ble_gap_config_adv_data_raw(raw_adv, n);
}
