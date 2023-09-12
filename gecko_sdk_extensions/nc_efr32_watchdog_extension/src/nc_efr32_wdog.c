/***************************************************************************//**
 * @file nc_efr32_wdog.c
 * @brief Legacy HAL Watchdog
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories, Inc, www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "nc_efr32_wdog.h"

/***************************************************************************//**
 * Watchdog Interrupt Handler.
 ******************************************************************************/
#if (_SILICON_LABS_32B_SERIES >= 1)
void NC_EFR32_WDOG_IRQHandler(void)
{
  uint32_t interrupts;

  interrupts = WDOGn_IntGet(NC_EFR32_WDOG);
  WDOGn_IntClear(NC_EFR32_WDOG, interrupts);
}
#endif

void halInternalEnableWatchDog(void)
{
  // Enable LE interface
#if !defined(_SILICON_LABS_32B_SERIES_2)
  CMU_ClockEnable(cmuClock_HFLE, true);
  CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);
#endif

#if defined(_SILICON_LABS_32B_SERIES_2) && !defined(_SILICON_LABS_32B_SERIES_2_CONFIG_1)
  CMU_ClockEnable(NC_EFR32_WDOG_CMUCLOCK, true);
#endif

  // Make sure FULL reset is used on WDOG timeout
#if defined(_RMU_CTRL_WDOGRMODE_MASK)
  RMU_ResetControl(rmuResetWdog, rmuResetModeFull);
#endif

  /* Note: WDOG_INIT_DEFAULT comes from platform/emlib/inc/em_wdog.h */
  WDOG_Init_TypeDef init = WDOG_INIT_DEFAULT;

  // The default timeout of wdogPeriod_64k will trigger
  // watchdog reset after 2 seconds (64k / 32k) and
  // warning interrupt is triggered after 1.5 seconds (75% of timeout).
  init.perSel = NC_EFR32_WDOG_PERIOD;
  init.warnSel = NC_EFR32_WDOG_WARNING;

  // Counter keeps running during debug halt
  init.debugRun = NC_EFR32_WDOG_DEBUG_RUN;

#if defined(_WDOG_CTRL_CLKSEL_MASK)
  init.clkSel = wdogClkSelLFRCO;
#else
  // Series 2 devices select watchdog oscillator with the CMU.
#if (NC_EFR32_WDOGn == 0)
  CMU_CLOCK_SELECT_SET(WDOG0, LFRCO);
#elif (NC_EFR32_WDOGn == 1)
  CMU_CLOCK_SELECT_SET(WDOG1, LFRCO);
#endif
#endif

  WDOGn_Init(NC_EFR32_WDOG, &init);

  /* Enable WARN interrupt. */
#if defined(WDOG_IF_WARN) && !defined(BOOTLOADER)
  NVIC_ClearPendingIRQ(NC_EFR32_WDOG_IRQn);
  WDOGn_IntClear(NC_EFR32_WDOG, WDOG_IF_WARN);
  NVIC_EnableIRQ(NC_EFR32_WDOG_IRQn);
  WDOGn_IntEnable(NC_EFR32_WDOG, WDOG_IEN_WARN);
#endif
}

void halResetWatchdog(void)
{
#if (NC_EFR32_DISABLE_WATCHDOG == 0)
  WDOGn_Feed(NC_EFR32_WDOG);
#endif // (NC_EFR32_DISABLE_WATCHDOG == 0)
}

/** @brief The value no longer matters.
 */
void halInternalDisableWatchDog(uint8_t magicKey)
{
  (void) magicKey;

#if (NC_EFR32_DISABLE_WATCHDOG == 0)
  WDOGn_SyncWait(DEFAULT_WDOG);
  WDOGn_Enable(NC_EFR32_WDOG, false);
#endif // (NC_EFR32_DISABLE_WATCHDOG == 0)
}

bool halInternalWatchDogEnabled(void)
{
#if (NC_EFR32_DISABLE_WATCHDOG == 0)
  return WDOGn_IsEnabled(NC_EFR32_WDOG);
#else // (NC_EFR32_DISABLE_WATCHDOG == 0)
  return false;
#endif // (NC_EFR32_DISABLE_WATCHDOG == 0)
}
