// Zephyr example: advertise a BTHome v2 payload.
//
// serviceData() returns exactly what BT_DATA_SVC_DATA16 expects:
//   [UUID lo][UUID hi][device-info][measurements...]
#include "bthome.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>

// Built once at startup; the bytes stay valid because `packet` is static.
static BTHomePacket<31> packet;

int main(void) {
    packet.add(BTHome::temperature(22.4f));
    packet.add(BTHome::humidity(54.3f));
    packet.add(BTHome::battery(92));

    if (bt_enable(nullptr)) {
        return -1;
    }

    const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
        BT_DATA(BT_DATA_SVC_DATA16, packet.serviceData(),
                packet.serviceDataSize()),
    };

    bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), nullptr, 0);
    return 0;
}
