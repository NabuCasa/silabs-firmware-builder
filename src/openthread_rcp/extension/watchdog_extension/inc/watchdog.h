/*
 * watchdog.h
 *
 * Hardware watchdog for the bare-metal superloop, with the WDOG warning
 * interrupt routed into the legacy-HAL crash handler for enhanced capture.
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

// Configures and enables WDOG0. Wired to the `driver_init` event.
void watchdog_init(void);

// Feeds WDOG0. Wired to the `service_process_action` event (every superloop pass).
void watchdog_feed(void);

#endif // WATCHDOG_H
