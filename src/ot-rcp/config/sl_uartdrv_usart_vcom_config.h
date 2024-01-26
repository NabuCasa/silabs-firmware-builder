/***************************************************************************//**
 * @file
 * @brief UARTDRV_USART Config
 *******************************************************************************
 * # License
 * <b>Copyright 2019 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_UARTDRV_USART_VCOM_CONFIG_H
#define SL_UARTDRV_USART_VCOM_CONFIG_H

#include "em_usart.h"
// <<< Use Configuration Wizard in Context Menu >>>

// <h> UART settings
// <o SL_UARTDRV_USART_VCOM_BAUDRATE> Baud rate
// <i> Default: 115200
#define SL_UARTDRV_USART_VCOM_BAUDRATE        460800

// <o SL_UARTDRV_USART_VCOM_PARITY> Parity mode to use
// <usartNoParity=> No Parity
// <usartEvenParity=> Even parity
// <usartOddParity=> Odd parity
// <i> Default: usartNoParity
#define SL_UARTDRV_USART_VCOM_PARITY          usartNoParity

// <o SL_UARTDRV_USART_VCOM_STOP_BITS> Number of stop bits to use.
// <usartStopbits0p5=> 0.5 stop bits
// <usartStopbits1=> 1 stop bits
// <usartStopbits1p5=> 1.5 stop bits
// <usartStopbits2=> 2 stop bits
// <i> Default: usartStopbits1
#define SL_UARTDRV_USART_VCOM_STOP_BITS       usartStopbits1

// <o SL_UARTDRV_USART_VCOM_FLOW_CONTROL_TYPE> Flow control method
// <uartdrvFlowControlNone=> None
// <uartdrvFlowControlSw=> Software XON/XOFF
// <uartdrvFlowControlHw=> nRTS/nCTS hardware handshake
// <uartdrvFlowControlHwUart=> UART peripheral controls nRTS/nCTS
// <i> Default: uartdrvFlowControlHwUart
#define SL_UARTDRV_USART_VCOM_FLOW_CONTROL_TYPE uartdrvFlowControlHwUart

// <o SL_UARTDRV_USART_VCOM_OVERSAMPLING> Oversampling selection
// <usartOVS16=> 16x oversampling
// <usartOVS8=> 8x oversampling
// <usartOVS6=> 6x oversampling
// <usartOVS4=> 4x oversampling
// <i> Default: usartOVS16
#define SL_UARTDRV_USART_VCOM_OVERSAMPLING      usartOVS4

// <o SL_UARTDRV_USART_VCOM_MVDIS> Majority vote disable for 16x, 8x and 6x oversampling modes
// <true=> True
// <false=> False
#define SL_UARTDRV_USART_VCOM_MVDIS             false

// <o SL_UARTDRV_USART_VCOM_RX_BUFFER_SIZE> Size of the receive operation queue
// <i> Default: 6
#define SL_UARTDRV_USART_VCOM_RX_BUFFER_SIZE  6

// <o SL_UARTDRV_USART_VCOM_TX_BUFFER_SIZE> Size of the transmit operation queue
// <i> Default: 6
#define SL_UARTDRV_USART_VCOM_TX_BUFFER_SIZE 6

// </h>
// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>
// <usart signal=TX,RX,(CTS),(RTS)> SL_UARTDRV_USART_VCOM
// $[USART_SL_UARTDRV_USART_VCOM]
#ifndef SL_UARTDRV_USART_VCOM_PERIPHERAL        
#define SL_UARTDRV_USART_VCOM_PERIPHERAL         USART0
#endif
#ifndef SL_UARTDRV_USART_VCOM_PERIPHERAL_NO     
#define SL_UARTDRV_USART_VCOM_PERIPHERAL_NO      0
#endif

// USART0 TX on PA05
#ifndef SL_UARTDRV_USART_VCOM_TX_PORT           
#define SL_UARTDRV_USART_VCOM_TX_PORT            gpioPortA
#endif
#ifndef SL_UARTDRV_USART_VCOM_TX_PIN            
#define SL_UARTDRV_USART_VCOM_TX_PIN             5
#endif

// USART0 RX on PA06
#ifndef SL_UARTDRV_USART_VCOM_RX_PORT           
#define SL_UARTDRV_USART_VCOM_RX_PORT            gpioPortA
#endif
#ifndef SL_UARTDRV_USART_VCOM_RX_PIN            
#define SL_UARTDRV_USART_VCOM_RX_PIN             6
#endif

// USART0 CTS on PC03
#ifndef SL_UARTDRV_USART_VCOM_CTS_PORT          
#define SL_UARTDRV_USART_VCOM_CTS_PORT           gpioPortC
#endif
#ifndef SL_UARTDRV_USART_VCOM_CTS_PIN           
#define SL_UARTDRV_USART_VCOM_CTS_PIN            3
#endif

// USART0 RTS on PC02
#ifndef SL_UARTDRV_USART_VCOM_RTS_PORT          
#define SL_UARTDRV_USART_VCOM_RTS_PORT           gpioPortC
#endif
#ifndef SL_UARTDRV_USART_VCOM_RTS_PIN           
#define SL_UARTDRV_USART_VCOM_RTS_PIN            2
#endif
// [USART_SL_UARTDRV_USART_VCOM]$
// <<< sl:end pin_tool >>>

#endif // SL_UARTDRV_USART_VCOM_CONFIG_H
