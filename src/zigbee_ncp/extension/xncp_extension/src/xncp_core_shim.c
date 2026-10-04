/*
 * Strong override for the SDK's SL_WEAK sl_zigbee_af_xncp_incoming_custom_frame_cb,
 * delegating to the Rust XNCP core (crates/ohf-xncp). As a direct source object it
 * always wins over the weak default; a strong override living in the Rust aggregate
 * archive would not be pulled under -flto -fwhole-program.
 */
#include <stdint.h>
#include "sl_status.h"

extern sl_status_t ohf_xncp_handle_frame(uint8_t message_length,
                                         uint8_t *message_payload,
                                         uint8_t *reply_payload_length,
                                         uint8_t *reply_payload);

sl_status_t sl_zigbee_af_xncp_incoming_custom_frame_cb(uint8_t message_length,
                                                       uint8_t *message_payload,
                                                       uint8_t *reply_payload_length,
                                                       uint8_t *reply_payload)
{
    return ohf_xncp_handle_frame(message_length, message_payload,
                                 reply_payload_length, reply_payload);
}
