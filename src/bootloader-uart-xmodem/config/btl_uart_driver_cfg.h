/***************************************************************************//**
 * @file
 * @brief Configuration header for bootloader Uart Driver
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
#ifndef BTL_UART_DRIVER_CONFIG_H
#define BTL_UART_DRIVER_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h>USART settings

// <o SL_SERIAL_UART_BAUD_RATE> Baud rate
// <i> Default: 115200
#define SL_SERIAL_UART_BAUD_RATE              115200

// <e SL_SERIAL_UART_FLOW_CONTROL> Hardware flow control
// <i> Default: 0
#define SL_SERIAL_UART_FLOW_CONTROL      0
// </e>

// <o SL_DRIVER_UART_RX_BUFFER_SIZE> Receive buffer size
// <0-2048:1>
// <i> Default: 512 [0-2048]
#define SL_DRIVER_UART_RX_BUFFER_SIZE    512

// <o SL_DRIVER_UART_TX_BUFFER_SIZE> Transmit buffer size
// <0-2048:1>
// <i> Default: 128 [0-2048]
#define SL_DRIVER_UART_TX_BUFFER_SIZE    128

// <e SL_VCOM_ENABLE> Virtual COM Port
// <i> Default: 0
#define SL_VCOM_ENABLE                    1
// </e>

// </h>

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>
// <usart signal=TX,RX,(CTS),(RTS)> SL_SERIAL_UART
// $[USART_SL_SERIAL_UART]
#ifndef SL_SERIAL_UART_PERIPHERAL               
#define SL_SERIAL_UART_PERIPHERAL                USART0
#endif
#ifndef SL_SERIAL_UART_PERIPHERAL_NO            
#define SL_SERIAL_UART_PERIPHERAL_NO             0
#endif

// USART0 TX on PA05
#ifndef SL_SERIAL_UART_TX_PORT                  
#define SL_SERIAL_UART_TX_PORT                   gpioPortA
#endif
#ifndef SL_SERIAL_UART_TX_PIN                   
#define SL_SERIAL_UART_TX_PIN                    5
#endif

// USART0 RX on PA06
#ifndef SL_SERIAL_UART_RX_PORT                  
#define SL_SERIAL_UART_RX_PORT                   gpioPortA
#endif
#ifndef SL_SERIAL_UART_RX_PIN                   
#define SL_SERIAL_UART_RX_PIN                    6
#endif

// USART0 CTS on PC03
#ifndef SL_SERIAL_UART_CTS_PORT                 
#define SL_SERIAL_UART_CTS_PORT                  gpioPortC
#endif
#ifndef SL_SERIAL_UART_CTS_PIN                  
#define SL_SERIAL_UART_CTS_PIN                   3
#endif

// USART0 RTS on PC02
#ifndef SL_SERIAL_UART_RTS_PORT                 
#define SL_SERIAL_UART_RTS_PORT                  gpioPortC
#endif
#ifndef SL_SERIAL_UART_RTS_PIN                  
#define SL_SERIAL_UART_RTS_PIN                   2
#endif
// [USART_SL_SERIAL_UART]$

// <gpio optional=true> SL_VCOM_ENABLE

// $[GPIO_SL_VCOM_ENABLE]
#ifndef SL_VCOM_ENABLE_PORT                     
#define SL_VCOM_ENABLE_PORT                      gpioPortD
#endif
#ifndef SL_VCOM_ENABLE_PIN                      
#define SL_VCOM_ENABLE_PIN                       4
#endif
// [GPIO_SL_VCOM_ENABLE]$

// <<< sl:end pin_tool >>>

#endif // BTL_UART_DRIVER_CONFIG_H