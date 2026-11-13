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
#include "sl_assert.h"
#include "zpal_nvm.h"

#define WORD_SIZE sizeof(uint32_t)

extern zpal_status_t __real_zpal_nvm_backup_write(uint32_t offset, const void *data, size_t data_length);
zpal_status_t __wrap_zpal_nvm_backup_write(uint32_t offset, const void *data, size_t data_length)
{
    const size_t nvm_size = zpal_nvm_backup_get_size();

    // ZPAL erases the page after a write if the write starts in the middle of a page
    // and ends at a page boundary. Only at the end of the NVM is that page outside the
    // NVM, so call ZPAL directly for every other write.
    const bool starts_on_page_boundary = offset % FLASH_PAGE_SIZE == 0;
    const bool ends_at_nvm_end = data != NULL
                                 && offset < nvm_size
                                 && data_length == nvm_size - offset;

    if (starts_on_page_boundary || !ends_at_nvm_end) {
        return __real_zpal_nvm_backup_write(offset, data, data_length);
    }

    // Assert a word-aligned offset, as MSC_WriteWord does for ZPAL's own writes
    EFM_ASSERT(offset % WORD_SIZE == 0);

    // Hold back the last word of the NVM, so the write ZPAL sees ends inside the last
    // NVM page
    const size_t last_word_offset = nvm_size - WORD_SIZE;
    const size_t head_length = offset < last_word_offset ? last_word_offset - offset : 0;

    if (head_length > 0) {
        zpal_status_t status = __real_zpal_nvm_backup_write(offset, data, head_length);

        if (status != ZPAL_STATUS_OK) {
            return status;
        }
    }

    // The write that started the last NVM page already erased it, so write the last
    // word through NVM3 without an erase. Bytes of the word outside this write stay
    // 0xFF, which leaves the flash under them unchanged.
    const size_t tail_length = data_length - head_length;
    uint32_t last_word = UINT32_MAX;
    memcpy((uint8_t *)&last_word + WORD_SIZE - tail_length, (const uint8_t *)data + head_length, tail_length);

    const nvm3_HalPtr_t address = (uint8_t *)nvm3_defaultInit->nvmAdr + last_word_offset;

    if (nvm3_defaultInit->halHandle->writeWords(address, &last_word, 1) != SL_STATUS_OK) {
        return ZPAL_STATUS_FAIL;
    }

    return ZPAL_STATUS_OK;
}
