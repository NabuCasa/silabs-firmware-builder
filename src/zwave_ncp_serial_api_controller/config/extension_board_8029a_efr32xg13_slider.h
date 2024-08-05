/**
 * Provides support for BRD8029A (Slider on EXP Board)
 *
 * @copyright 2022 Silicon Laboratories Inc.
 */

#ifndef EXTENSION_BOARD_8029A_EFR32XG13_SLIDER_H
#define EXTENSION_BOARD_8029A_EFR32XG13_SLIDER_H

/*************************************************************************/
/* Configure slider button                                               */
/*************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>
// <h>Slider Configuration
#define SLIDER1_LABEL                       "SW1"

// <o SLIDER1_ON_VALUE> Slider ON value
// <1=> Active high
// <0=> Active low
// <d> 0
#define SLIDER1_ON_VALUE       0

// <o SLIDER1_INT_NO> Interrupt pin number
// <d> 9
#define SLIDER1_INT_NO       9

// <q SLIDER1_CAN_WAKEUP_EM4> Wake up from EM4
// <d> 0
#define SLIDER1_CAN_WAKEUP_EM4       0

// </h>

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>

// <gpio> SLIDER1_GPIO
// $[GPIO_SLIDER1_GPIO]
#ifndef SLIDER1_GPIO_PORT                       
#define SLIDER1_GPIO_PORT                        gpioPortC
#endif
#ifndef SLIDER1_GPIO_PIN                        
#define SLIDER1_GPIO_PIN                         9
#endif
// [GPIO_SLIDER1_GPIO]$

// <<< sl:end pin_tool >>>

#endif /* EXTENSION_BOARD_8029A_EFR32XG13_SLIDER_H */
