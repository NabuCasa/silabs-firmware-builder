/* Real symbols for inline SDK helpers, which bindgen can't bind */
#include <stdint.h>
#include "em_gpio.h"

void ohf_gpio_pin_mode_set(uint32_t port, uint32_t pin, uint32_t mode, uint32_t out)
{
    GPIO_PinModeSet((GPIO_Port_TypeDef)port, pin, (GPIO_Mode_TypeDef)mode, out);
}

void ohf_system_reset(void)
{
    NVIC_SystemReset();
}
