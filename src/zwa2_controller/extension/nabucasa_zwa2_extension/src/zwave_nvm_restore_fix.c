/*
 * zwave_nvm_restore_fix.c
 *
 * Keep NVM restores from erasing the flash page after the NVM
 *
 * Before writing, ZPAL's zpal_nvm_backup_write erases the page a write crosses into.
 * It treats the first byte after the write as part of the write. A write that ends
 * exactly on a page boundary therefore erases the following page. For a write that
 * ends at the end of the NVM, that page holds the manufacturing tokens and bootloader
 * keys. Every full NVM restore over the Serial API ends with such a write.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "em_device.h"
#include "nvm3_default.h"
#include "zpal_nvm.h"

#define WORD_SIZE sizeof(uint32_t)

extern zpal_status_t __real_zpal_nvm_backup_write(uint32_t offset, const void *data, size_t data_length);
zpal_status_t __wrap_zpal_nvm_backup_write(uint32_t offset, const void *data, size_t data_length)
{
    const size_t nvm_size = zpal_nvm_backup_get_size();
    const bool ends_at_nvm_end = data != NULL
                                 && offset < nvm_size
                                 && data_length == nvm_size - offset;

    // ZPAL erases the start page of a write that starts on a page boundary, so only a
    // write that starts inside a page reaches the page after the NVM
    if (!ends_at_nvm_end || offset % FLASH_PAGE_SIZE == 0) {
        return __real_zpal_nvm_backup_write(offset, data, data_length);
    }

    // Reject an unaligned write here because ZPAL erases the page after the NVM before
    // the flash refuses the write
    if (offset % WORD_SIZE != 0) {
        return ZPAL_STATUS_FAIL;
    }

    // Leave out the last word so ZPAL's write ends inside the last NVM page
    const size_t head_length = data_length - WORD_SIZE;

    if (head_length > 0) {
        zpal_status_t status = __real_zpal_nvm_backup_write(offset, data, head_length);

        if (status != ZPAL_STATUS_OK) {
            return status;
        }
    }

    uint32_t last_word;
    memcpy(&last_word, (const uint8_t *)data + head_length, WORD_SIZE);

    // Write the last word without erasing because the write that started the last NVM
    // page already erased it
    const nvm3_HalPtr_t address = (uint8_t *)nvm3_defaultInit->nvmAdr + nvm_size - WORD_SIZE;

    if (nvm3_defaultInit->halHandle->writeWords(address, &last_word, 1) != SL_STATUS_OK) {
        return ZPAL_STATUS_FAIL;
    }

    return ZPAL_STATUS_OK;
}
