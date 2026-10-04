/*
 * Thin C shim for the Rust Zigbee LED wrapper (crates/ohf-led-effects-zigbee).
 *
 * halCommonGetToken is a macro, so it can't be bound from Rust directly. Re-export the
 * one token read as a real function; all logic lives in Rust.
 */
#include <stdint.h>
#include "sl_token_api.h"

#ifdef STACK_TYPES_HEADER
#include "stack/include/sl_zigbee_types.h"
#else
#include "stack/include/ember-types.h"
#endif

void ohf_zigbee_stack_node_data(uint16_t *pan_id, uint8_t *channel)
{
    tokTypeStackNodeData node_data;
    halCommonGetToken(&node_data, TOKEN_STACK_NODE_DATA);
    *pan_id = node_data.panId;
    *channel = node_data.radioFreqChannel;
}
