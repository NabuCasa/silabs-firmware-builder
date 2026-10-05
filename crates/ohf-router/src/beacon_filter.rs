//! Drop beacons from networks we must not join.
use crate::bindings::{sl_status_t, sl_zigbee_beacon_data_t, SL_STATUS_NOT_FOUND, SL_STATUS_OK};

extern "C" {
    fn __real_sli_zigbee_stack_get_stored_beacon(
        beacon_number: u8,
        beacon: *mut sl_zigbee_beacon_data_t,
    ) -> sl_status_t;
}

#[no_mangle]
pub unsafe extern "C" fn __wrap_sli_zigbee_stack_get_stored_beacon(
    beacon_number: u8,
    beacon: *mut sl_zigbee_beacon_data_t,
) -> sl_status_t {
    let status = __real_sli_zigbee_stack_get_stored_beacon(beacon_number, beacon);

    if status == SL_STATUS_OK {
        if let Some(beacon) = beacon.as_ref() {
            // Itron smart meters are buggy and often stuck "permitting joins"; never join
            // them. Their EPID (little-endian) has high bytes 00 07 81.
            let epid = beacon.extendedPanId;
            if epid[7] == 0x00 && epid[6] == 0x07 && epid[5] == 0x81 {
                return SL_STATUS_NOT_FOUND;
            }
        }
    }
    status
}
