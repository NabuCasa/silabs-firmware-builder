/***************************************************************************//**
 * @file
 * @brief USTIMER configuration file.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#ifndef __SILICON_LABS_USTIMER_CONFIG_H__
#define __SILICON_LABS_USTIMER_CONFIG_H__

/***************************************************************************//**
 * @addtogroup ustimer
 * @{
 ******************************************************************************/

// <<< sl:start pin_tool >>>
// <timer> USTIMER
// $[TIMER_USTIMER]
#ifndef USTIMER_PERIPHERAL                      
#define USTIMER_PERIPHERAL                       TIMER0
#endif
#ifndef USTIMER_PERIPHERAL_NO                   
#define USTIMER_PERIPHERAL_NO                    0
#endif
// [TIMER_USTIMER]$

// <<< sl:end pin_tool >>>

#define USTIMER_TIMER USTIMER_PERIPHERAL_NO

/** @} (end addtogroup ustimer) */

#endif /* __SILICON_LABS_USTIMER_CONFIG_H__ */
