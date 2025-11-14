/***************************************************************************//**
 * @file
 * @brief Configuration header for bootloader Dual GPIO Activation
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc.  Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement.  This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#ifndef BTL_GPIO_ACTIVATION_DUAL_CONFIG_H
#define BTL_GPIO_ACTIVATION_DUAL_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h>Properties of Bootloader Entry

// <o NC_PRIMARY_GPIO_ACTIVATION_POLARITY> Primary button active state
// <LOW=> Low
// <HIGH=> High
// <i> Default: LOW
// <i> Enter firmware upgrade mode if primary GPIO pin has this state
#define NC_PRIMARY_GPIO_ACTIVATION_POLARITY       LOW

// <o NC_SECONDARY_GPIO_ACTIVATION_POLARITY> Secondary button active state
// <LOW=> Low
// <HIGH=> High
// <i> Default: LOW
// <i> Enter firmware upgrade mode if secondary GPIO pin has this state
#define NC_SECONDARY_GPIO_ACTIVATION_POLARITY     LOW

// </h>

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>

// <gpio> NC_BTL_BUTTON_PRIMARY

// $[GPIO_NC_BTL_BUTTON_PRIMARY]
#warning "Primary GPIO activation port not configured"
//#define NC_BTL_BUTTON_PRIMARY_PORT                      gpioPortA
//#define NC_BTL_BUTTON_PRIMARY_PIN                       6
// [GPIO_NC_BTL_BUTTON_PRIMARY]$

// <gpio> NC_BTL_BUTTON_SECONDARY

// $[GPIO_NC_BTL_BUTTON_SECONDARY]
#warning "Secondary GPIO activation port not configured"
//#define NC_BTL_BUTTON_SECONDARY_PORT                    gpioPortC
//#define NC_BTL_BUTTON_SECONDARY_PIN                     5
// [GPIO_NC_BTL_BUTTON_SECONDARY]$

// <<< sl:end pin_tool >>>

#endif // BTL_GPIO_ACTIVATION_DUAL_CONFIG_H
