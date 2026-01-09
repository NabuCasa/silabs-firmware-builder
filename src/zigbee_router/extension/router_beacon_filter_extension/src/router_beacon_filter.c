#include "app/framework/include/af.h"

// Wrap sli_zigbee_stack_get_stored_beacon to filter out networks with blocked EPIDs.
extern sl_status_t __real_sli_zigbee_stack_get_stored_beacon(uint8_t beacon_number,
                                                              sl_zigbee_beacon_data_t *beacon);

sl_status_t __wrap_sli_zigbee_stack_get_stored_beacon(uint8_t beacon_number,
                                                       sl_zigbee_beacon_data_t *beacon)
{
    sl_status_t status = __real_sli_zigbee_stack_get_stored_beacon(beacon_number, beacon);

    if (status == SL_STATUS_OK && beacon != NULL) {
        const uint8_t *epid = beacon->extendedPanId;

        // Itron smart meters are buggy and many are stuck in a perpetual "permitting joins"
        // state. We should not consider joining them.
        if (epid[7] == 0x00 && epid[6] == 0x07 && epid[5] == 0x81) {
            sl_zigbee_app_debug_print("Skipping Itron beacon with EPID ");
            sl_zigbee_af_print_big_endian_eui64(epid);
            sl_zigbee_app_debug_println("");
            return SL_STATUS_NOT_FOUND;
        }
    }

    return status;
}
