/*
 * zwave_ldma_wrap.c
 *
 * LDMA IRQ handler for Z-Wave + DMADRV coexistence.
 *
 * The Z-Wave ZPAL library defines a strong LDMA_IRQHandler that only services
 * channel 2 (UART TX DMA) and aggressively clears all interrupt flags.  DMADRV
 * also defines LDMA_IRQHandler for I2C/SPI driver support.
 */

#include "em_ldma.h"
#include <stddef.h>

// Renamed in the SDK patch
extern void dmadrv_LDMA_IRQHandler(void);

typedef void (*zw_callback_t)(int);
extern void *pCurrentExploreQueueElement;

void __wrap_LDMA_IRQHandler(void)
{
    // Run DMADRV handler first for SPI, I2C, etc.
    dmadrv_LDMA_IRQHandler();

    // Run Z-Wave SDK logic manually
    if ((LDMA->IF & LDMA->IEN) & ZWAVE_LDMA_RESERVED_CHANNELS) {
        if (pCurrentExploreQueueElement != NULL) {
            zw_callback_t callback = *(zw_callback_t *)(pCurrentExploreQueueElement + 0x1c);

            if (callback != NULL) {
                callback(0);
            }
        }

        // The entire purpose of this patch is this line. The Z-Wave SDK sets
        // `LDMA->IF_CLR = 0xFFFFFFFF`, which clears ALL handlers instead of just
        // handler 2.
        LDMA->IF_CLR = ZWAVE_LDMA_RESERVED_CHANNELS;
    }
}
