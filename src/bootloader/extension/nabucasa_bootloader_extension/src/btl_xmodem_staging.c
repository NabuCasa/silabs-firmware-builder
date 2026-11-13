/*
 * btl_xmodem_staging.c
 *
 * Stage XMODEM uploads in storage slot 0 and install them from there
 *
 * The stock XMODEM bootloader writes the application while the GBL file is still
 * arriving, so an interrupted upload leaves no bootable application. Here, every
 * XMODEM packet is copied into the slot as is. The GBL parser still runs on the
 * stream, which keeps the signature check and error reporting of the stock upload,
 * but its application and bootloader writes are dropped. Once the upload has
 * verified, "run" enters the bootloader again, and the storage path installs the
 * image from the slot.
 *
 * A bootloader-only file is unpacked into the slot's last BTL_MAIN_STAGE_MAX_SIZE
 * bytes, so the application stays intact. A file that also holds an application
 * unpacks its bootloader at the SDK default upgrade location in the application
 * area. This happens only during the install, which replaces the old application.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "em_device.h"
#include "btl_storage_slot_cfg.h"
#include "api/btl_errorcode.h"
#include "api/btl_interface.h"
#include "api/btl_reset_info.h"
#include "communication/xmodem-parser/btl_xmodem.h"
#include "core/btl_bootload.h"
#include "core/btl_parse.h"
#include "core/btl_reset.h"
#include "parser/gbl/btl_gbl_parser.h"
#include "storage/btl_storage.h"

#if BTL_STORAGE_NUM_SLOTS != 1
#error "XMODEM staging supports a single storage slot only"
#endif

#define STAGING_SLOT 0UL

// The slot tail holds a bootloader-only upgrade while the SE copies it into place
#define SLOT_TAIL_SIZE    BTL_MAIN_STAGE_MAX_SIZE
#define SLOT_TAIL_ADDRESS (SLOT0_START + SLOT0_SIZE - SLOT_TAIL_SIZE)

_Static_assert(SLOT_TAIL_ADDRESS % FLASH_PAGE_SIZE == 0, "Slot tail must start on a page boundary");
_Static_assert(SLOT_TAIL_ADDRESS > BTL_UPGRADE_LOCATION, "Slot tail must lie above the default upgrade location");

typedef enum {
  SLOT_INVALID,
  SLOT_REFUSED,
  SLOT_DONE,
  SLOT_INSTALL,
  SLOT_INSTALL_FROM_TAIL,
} SlotAction_t;

static bool staging = false;
static uint32_t staged_bytes = 0;
static bool stage_in_tail = false;

static void enter_bootloader(void)
{
  // Clear the reset counter because the next boot only honors the reset reason without it
  reset_disableResetCounter();
  reset_resetWithReason(BOOTLOADER_RESET_REASON_BOOTLOAD);
}

static void erase_slot_header(void)
{
  storage_eraseRaw(SLOT0_START, FLASH_PAGE_SIZE);
}

static int32_t stage_packet(const XmodemPacket_t *packet)
{
  BootloaderStorageSlot_t slot;
  int32_t ret = storage_getSlotInfo(STAGING_SLOT, &slot);
  if (ret != BOOTLOADER_OK) {
    return ret;
  }

  if (staged_bytes + XMODEM_DATA_SIZE > slot.length) {
    return BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS;
  }

  uint32_t address = slot.address + staged_bytes;
  if (address % FLASH_PAGE_SIZE == 0) {
    ret = storage_eraseRaw(address, FLASH_PAGE_SIZE);
    if (ret != BOOTLOADER_OK) {
      return ret;
    }
  }

  // Flash writes need a word-aligned source, but the payload sits at offset 3 of the packet
  uint32_t data[XMODEM_DATA_SIZE / sizeof(uint32_t)];
  memcpy(data, packet->data, XMODEM_DATA_SIZE);

  ret = storage_writeRaw(address, (const uint8_t *)data, XMODEM_DATA_SIZE);
  if (ret != BOOTLOADER_OK) {
    return ret;
  }

  staged_bytes += XMODEM_DATA_SIZE;
  return BOOTLOADER_OK;
}

extern void __real_xmodem_reset(void);
void __wrap_xmodem_reset(void)
{
  staging = true;
  staged_bytes = 0;

  // Invalidate the previous upload, because the menu could still run it if this one never starts
  erase_slot_header();

  __real_xmodem_reset();
}

extern int32_t __real_xmodem_parsePacket(const XmodemPacket_t *packet, uint8_t *response);
int32_t __wrap_xmodem_parsePacket(const XmodemPacket_t *packet, uint8_t *response)
{
  int32_t ret = __real_xmodem_parsePacket(packet, response);

  // Duplicates and bad packets return an error here and are not passed to the parser either
  if (ret != BOOTLOADER_OK || packet->header != XMODEM_CMD_SOH) {
    return ret;
  }

  ret = stage_packet(packet);
  if (ret != BOOTLOADER_OK) {
    *response = XMODEM_CMD_CAN;
  }

  return ret;
}

extern void __real_bootload_applicationCallback(uint32_t address, uint8_t data[], size_t length, void *context);
void __wrap_bootload_applicationCallback(uint32_t address, uint8_t data[], size_t length, void *context)
{
  if (staging) {
    return;
  }

  __real_bootload_applicationCallback(address, data, length, context);
}

extern void __real_bootload_bootloaderCallback(uint32_t offset, uint8_t data[], size_t length, void *context);
void __wrap_bootload_bootloaderCallback(uint32_t offset, uint8_t data[], size_t length, void *context)
{
  if (staging) {
    return;
  }

  if (stage_in_tail) {
    // Keep writes inside the tail because the real callback only stops them at the end of flash
    if (offset > SLOT_TAIL_SIZE || length > SLOT_TAIL_SIZE - offset) {
      return;
    }

    // The shifted offset is never 0, so the real callback also keeps the application's first page
    offset += SLOT_TAIL_ADDRESS - BTL_UPGRADE_LOCATION;
  }

  __real_bootload_bootloaderCallback(offset, data, length, context);
}

extern bool __real_bootload_commitBootloaderUpgrade(uint32_t upgradeAddress, uint32_t size);
bool __wrap_bootload_commitBootloaderUpgrade(uint32_t upgradeAddress, uint32_t size)
{
  // Reboot into the storage install because the XMODEM menu commits before anything is unpacked
  if (staging) {
    enter_bootloader();
    return false;
  }

  if (stage_in_tail) {
    upgradeAddress = SLOT_TAIL_ADDRESS;
  }

  return __real_bootload_commitBootloaderUpgrade(upgradeAddress, size);
}

// SE upgrades are never committed. The storage path refuses them in check_slot().
bool __wrap_bootload_commitSeUpgrade(uint32_t upgradeAddress)
{
  (void) upgradeAddress;

  if (staging) {
    enter_bootloader();
  }

  return false;
}

extern void __real_reset_resetWithReason(uint16_t resetReason);
void __wrap_reset_resetWithReason(uint16_t resetReason)
{
  // The XMODEM menu only asks to run the application after an upload has verified
  if (staging && resetReason == BOOTLOADER_RESET_REASON_GO) {
    enter_bootloader();
  }

  __real_reset_resetWithReason(resetReason);
}

// Kept out of line so its parser context is off the stack before the real install allocates its own
__attribute__((noinline)) static SlotAction_t check_slot(void)
{
  BootloaderParserContext_t context;
  int32_t ret = storage_initParseSlot(STAGING_SLOT, &context, sizeof(context));
  if (ret != BOOTLOADER_OK) {
    return SLOT_INVALID;
  }

  do {
    ret = storage_verifySlot(&context, NULL);
  } while (ret == BOOTLOADER_ERROR_PARSE_CONTINUE);

  if (ret != BOOTLOADER_ERROR_PARSE_SUCCESS) {
    return SLOT_INVALID;
  }

  const ImageProperties_t *image = &context.imageProperties;
  bool has_application = image->contents & BTL_IMAGE_CONTENT_APPLICATION;
  bool has_new_bootloader = (image->contents & BTL_IMAGE_CONTENT_BOOTLOADER)
                            && image->bootloaderVersion > bootload_getBootloaderVersion();

  // Refuse SE upgrades because they are unpacked at BTL_UPGRADE_LOCATION, inside the application
  if (image->contents & BTL_IMAGE_CONTENT_SE) {
    return SLOT_REFUSED;
  }

  if (has_application) {
    return SLOT_INSTALL;
  }

  if (!has_new_bootloader) {
    return SLOT_DONE;
  }

  // `slotOffset` is the end of what the parser reads. Unpacking into the tail must not overwrite it.
  if (image->bootloaderUpgradeSize > SLOT_TAIL_SIZE
      || context.slotOffset > SLOT_TAIL_ADDRESS - SLOT0_START) {
    return SLOT_REFUSED;
  }

  return SLOT_INSTALL_FROM_TAIL;
}

extern int32_t __real_storage_main(void);
int32_t __wrap_storage_main(void)
{
  switch (check_slot()) {
    // Erase a file that can never install, so later entries skip verifying it
    case SLOT_INVALID:
      if (*(const volatile uint32_t *)SLOT0_START != 0xFFFFFFFFUL) {
        erase_slot_header();
      }
      return BOOTLOADER_ERROR_STORAGE_BOOTLOAD;

    case SLOT_REFUSED:
      erase_slot_header();
      return BOOTLOADER_ERROR_STORAGE_BOOTLOAD;

    // Nothing is left to install, e.g. after the SE has applied a bootloader-only upgrade
    case SLOT_DONE:
      erase_slot_header();
      return BOOTLOADER_OK;

    case SLOT_INSTALL_FROM_TAIL:
      stage_in_tail = true;
      break;

    case SLOT_INSTALL:
      break;
  }

  int32_t ret = __real_storage_main();
  stage_in_tail = false;

  // The bootloader installs any valid image in the slot whenever it is entered
  if (ret == BOOTLOADER_OK) {
    erase_slot_header();
  }

  return ret;
}
