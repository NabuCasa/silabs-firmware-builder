/*
 * Generic C wrappers exposing real symbols for __STATIC_INLINE SDK helpers that
 * bindgen cannot bind. Compiled into ohf-sys by build.rs. Keep these one-liners that
 * only re-export an SDK call; anything more belongs in Rust.
 */
#include <stdint.h>
#include "em_gpio.h"

void ohf_gpio_pin_mode_set(uint32_t port, uint32_t pin, uint32_t mode, uint32_t out)
{
    GPIO_PinModeSet((GPIO_Port_TypeDef)port, pin, (GPIO_Mode_TypeDef)mode, out);
}

/* Zigbee stack node-data token read. halCommonGetToken is a macro, so bindgen can't bind
 * it. Guarded on the zigbee stack being present (STACK_TYPES_HEADER is only defined in
 * zigbee builds): ohf-sys is also compiled for the non-zigbee RCP, where these token types
 * don't exist. */
#ifdef STACK_TYPES_HEADER
#include "sl_token_api.h"
#include STACK_TYPES_HEADER

void ohf_zigbee_stack_node_data(uint16_t *pan_id, uint8_t *channel)
{
    tokTypeStackNodeData node_data;
    halCommonGetToken(&node_data, TOKEN_STACK_NODE_DATA);
    *pan_id = node_data.panId;
    *channel = node_data.radioFreqChannel;
}
#endif
