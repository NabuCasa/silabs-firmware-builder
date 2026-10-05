//! ZBT-2 router: shows the Zigbee light endpoint's state, joining and identify on the LED,
//! and retries network steering until it joins.
#![no_std]

use core::ffi::c_int;
use core::mem::{size_of, MaybeUninit};

use ohf_led_effects::{
    clear_pattern, color, search_pulse, set_color, set_network_state, set_pattern,
};
use ohf_led_effects::{Pattern, Priority, Rgb};
use ohf_sys::token::has_stored_network;
use ohf_sys::SyncCell;

#[allow(
    non_camel_case_types,
    non_upper_case_globals,
    non_snake_case,
    dead_code
)]
mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
use bindings::*;

const LIGHT_ENDPOINT: u8 = 1;
const COMMISSIONING_RETRY_DELAY_MS: u32 = 5000;
const JOIN_NOTIFICATION_MS: u32 = 2000;
const IDENTIFY_BLINK_PERIOD_MS: u16 = 1000;
const COLOR_MODE_TEMPERATURE: u8 = 0x02;

// The SDK's event macros pass these for "current network" and "no endpoint"
const CURRENT_NETWORK: u8 = 0xFF;
const NO_ENDPOINT: u8 = 0xFF;

static COMMISSIONING_RETRY_EVENT: SyncCell<sl_zigbee_af_event_t> =
    SyncCell::new(unsafe { MaybeUninit::zeroed().assume_init() });

extern "C" {
    fn powf(x: f32, y: f32) -> f32;
    fn logf(x: f32) -> f32;
}

// sl_zigbee_app_debug_println, which is a macro
macro_rules! app_debug_println {
    ($format:literal $(, $arg:expr)* $(,)?) => {
        if SL_ZIGBEE_DEBUG_APP_GROUP_ENABLED != 0 {
            unsafe {
                sli_zigbee_debug_print(SL_ZIGBEE_DEBUG_PRINT_TYPE_APP, true, $format.as_ptr() $(, $arg)*)
            }
        }
    };
}

// sRGB from CIE xy chromaticity, with the level as luminance
fn xy_to_rgb(level: u8, current_x: u16, current_y: u16) -> Rgb {
    if current_y == 0 {
        return color::BLACK;
    }

    let x = current_x as f32 / 65535.0;
    let y = current_y as f32 / 65535.0;
    let z = 1.0 - x - y;

    let big_y = level as f32 / 254.0;
    let big_x = (big_y / y) * x;
    let big_z = (big_y / y) * z;

    // XYZ to linear sRGB (D65)
    let r = big_x * 3.2410 - big_y * 1.5374 - big_z * 0.4986;
    let g = -big_x * 0.9692 + big_y * 1.8760 + big_z * 0.0416;
    let b = big_x * 0.0556 - big_y * 0.2040 + big_z * 1.0570;

    let gamma = |c: f32| {
        let c = if c <= 0.00304 {
            12.92 * c
        } else {
            1.055 * unsafe { powf(c, 1.0 / 2.4) } - 0.055
        };
        (c.clamp(0.0, 1.0) * 65535.0) as u16
    };
    Rgb {
        r: gamma(r),
        g: gamma(g),
        b: gamma(b),
    }
}

// Tanner Helland's color temperature approximation
fn color_temp_to_rgb(level: u8, mireds: u16) -> Rgb {
    let kelvin = (1_000_000.0 / mireds as f32).clamp(1600.0, 6535.0);
    let temp = kelvin / 100.0;

    let red = if temp <= 66.0 {
        255.0
    } else {
        329.698727446 * unsafe { powf(temp - 60.0, -0.1332047592) }
    };
    let green = if temp <= 66.0 {
        99.4708025861 * unsafe { logf(temp) } - 161.1195681661
    } else {
        288.1221695283 * unsafe { powf(temp - 60.0, -0.0755148492) }
    };
    let blue = if temp >= 66.0 {
        255.0
    } else if temp <= 19.0 {
        0.0
    } else {
        138.5177312231 * unsafe { logf(temp - 10.0) } - 305.0447927307
    };

    let brightness = level as f32 / 254.0;
    let scale = |c: f32| (c.clamp(0.0, 255.0) * 257.0 * brightness) as u16;
    Rgb {
        r: scale(red),
        g: scale(green),
        b: scale(blue),
    }
}

// A failed read leaves `default`
fn read_attribute<T: Copy>(endpoint: u8, cluster: u32, attribute: u32, default: T) -> T {
    let mut value = default;
    unsafe {
        sl_zigbee_af_read_server_attribute(
            endpoint,
            cluster as u16,
            attribute as u16,
            &mut value as *mut T as *mut u8,
            size_of::<T>() as u8,
        );
    }
    value
}

fn sync_light_state(endpoint: u8) {
    let on_off: u8 = read_attribute(endpoint, ZCL_ON_OFF_CLUSTER_ID, ZCL_ON_OFF_ATTRIBUTE_ID, 0);
    if on_off == 0 {
        clear_pattern(Priority::Manual);
        return;
    }

    let level: u8 = read_attribute(
        endpoint,
        ZCL_LEVEL_CONTROL_CLUSTER_ID,
        ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
        0,
    )
    .max(1);

    let cluster = ZCL_COLOR_CONTROL_CLUSTER_ID;
    let color_mode: u8 = read_attribute(
        endpoint,
        cluster,
        ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID,
        0x01,
    );

    let rgb = if color_mode == COLOR_MODE_TEMPERATURE {
        let mireds = read_attribute(
            endpoint,
            cluster,
            ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
            0u16,
        );
        color_temp_to_rgb(level, mireds)
    } else {
        let x = read_attribute(
            endpoint,
            cluster,
            ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID,
            0u16,
        );
        let y = read_attribute(
            endpoint,
            cluster,
            ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID,
            0u16,
        );
        xy_to_rgb(level, x, y)
    };

    set_color(Priority::Manual, rgb);
}

fn retry_commissioning_after(delay_ms: u32) {
    unsafe {
        sli_zigbee_af_event_set_delay_ms(COMMISSIONING_RETRY_EVENT.get(), NO_ENDPOINT, delay_ms)
    };
}

unsafe extern "C" fn commissioning_retry_event_handler(_event: *mut sl_zigbee_af_event_t) {
    if sl_zigbee_af_network_state() == SL_ZIGBEE_JOINED_NETWORK as u8 {
        return;
    }

    // Blue pulse while searching
    set_pattern(Priority::Background, search_pulse(color::SEARCH_BLUE));
    let status = sl_zigbee_af_network_steering_start();
    app_debug_println!(c"Join network start: 0x%X", status);
}

#[no_mangle]
pub extern "C" fn zbt2_router_init(_init_level: u8) {
    ohf_led_effects::init();
    set_network_state(has_stored_network());

    let event = COMMISSIONING_RETRY_EVENT.get();
    unsafe {
        sli_zigbee_af_event_internal_init(
            event,
            core::ptr::null(),
            commissioning_retry_event_handler as *mut _,
            CURRENT_NETWORK,
            NO_ENDPOINT,
        );
        if !has_stored_network() {
            sli_zigbee_af_event_set_active(event, NO_ENDPOINT);
        }
    }
}

#[no_mangle]
pub extern "C" fn zbt2_router_stack_status_cb(status: sl_status_t) {
    if status == SL_STATUS_NETWORK_DOWN {
        set_network_state(false);
        clear_pattern(Priority::Manual);
    } else if status == SL_STATUS_NETWORK_UP {
        set_network_state(true);
        set_pattern(
            Priority::Notification,
            Pattern::solid(color::JOIN_GREEN).lasting(JOIN_NOTIFICATION_MS),
        );

        // Shown once the join notification ends
        sync_light_state(LIGHT_ENDPOINT);
    }
}

#[no_mangle]
pub extern "C" fn sl_zigbee_af_network_steering_complete_cb(
    status: sl_status_t,
    _total_beacons: u8,
    _join_attempts: u8,
    _final_state: u8,
) {
    app_debug_println!(c"Network steering complete: 0x%X", status);

    if status != SL_STATUS_OK {
        set_network_state(false);
        clear_pattern(Priority::Notification);
        retry_commissioning_after(COMMISSIONING_RETRY_DELAY_MS);
    }
}

#[no_mangle]
pub extern "C" fn sl_zigbee_af_post_attribute_change_cb(
    endpoint: u8,
    cluster: u16,
    attribute: u16,
    mask: u8,
    _manufacturer_code: u16,
    _type: u8,
    _size: u8,
    _value: *mut u8,
) {
    if mask as u32 != CLUSTER_MASK_SERVER {
        return;
    }

    let (cluster, attribute) = (cluster as u32, attribute as u32);
    let affects_light = match cluster {
        ZCL_ON_OFF_CLUSTER_ID => attribute == ZCL_ON_OFF_ATTRIBUTE_ID,
        ZCL_LEVEL_CONTROL_CLUSTER_ID => attribute == ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
        ZCL_COLOR_CONTROL_CLUSTER_ID => matches!(
            attribute,
            ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID
                | ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID
                | ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID
                | ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID
        ),
        _ => false,
    };

    if affects_light {
        sync_light_state(endpoint);
    }
}

#[no_mangle]
pub extern "C" fn sl_zigbee_af_identify_start_feedback_cb(endpoint: u8, identify_time: u16) {
    app_debug_println!(
        c"Identify start: endpoint=%d, time=%d",
        endpoint as c_int,
        identify_time as c_int,
    );
    set_pattern(
        Priority::Notification,
        Pattern::blink(color::WHITE_DIM, IDENTIFY_BLINK_PERIOD_MS)
            .lasting(identify_time as u32 * 1000),
    );
}

#[no_mangle]
pub extern "C" fn sl_zigbee_af_identify_stop_feedback_cb(endpoint: u8) {
    app_debug_println!(c"Identify stop: endpoint=%d", endpoint as c_int);
    clear_pattern(Priority::Notification);
}

#[no_mangle]
pub extern "C" fn sl_zigbee_af_on_off_cluster_server_post_init_cb(endpoint: u8) {
    sync_light_state(endpoint);
}

#[no_mangle]
pub extern "C" fn sl_zigbee_af_level_control_cluster_server_post_init_cb(endpoint: u8) {
    sync_light_state(endpoint);
}

#[no_mangle]
pub extern "C" fn sl_zigbee_af_color_control_server_compute_pwm_from_xy_cb(endpoint: u8) {
    sync_light_state(endpoint);
}

#[no_mangle]
pub extern "C" fn sl_zigbee_af_color_control_server_compute_pwm_from_temp_cb(endpoint: u8) {
    sync_light_state(endpoint);
}
