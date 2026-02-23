#include "sl_token_api.h"
#include "sl_token_manager.h"
#include <stdint.h>

// Wrap sl_token_manager_get_data to modify the EUI64 at the earliest possible point.
// If an old Zigbee coordinator is used as a router, its EUI64 address will remain
// unchanged and it will be unable to join its old network without breaking everything.
// To work around this issue, the router firmware uses a modified EUI64 address and
// inverts the last 6 octets, ensuring a unique address.
extern sl_status_t __real_sl_token_manager_get_data(uint32_t token, void *data, uint32_t length);

sl_status_t __wrap_sl_token_manager_get_data(uint32_t token, void *data, uint32_t length)
{
    sl_status_t status = __real_sl_token_manager_get_data(token, data, length);

    // Modify EUI64 after reading from manufacturing token
    if (token == SL_TOKEN_GET_STATIC_DEVICE_TOKEN(TOKEN_MFG_EUI_64) && status == SL_STATUS_OK) {
        uint8_t *eui64 = (uint8_t *)data;

        // EUI64 is stored little endian
        for (int i = 0; i < 6; i++) {
            eui64[i] ^= 0xFF;
        }
    }

    return status;
}
