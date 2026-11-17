/*
 * btl_slot_install.c
 *
 * Adapt the stock bootloader to storage slot 0
 */

#include <stdbool.h>
#include <stdint.h>

#include "em_device.h"
#include "btl_storage_slot_cfg.h"
#include "api/btl_errorcode.h"
#include "api/btl_reset_info.h"
#include "core/btl_bootload.h"
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

// Clear the SDK's reset counter before leaving the XMODEM menu.
// The SDK counts every entry where __wrap_storage_main returns an error, which includes an empty slot.
// After three in a row it stops in a busy loop, so leaving the menu must not count towards it.
// FATAL resets keep counting.
static void clear_reset_counter_unless_fatal(uint16_t resetReason)
{
  if (resetReason != BOOTLOADER_RESET_REASON_FATAL) {
    reset_disableResetCounter();
  }
}

// "run" resets with GO after an upload with nothing to commit, and with BADIMAGE without an upload
extern void __real_reset_resetWithReason(uint16_t resetReason);
void __wrap_reset_resetWithReason(uint16_t resetReason)
{
  clear_reset_counter_unless_fatal(resetReason);
  __real_reset_resetWithReason(resetReason);
}

// "run" commits a newer bootloader, which sets reason BOOTLOAD without reset_resetWithReason
extern bool __real_bootload_commitBootloaderUpgrade(uint32_t upgradeAddress, uint32_t size);
bool __wrap_bootload_commitBootloaderUpgrade(uint32_t upgradeAddress, uint32_t size)
{
  reset_disableResetCounter();
  return __real_bootload_commitBootloaderUpgrade(upgradeAddress, size);
}

// "run" commits a newer SE version the same way
extern bool __real_bootload_commitSeUpgrade(uint32_t upgradeAddress);
bool __wrap_bootload_commitSeUpgrade(uint32_t upgradeAddress)
{
  reset_disableResetCounter();
  return __real_bootload_commitSeUpgrade(upgradeAddress);
}
