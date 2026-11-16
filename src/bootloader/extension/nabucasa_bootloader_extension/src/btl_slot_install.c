/*
 * btl_slot_install.c
 *
 * Adapt the stock bootloader to storage slot 0
 */

#include <stdint.h>

#include "em_device.h"
#include "btl_storage_slot_cfg.h"
#include "api/btl_errorcode.h"
#include "api/btl_reset_info.h"
#include "core/btl_reset.h"
#include "storage/btl_storage.h"

#if BTL_STORAGE_NUM_SLOTS != 1
#error "Slot install supports a single storage slot only"
#endif

// Install the file in the slot, then erase its first page.
// The file no longer verifies without it, so later bootloader entries skip it.
// The bootloader calls this before the XMODEM menu, after a reset into the bootloader,
// with the button held, or when the application is invalid.
extern int32_t __real_storage_main(void);
int32_t __wrap_storage_main(void)
{
  int32_t ret = __real_storage_main();
  if (ret == BOOTLOADER_OK) {
    storage_eraseRaw(SLOT0_START, FLASH_PAGE_SIZE);
  }

  return ret;
}

// Clear the SDK's reset counter when "run" leaves the XMODEM menu.
// The SDK counts every entry where __wrap_storage_main returns an error, which includes an empty slot.
// After three in a row it stops in a busy loop, so the menu must not count towards it.
// "run" resets with GO after a successful upload and with BADIMAGE otherwise. FATAL resets keep counting.
extern void __real_reset_resetWithReason(uint16_t resetReason);
void __wrap_reset_resetWithReason(uint16_t resetReason)
{
  if (resetReason == BOOTLOADER_RESET_REASON_GO || resetReason == BOOTLOADER_RESET_REASON_BADIMAGE) {
    reset_disableResetCounter();
  }

  __real_reset_resetWithReason(resetReason);
}
