#include "config/btl_config.h"
#include "em_device.h"
#include "em_gpio.h"
#include "gpio/gpio-activation/btl_gpio_activation.h"
#include "btl_gpio_activation_dual_cfg.h"

#define HIGH 0
#define LOW  1

bool gpio_enterBootloader(void)
{
  bool pressed;

#if defined(CMU_HFBUSCLKEN0_GPIO)
  CMU->HFBUSCLKEN0 |= CMU_HFBUSCLKEN0_GPIO;
#endif
#if defined(_CMU_CLKEN0_MASK)
  CMU->CLKEN0_SET = CMU_CLKEN0_GPIO;
#endif

  GPIO_PinModeSet(NC_BTL_BUTTON_PRIMARY_PORT, NC_BTL_BUTTON_PRIMARY_PIN, gpioModePushPull, NC_PRIMARY_GPIO_ACTIVATION_POLARITY);
  GPIO_PinModeSet(NC_BTL_BUTTON_SECONDARY_PORT, NC_BTL_BUTTON_SECONDARY_PIN, gpioModePushPull, NC_SECONDARY_GPIO_ACTIVATION_POLARITY);
  for (volatile int i = 0; i < 100; i++);

  GPIO_PinModeSet(NC_BTL_BUTTON_PRIMARY_PORT, NC_BTL_BUTTON_PRIMARY_PIN, gpioModeInputPull, NC_PRIMARY_GPIO_ACTIVATION_POLARITY);
  GPIO_PinModeSet(NC_BTL_BUTTON_SECONDARY_PORT, NC_BTL_BUTTON_SECONDARY_PIN, gpioModeInputPull, NC_SECONDARY_GPIO_ACTIVATION_POLARITY);
  for (volatile int i = 0; i < 500; i++);

  bool primary_pressed = (GPIO_PinInGet(NC_BTL_BUTTON_PRIMARY_PORT, NC_BTL_BUTTON_PRIMARY_PIN) != NC_PRIMARY_GPIO_ACTIVATION_POLARITY);
  bool secondary_pressed = (GPIO_PinInGet(NC_BTL_BUTTON_SECONDARY_PORT, NC_BTL_BUTTON_SECONDARY_PIN) != NC_SECONDARY_GPIO_ACTIVATION_POLARITY);
  pressed = primary_pressed || secondary_pressed;

  GPIO_PinModeSet(NC_BTL_BUTTON_PRIMARY_PORT, NC_BTL_BUTTON_PRIMARY_PIN, gpioModeDisabled, NC_PRIMARY_GPIO_ACTIVATION_POLARITY);
  GPIO_PinModeSet(NC_BTL_BUTTON_SECONDARY_PORT, NC_BTL_BUTTON_SECONDARY_PIN, gpioModeDisabled, NC_SECONDARY_GPIO_ACTIVATION_POLARITY);

#if defined(CMU_HFBUSCLKEN0_GPIO)
  CMU->HFBUSCLKEN0 &= ~CMU_HFBUSCLKEN0_GPIO;
#endif

  return pressed;
}
