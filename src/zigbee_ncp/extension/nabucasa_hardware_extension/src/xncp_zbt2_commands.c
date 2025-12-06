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
#include "ember.h"
#include <string.h>

#define XNCP_CMD_SET_LED_STATE_REQ     0x0F00
#define XNCP_CMD_GET_ACCELEROMETER_REQ 0x0F01

static bool handle_set_led_state(xncp_context_t *ctx)
{
    rgb_t colors[4];

    if (ctx->payload_length == 12) {
        // Individual LED colors
        colors[0].R = ctx->payload[0];
        colors[0].G = ctx->payload[1];
        colors[0].B = ctx->payload[2];
        colors[1].R = ctx->payload[3];
        colors[1].G = ctx->payload[4];
        colors[1].B = ctx->payload[5];
        colors[2].R = ctx->payload[6];
        colors[2].G = ctx->payload[7];
        colors[2].B = ctx->payload[8];
        colors[3].R = ctx->payload[9];
        colors[3].G = ctx->payload[10];
        colors[3].B = ctx->payload[11];
    } else if (ctx->payload_length == 3) {
        // Single color for all LEDs
        for (int i = 0; i < 4; i++) {
            colors[i].R = ctx->payload[0];
            colors[i].G = ctx->payload[1];
            colors[i].B = ctx->payload[2];
        }
    } else {
        *ctx->status = EMBER_BAD_ARGUMENT;
        *ctx->response_id = XNCP_CMD_SET_LED_STATE_REQ | XNCP_CMD_RESPONSE_BIT;
        return true;
    }

    // Manual LED control via XNCP overrides autonomous pulsing
    led_effects_stop_all();
    set_color_buffer(colors);

    *ctx->status = EMBER_SUCCESS;
    *ctx->response_id = XNCP_CMD_SET_LED_STATE_REQ | XNCP_CMD_RESPONSE_BIT;
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
    *ctx->response_id = XNCP_CMD_GET_ACCELEROMETER_REQ | XNCP_CMD_RESPONSE_BIT;
    return true;
}

// Main command handler - called from generated dispatcher
bool xncp_zbt2_handle_command(xncp_context_t *ctx)
{
    switch (ctx->command_id) {
        case XNCP_CMD_SET_LED_STATE_REQ:
            return handle_set_led_state(ctx);

        case XNCP_CMD_GET_ACCELEROMETER_REQ:
            return handle_get_accelerometer(ctx);

        default:
            return false;  // Not handled
    }
}

// Return feature flags for ZBT-2 commands
uint32_t xncp_zbt2_handle_command_features(void)
{
    return XNCP_FEATURE_LED_CONTROL;
}
