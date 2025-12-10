/*
 * xncp_zbt2_commands.c
 *
 * ZBT-2 specific XNCP command handlers (LED control, accelerometer)
 */

#include "xncp_zbt2_commands.h"
#include "xncp_types.h"
#include "ws2812.h"
#include "qma6100p.h"
#include "led_effects.h"
#include "sl_i2cspm_instances.h"
#include "sl_simple_rgb_pwm_led.h"
#include "ember.h"
#include <string.h>

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------

static bool handle_set_led_state(xncp_context_t *ctx);
static bool handle_get_accelerometer(xncp_context_t *ctx);

//------------------------------------------------------------------------------
// Command table
//------------------------------------------------------------------------------

const xncp_command_def_t xncp_zbt2_commands[] = {
    {0x0F00, handle_set_led_state},
    {0x0F01, handle_get_accelerometer},
    {0, NULL}  // sentinel
};

//------------------------------------------------------------------------------
// Handlers
//------------------------------------------------------------------------------

static bool handle_set_led_state(xncp_context_t *ctx)
{
    uint16_t r, g, b;

    if (ctx->payload_length == 3) {
        r = (uint16_t)ctx->payload[0] << 8;
        g = (uint16_t)ctx->payload[1] << 8;
        b = (uint16_t)ctx->payload[2] << 8;
    } else if (ctx->payload_length == 6) {
        r = ((uint16_t)ctx->payload[0] << 8) | ctx->payload[1];
        g = ((uint16_t)ctx->payload[2] << 8) | ctx->payload[3];
        b = ((uint16_t)ctx->payload[4] << 8) | ctx->payload[5];
    } else {
        *ctx->status = EMBER_BAD_ARGUMENT;
        return true;
    }

    // Manual LED control via XNCP overrides autonomous pulsing
    led_effects_stop_all();
    sl_led_set_rgb_color(&sl_led_ws2812, r, g, b);
    sl_led_turn_on(&sl_led_ws2812.led_common);

    *ctx->status = EMBER_SUCCESS;
    return true;
}

static bool handle_get_accelerometer(xncp_context_t *ctx)
{
    float xyz[3];
    qma6100p_read_acc_xyz(sl_i2cspm_inst, xyz);

    // X
    memcpy(ctx->reply + *ctx->reply_length, (uint8_t*)&xyz[0], sizeof(float));
    *ctx->reply_length += sizeof(float);

    // Y
    memcpy(ctx->reply + *ctx->reply_length, (uint8_t*)&xyz[1], sizeof(float));
    *ctx->reply_length += sizeof(float);

    // Z
    memcpy(ctx->reply + *ctx->reply_length, (uint8_t*)&xyz[2], sizeof(float));
    *ctx->reply_length += sizeof(float);

    *ctx->status = EMBER_SUCCESS;
    return true;
}
