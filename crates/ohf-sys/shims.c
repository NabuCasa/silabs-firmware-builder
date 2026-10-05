/*
 * Generic C wrappers exposing real symbols for __STATIC_INLINE SDK helpers that
 * bindgen cannot bind. Compiled into ohf-sys by build.rs. Keep these one-liners that
 * only re-export an SDK call; anything more belongs in Rust.
 */
#include <stdint.h>
#include "em_gpio.h"

void ohf_gpio_pin_mode_set(uint32_t port, uint32_t pin, uint32_t mode, uint32_t out)
{
    GPIO_PinModeSet((GPIO_Port_TypeDef)port, pin, (GPIO_Mode_TypeDef)mode, out);
}

/* NVIC_SystemReset is a CMSIS __STATIC_INLINE (__NO_RETURN), so bindgen can't bind it. */
void ohf_system_reset(void)
{
    NVIC_SystemReset();
}
