/*
 * watchdog.c
 *
 * WDOG0 driver for the bare-metal superloop.
 *
 * The watchdog is fed once per superloop pass (`service_process_action`). If the
 * loop stalls, the warning interrupt fires at 75% of the timeout and is routed
 * straight into the legacy-HAL crash collector (`fault` in faults.s). That path
 * captures the registers/stack of the stalled code and classifies the reset as
 * RESET_WATCHDOG_CAUGHT (see halInternalCrashHandler() in diagnostic.c), then
 * resets the chip well before the hard timeout would expire.
 */

#include "watchdog.h"
#include "watchdog_config.h"

#include "em_device.h"
#include "em_wdog.h"
#include "em_rmu.h"
#include "sl_clock_manager.h"

// Defined in the legacy-HAL crash collector (faults.s). Entered with the
// exception frame intact; saves crash data and resets via halInternalSysReset().
extern void fault(void);

void watchdog_init(void)
{
#if WATCHDOG_ENABLE
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_WDOG0);

  // Use a FULL reset (not just a core reset) on watchdog timeout.
#if defined(_RMU_CTRL_WDOGRMODE_MASK)
  RMU_ResetControl(rmuResetWdog, rmuResetModeFull);
#endif

  WDOG_Init_TypeDef init = WDOG_INIT_DEFAULT;
  init.perSel   = WATCHDOG_PERIOD;
  init.warnSel  = wdogWarnTime75pct;
  init.debugRun = WATCHDOG_DEBUG_RUN;

#if defined(_WDOG_CTRL_CLKSEL_MASK)
  init.clkSel = wdogClkSelLFRCO;
#endif

  WDOGn_Init(DEFAULT_WDOG, &init);

  // Enable the WARN interrupt so a stall is caught with enhanced crash info
  // before the hard reset. The IRQ vector (WDOG0_IRQHandler below) branches
  // into the crash collector; the WARN flag is intentionally left set so the
  // collector can classify the reset as RESET_WATCHDOG_CAUGHT.
#if defined(WDOG_IF_WARN)
  NVIC_ClearPendingIRQ(WDOG0_IRQn);
  WDOGn_IntClear(DEFAULT_WDOG, WDOG_IF_WARN);
  NVIC_EnableIRQ(WDOG0_IRQn);
  WDOGn_IntEnable(DEFAULT_WDOG, WDOG_IEN_WARN);
#endif
#endif // WATCHDOG_ENABLE
}

void watchdog_feed(void)
{
#if WATCHDOG_ENABLE
  WDOGn_Feed(DEFAULT_WDOG);
#endif
}

#if WATCHDOG_ENABLE && defined(WDOG_IF_WARN)
// Route the warning interrupt into the crash collector with the exception frame
// untouched. `naked` keeps the compiler from emitting a prologue; `used`
// prevents LTO from discarding the vector-table override.
__attribute__((naked, used)) void WDOG0_IRQHandler(void)
{
  __asm volatile ("b.w fault");
}
#endif
