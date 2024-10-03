/**
 * @file
 * Platform abstraction for Serial API application
 * 
 * @copyright 2022 Silicon Laboratories Inc.
 */
#ifndef SERIALAPI_HW_H_
#define SERIALAPI_HW_H_

/**
 * Get zpal-uart config extension.
 */
const void* SerialAPI_get_uart_config_ext(void);

#ifdef USB_SUSPEND_SUPPORT

typedef void (*SerialAPI_hw_usb_suspend_callback_t)(void);

/**
 * Set USB suspend callback.
 * 
 * @param[in] callback USB suspend callback.
 */
void SerialAPI_set_usb_supend_callback(SerialAPI_hw_usb_suspend_callback_t callback);

/**
 * USB suspend handler.
 */
void SerialAPI_hw_usb_suspend_handler(void);

#endif /* USB_SUSPEND_SUPPORT */

#endif /* SERIALAPI_HW_H_ */
