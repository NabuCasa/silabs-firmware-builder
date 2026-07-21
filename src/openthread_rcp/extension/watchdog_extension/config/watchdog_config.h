/*
 * watchdog_config.h
 *
 * Watchdog configuration.
 */

// <<< Use Configuration Wizard in Context Menu >>>

#ifndef WATCHDOG_CONFIG_H
#define WATCHDOG_CONFIG_H

#include "em_wdog.h"

// <h> Watchdog Configuration

// <e WATCHDOG_ENABLE> Enable the watchdog.
// <i> Default: 1
#define WATCHDOG_ENABLE 1
// </e>

// <o WATCHDOG_PERIOD> Watchdog timeout period.
// <i> Default: wdogPeriod_64k (~2s on the 32 kHz LFRCO). Warning interrupt
// <i> fires at 75% (~1.5s); reset occurs at the full period.
// <wdogPeriod_2k=> wdogPeriod_2k / ~64ms
// <wdogPeriod_4k=> wdogPeriod_4k / ~125ms
// <wdogPeriod_8k=> wdogPeriod_8k / ~250ms
// <wdogPeriod_16k=> wdogPeriod_16k / ~500ms
// <wdogPeriod_32k=> wdogPeriod_32k / ~1s
// <wdogPeriod_64k=> wdogPeriod_64k / ~2s
// <wdogPeriod_128k=> wdogPeriod_128k / ~4s
// <wdogPeriod_256k=> wdogPeriod_256k / ~8s
#define WATCHDOG_PERIOD wdogPeriod_128k

// <q WATCHDOG_DEBUG_RUN> Keep counting during debug halt.
// <i> Default: 0 (paused while a debugger has the core halted).
#define WATCHDOG_DEBUG_RUN 0

// </h>

#endif // WATCHDOG_CONFIG_H

// <<< end of configuration section >>>
