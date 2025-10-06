#ifndef BTL_GPIO_ACTIVATION_DUAL_H
#define BTL_GPIO_ACTIVATION_DUAL_H

/***************************************************************************//**
 * @addtogroup Components
 * @{
 * @addtogroup GpioActivation GPIO Activation
 * @brief Enter bootloader based on GPIO state.
 * @brief This component provides functionality to enter firmware upgrade mode automatically after reset if a GPIO pin is active during boot.
 * @brief  The GPIO pin location and polarity are configurable.
 * @details
 * @{ 
 * @addtogroup ButtonGPIO Button GPIO
 * @brief Enter bootloader based on Button GPIO state.
 * @brief By enabling the GPIO activation component, the firmware upgrade mode can be activated by the GPIO configuration.
 * @details
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Enter the bootlader if the GPIO pin is active.
 *
 * @return True if the bootloader should be entered
 ******************************************************************************/
bool gpio_enterBootloader(void);

/** @} (end addtogroup Button GPIO) */
/** @} (end addtogroup GpioActivation) */
/** @} (end addtogroup Components) */

#endif // BTL_GPIO_ACTIVATION_DUAL_H
