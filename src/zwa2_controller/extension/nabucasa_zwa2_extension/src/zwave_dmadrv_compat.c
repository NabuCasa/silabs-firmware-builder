/*
 * zwave_dmadrv_compat.c
 *
 * Z-Wave DMADRV compatibility shim
 *
 * The Z-Wave ZPAL library defines a strong LDMA_IRQHandler that only services
 * channel 2 (UART TX DMA) and aggressively clears all interrupt flags.  DMADRV
 * also defines LDMA_IRQHandler for I2C/SPI driver support. We ignore the Z-Wave ZPAL
 * implementation entirely and instead implement it using DMADRV constructs.
 */

#include <stddef.h>

#include "em_ldma.h"
#include "zpal_uart.h"
#include "dmadrv.h"

// `tx_callback` is internal to ZPAL but we still need to call it. To avoid hardcoding
// memory offsets, we capture a reference to `tx_callback` at runtime.
static zpal_uart_transmit_done_t captured_tx_callback = NULL;

extern void __real_zpal_uart_utils_tx_transfer_start(uint8_t *data, size_t length, zpal_uart_transmit_done_t tx_cb);
void __wrap_zpal_uart_utils_tx_transfer_start(uint8_t *data, size_t length, zpal_uart_transmit_done_t tx_cb)
{
    captured_tx_callback = tx_cb;
    __real_zpal_uart_utils_tx_transfer_start(data, length, tx_cb);
}


// Both `LDMA_IRQHandler`s are strong. Instead of weakening one at build time, we
// instead use linker wrapping and just ignore the one in ZPAL entirely.
extern void dmadrv_LDMA_IRQHandler(void);
void __wrap_LDMA_IRQHandler(void)
{
    dmadrv_LDMA_IRQHandler();
}


// Hook up the Z-Wave SDK callback to the DMADRV system
bool zwave_ldma_tx_callback(unsigned int channel, unsigned int sequenceNo, void *userParam)
{
    (void)channel;
    (void)sequenceNo;
    (void)userParam;

    if (captured_tx_callback != NULL) {
        captured_tx_callback(NULL);
    }

    return false;
}

sl_status_t zwave_dmadrv_compat_init()
{
    Ecode_t result = DMADRV_Init();

    if ((result != ECODE_EMDRV_DMADRV_OK) && (result != ECODE_EMDRV_DMADRV_ALREADY_INITIALIZED)) {
        return SL_STATUS_FAIL;
    }

    // Allocate the Z-Wave LDMA channel before any other drivers
    result = DMADRV_AllocateChannelById(ZWAVE_LDMA_CHANNEL, NULL);
    if (result != ECODE_EMDRV_DMADRV_OK) {
        return SL_STATUS_FAIL;
    }

    // Start a fake transfer to inject our callback. The callback persists indefinitely.
    static uint8_t dummy_data = 0;
    result = DMADRV_MemoryPeripheral(ZWAVE_LDMA_CHANNEL,
                                     0,
                                     &dummy_data,
                                     &dummy_data,
                                     false,
                                     1,
                                     ldmaCtrlSizeByte,
                                     zwave_ldma_tx_callback,
                                     NULL);

    if (result != ECODE_EMDRV_DMADRV_OK) {
        return SL_STATUS_FAIL;
    }

    return SL_STATUS_OK;
}
