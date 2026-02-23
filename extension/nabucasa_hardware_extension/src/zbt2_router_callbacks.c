/*
 * zbt2_router_callbacks.c
 *
 * ZBT-2 Router Callbacks Implementation
 * Handles Zigbee router callbacks with LED feedback for ZBT-2 hardware
 */

#include <math.h>

#include "zbt2_router_callbacks.h"
#include "led_effects.h"
#include "led_manager.h"
#include "ws2812.h"

#include "app/framework/include/af.h"
#include "network-steering.h"
#include "find-and-bind-target.h"
#include "sl_token_api.h"

#define LIGHT_ENDPOINT  1
#define COMMISSIONING_RETRY_DELAY_MS  5000

static sl_zigbee_af_event_t commissioning_retry_event;


// Let the compiler go wild with optimizations for these floating point operations.
// Accuracy is not critical here.
#pragma GCC push_options
#pragma GCC optimize ("Ofast")

static inline float clampf(float value, float min_val, float max_val)
{
    return value < min_val ? min_val : (value > max_val ? max_val : value);
}

static rgb_t xy_to_rgb(uint8_t level, uint16_t current_x, uint16_t current_y)
{
    rgb_t rgb = {0, 0, 0};

    // Avoid division by zero
    if (current_y == 0) {
        return rgb;
    }

    float x = (float)current_x / 65535.0f;
    float y = (float)current_y / 65535.0f;
    float z = 1.0f - x - y;

    // Y is brightness (level 0-254 -> 0.0-1.0)
    float Y = (float)level / 254.0f;
    float X = (Y / y) * x;
    float Z = (Y / y) * z;

    // XYZ to sRGB (D65 illuminant)
    float r = (X * 3.2410f) - (Y * 1.5374f) - (Z * 0.4986f);
    float g = -(X * 0.9692f) + (Y * 1.8760f) + (Z * 0.0416f);
    float b = (X * 0.0556f) - (Y * 0.2040f) + (Z * 1.0570f);

    // Gamma 2.2 correction (sRGB)
    r = (r <= 0.00304f) ? (12.92f * r) : (1.055f * powf(r, 1.0f / 2.4f) - 0.055f);
    g = (g <= 0.00304f) ? (12.92f * g) : (1.055f * powf(g, 1.0f / 2.4f) - 0.055f);
    b = (b <= 0.00304f) ? (12.92f * b) : (1.055f * powf(b, 1.0f / 2.4f) - 0.055f);

    // Clamp and scale to 0-65535
    rgb.r = (uint16_t)(clampf(r, 0.0f, 1.0f) * 65535.0f);
    rgb.g = (uint16_t)(clampf(g, 0.0f, 1.0f) * 65535.0f);
    rgb.b = (uint16_t)(clampf(b, 0.0f, 1.0f) * 65535.0f);

    return rgb;
}

// Color temperature (mireds) to sRGB conversion
// Based on Tanner Helland algorithm
static rgb_t color_temp_to_rgb(uint8_t level, uint16_t mireds)
{
    rgb_t rgb = {0, 0, 0};

    // Convert mireds to Kelvin, clamp to valid range (153-625 mireds = 6535K-1600K)
    float kelvin = 1000000.0f / (float)mireds;
    kelvin = clampf(kelvin, 1600.0f, 6535.0f);
    float temp = kelvin / 100.0f;

    float red, green, blue;

    if (temp <= 66.0f) {
        red = 255.0f;
    } else {
        red = 329.698727446f * powf(temp - 60.0f, -0.1332047592f);
    }

    if (temp <= 66.0f) {
        green = 99.4708025861f * logf(temp) - 161.1195681661f;
    } else {
        green = 288.1221695283f * powf(temp - 60.0f, -0.0755148492f);
    }

    if (temp >= 66.0f) {
        blue = 255.0f;
    } else if (temp <= 19.0f) {
        blue = 0.0f;
    } else {
        blue = 138.5177312231f * logf(temp - 10.0f) - 305.0447927307f;
    }

    float brightness = (float)level / 254.0f;
    rgb.r = (uint16_t)(clampf(red, 0.0f, 255.0f) * 257.0f * brightness);
    rgb.g = (uint16_t)(clampf(green, 0.0f, 255.0f) * 257.0f * brightness);
    rgb.b = (uint16_t)(clampf(blue, 0.0f, 255.0f) * 257.0f * brightness);

    return rgb;
}
#pragma GCC pop_options  // restore optimization level

bool device_has_stored_network_settings(void)
{
  tokTypeStackNodeData nodeData;
  halCommonGetToken(&nodeData, TOKEN_STACK_NODE_DATA);

  if (nodeData.panId == 0xFFFF) {
    return false;
  }

  if (nodeData.radioFreqChannel < 11 || nodeData.radioFreqChannel > 26) {
    return false;
  }

  return true;
}

static void sync_light_state(uint8_t endpoint)
{
  bool on_off = false;
  sl_zigbee_af_read_server_attribute(endpoint,
                                     ZCL_ON_OFF_CLUSTER_ID,
                                     ZCL_ON_OFF_ATTRIBUTE_ID,
                                     (uint8_t *)&on_off,
                                     sizeof(on_off));

  if (!on_off) {
    led_manager_clear_pattern(LED_PRIORITY_MANUAL);
    return;
  }

  uint8_t level = 0;
  sl_zigbee_af_read_server_attribute(endpoint,
                                     ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                     ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                     (uint8_t *)&level,
                                     sizeof(level));

  if (level == 0) {
    level = 1;
  }

  uint8_t color_mode = 0x01;
  sl_zigbee_af_read_server_attribute(endpoint,
                                     ZCL_COLOR_CONTROL_CLUSTER_ID,
                                     ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID,
                                     (uint8_t *)&color_mode,
                                     sizeof(color_mode));

  rgb_t rgb;

  if (color_mode == 0x02) {
    uint16_t color_temp = 0;
    sl_zigbee_af_read_server_attribute(endpoint,
                                       ZCL_COLOR_CONTROL_CLUSTER_ID,
                                       ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                                       (uint8_t *)&color_temp,
                                       sizeof(color_temp));
    rgb = color_temp_to_rgb(level, color_temp);
  } else {
    uint16_t color_x = 0x0000;
    uint16_t color_y = 0x0000;

    sl_zigbee_af_read_server_attribute(endpoint,
                                       ZCL_COLOR_CONTROL_CLUSTER_ID,
                                       ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID,
                                       (uint8_t *)&color_x,
                                       sizeof(color_x));
    sl_zigbee_af_read_server_attribute(endpoint,
                                       ZCL_COLOR_CONTROL_CLUSTER_ID,
                                       ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID,
                                       (uint8_t *)&color_y,
                                       sizeof(color_y));
    rgb = xy_to_rgb(level, color_x, color_y);
  }

  led_manager_set_color(LED_PRIORITY_MANUAL, rgb);
}

// Commissioning event handler - starts network steering
static void commissioning_retry_event_handler(sl_zigbee_af_event_t *event)
{
  if (sl_zigbee_af_network_state() != SL_ZIGBEE_JOINED_NETWORK) {
    // Switch from white pulse (ready) to blue pulse (searching)
    led_pattern_t search_pattern = {
        .mode = LED_MODE_PULSE,
        .color = LED_COLOR_SEARCH_BLUE,
        .period_ms = 2760,
        .duration_ms = 0
    };
    led_manager_set_pattern(LED_PRIORITY_BACKGROUND, &search_pattern);

    sl_status_t status = sl_zigbee_af_network_steering_start();
    sl_zigbee_app_debug_println("%s network %s: 0x%X", "Join", "start", status);
  }
}

void zbt2_router_init(uint8_t init_level)
{
  (void)init_level;

  led_effects_init();
  led_effects_set_network_state(device_has_stored_network_settings());

  sl_zigbee_af_event_init(&commissioning_retry_event, commissioning_retry_event_handler);

  if (!device_has_stored_network_settings()) {
    sl_zigbee_af_event_set_active(&commissioning_retry_event);
  }
}

void zbt2_router_stack_status_cb(sl_status_t status)
{
  if (status == SL_STATUS_NETWORK_DOWN) {
    led_effects_set_network_state(false);
    led_manager_clear_pattern(LED_PRIORITY_MANUAL);
  } else if (status == SL_STATUS_NETWORK_UP) {
    // Stop pulsing - we're on a network
    led_effects_set_network_state(true);

    led_pattern_t join_pattern = {
        .mode = LED_MODE_STATIC,
        .color = LED_COLOR_JOIN_GREEN,
        .duration_ms = 2000
    };
    led_manager_set_pattern(LED_PRIORITY_NOTIFICATION, &join_pattern);

    // Ensure bulb state is active on MANUAL layer, which will take effect once the
    // above effect is done
    sync_light_state(LIGHT_ENDPOINT);
  }
}

void sl_zigbee_af_network_steering_complete_cb(sl_status_t status,
                                               uint8_t totalBeacons,
                                               uint8_t joinAttempts,
                                               uint8_t finalState)
{
  sl_zigbee_app_debug_println("Network steering complete: 0x%X", status);

  if (status != SL_STATUS_OK) {
    led_effects_set_network_state(false);
    led_manager_clear_pattern(LED_PRIORITY_NOTIFICATION);

    // Retry steering after 5 seconds
    sl_zigbee_af_event_set_delay_ms(&commissioning_retry_event, COMMISSIONING_RETRY_DELAY_MS);
  }
}

void sl_zigbee_af_post_attribute_change_cb(uint8_t endpoint,
                                           sl_zigbee_af_cluster_id_t clusterId,
                                           sl_zigbee_af_attribute_id_t attributeId,
                                           uint8_t mask,
                                           uint16_t manufacturerCode,
                                           uint8_t type,
                                           uint8_t size,
                                           uint8_t* value)
{
  if (mask != CLUSTER_MASK_SERVER) {
    return;
  }

  // Re-sync LED on any relevant attribute change
  bool should_sync = false;

  if (clusterId == ZCL_ON_OFF_CLUSTER_ID && attributeId == ZCL_ON_OFF_ATTRIBUTE_ID) {
    should_sync = true;
  } else if (clusterId == ZCL_LEVEL_CONTROL_CLUSTER_ID && attributeId == ZCL_CURRENT_LEVEL_ATTRIBUTE_ID) {
    should_sync = true;
  } else if (clusterId == ZCL_COLOR_CONTROL_CLUSTER_ID && (attributeId == ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID
                                                        || attributeId == ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID
                                                        || attributeId == ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID
                                                        || attributeId == ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID)) {
    should_sync = true;
  }

  if (should_sync) {
    sync_light_state(endpoint);
  }
}

void sl_zigbee_af_identify_start_feedback_cb(uint8_t endpoint, uint16_t identifyTime)
{
  sl_zigbee_app_debug_println("Identify start: endpoint=%d, time=%d", endpoint, identifyTime);
  
  led_pattern_t identify_pattern = {
      .mode = LED_MODE_BLINK,
      .color = LED_COLOR_WHITE_DIM,
      .period_ms = 1000,
      .duration_ms = (uint32_t)identifyTime * 1000
  };
  led_manager_set_pattern(LED_PRIORITY_NOTIFICATION, &identify_pattern);
}

void sl_zigbee_af_identify_stop_feedback_cb(uint8_t endpoint)
{
  sl_zigbee_app_debug_println("Identify stop: endpoint=%d", endpoint);
  led_manager_clear_pattern(LED_PRIORITY_NOTIFICATION);
}

void sl_zigbee_af_on_off_cluster_server_post_init_cb(uint8_t endpoint)
{
  sync_light_state(endpoint);
}

void sl_zigbee_af_level_control_cluster_server_post_init_cb(uint8_t endpoint)
{
  sync_light_state(endpoint);
}

void sl_zigbee_af_color_control_server_compute_pwm_from_xy_cb(uint8_t endpoint)
{
  sync_light_state(endpoint);
}

void sl_zigbee_af_color_control_server_compute_pwm_from_temp_cb(uint8_t endpoint)
{
  sync_light_state(endpoint);
}