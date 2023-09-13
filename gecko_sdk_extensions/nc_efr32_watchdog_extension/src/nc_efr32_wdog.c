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

#include "em_wdog.h"
#include "nc_efr32_wdog.h"
#include "sli_cpc_timer.h"

static sli_cpc_timer_handle_t timer_handle;


void nc_periodic_timer(sli_cpc_timer_handle_t *handle, void *data)
{
  (void)data;

  WDOGn_Feed(WDOG0);

  sl_status_t status;
  status = sli_cpc_timer_restart_timer(handle,
                                       sli_cpc_timer_ms_to_tick(1000),  // 1 second
                                       nc_periodic_timer,
                                       (void *)NULL);

  EFM_ASSERT(status == SL_STATUS_OK);
}


void nc_enable_watchdog(void)
{
  // Enable LE interface
#if !defined(_SILICON_LABS_32B_SERIES_2)
  CMU_ClockEnable(cmuClock_HFLE, true);
  CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);
#endif

#if defined(_SILICON_LABS_32B_SERIES_2) && !defined(_SILICON_LABS_32B_SERIES_2_CONFIG_1)
  CMU_ClockEnable(cmuClock_WDOG0, true);
#endif

  // Make sure FULL reset is used on WDOG timeout
#if defined(_RMU_CTRL_WDOGRMODE_MASK)
  RMU_ResetControl(rmuResetWdog, rmuResetModeFull);
#endif

  WDOG_Init_TypeDef init = WDOG_INIT_DEFAULT;
  init.enable = false;
  init.perSel = wdogPeriod_128k;  // 4 seconds

#if defined(_WDOG_CTRL_CLKSEL_MASK)
  init.clkSel = wdogClkSelLFRCO;
#else
  // Series 2 devices select watchdog oscillator with the CMU.
  CMU_ClockSelectSet(cmuClock_WDOG0, cmuSelect_LFRCO);
#endif

  WDOGn_Init(WDOG0, &init);
  WDOGn_Enable(WDOG0, true);
  WDOGn_Feed(WDOG0);

  nc_periodic_timer(&timer_handle, NULL);
}
