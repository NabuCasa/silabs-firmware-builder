/* The SDK surface we bind. What is actually emitted is controlled by the
 * allowlist in build.rs; these includes just make the declarations visible. */
#include "sl_status.h"
#include "sl_i2cspm.h"
#include "em_i2c.h"
#include "sl_udelay.h"
#include "sl_led.h"
#include "sl_simple_rgb_pwm_led.h"
#include "spidrv.h"
#include "em_gpio.h"
#include "sl_device_gpio.h"
#include "sl_sleeptimer.h"
#include "em_core.h"
