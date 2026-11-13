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
 * The slot holds the GBL file as uploaded, with its application still LZMA
 * compressed. The GBL files must use LZMA because the uncompressed application
 * is larger than the slot.
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

// Store a bootloader-only upgrade at the tail of the slot. The SE copies it into place while leaving the application image alone.
#define SLOT_TAIL_SIZE    BTL_MAIN_STAGE_MAX_SIZE
#define SLOT_TAIL_ADDRESS (SLOT0_START + SLOT0_SIZE - SLOT_TAIL_SIZE)

_Static_assert(SLOT_TAIL_ADDRESS % FLASH_PAGE_SIZE == 0, "Slot tail must start on a page boundary");
_Static_assert(SLOT_TAIL_ADDRESS > BTL_UPGRADE_LOCATION, "Slot tail must lie above the default upgrade location");

// What __wrap_storage_main does with the file in the slot
typedef enum {
  SLOT_INVALID,
  SLOT_REFUSED,
  SLOT_DONE,
  SLOT_INSTALL,
  SLOT_INSTALL_FROM_TAIL,
} SlotAction_t;

// `staging` is set when "1. upload gbl" starts an upload.
// From then on, packets go into the slot and the GBL parser only checks the file.
static bool staging = false;
static uint32_t staged_bytes = 0;
// `stage_in_tail` is true while a bootloader-only file is installed from the slot
static bool stage_in_tail = false;

// Reset into the bootloader, which then installs the file from the slot
static void enter_bootloader(void)
{
  // The SDK enabled the reset counter because __wrap_storage_main returned an error before the menu started.
  // While the counter is enabled, the next boot ignores the reset reason.
  reset_disableResetCounter();
  reset_resetWithReason(BOOTLOADER_RESET_REASON_BOOTLOAD);
}

// Erase the slot's first page. The staged file no longer verifies without it.
static void erase_slot_header(void)
{
  storage_eraseRaw(SLOT0_START, FLASH_PAGE_SIZE);
}

// Append the data of one XMODEM packet to the file in the slot
static int32_t stage_packet(const XmodemPacket_t *packet)
{
  // Refuse data past the end of the slot
  BootloaderStorageSlot_t slot;
  int32_t ret = storage_getSlotInfo(STAGING_SLOT, &slot);
  if (ret != BOOTLOADER_OK) {
    return ret;
  }

  if (staged_bytes + XMODEM_DATA_SIZE > slot.length) {
    return BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS;
  }

  // Erase each page before the first write into it
  uint32_t address = slot.address + staged_bytes;
  if (address % FLASH_PAGE_SIZE == 0) {
    ret = storage_eraseRaw(address, FLASH_PAGE_SIZE);
    if (ret != BOOTLOADER_OK) {
      return ret;
    }
  }

  // Copy the payload into a word-aligned buffer. Flash writes need one, but the payload sits at offset 3 of the packet.
  uint32_t data[XMODEM_DATA_SIZE / sizeof(uint32_t)];
  memcpy(data, packet->data, XMODEM_DATA_SIZE);

  // Write the payload
  ret = storage_writeRaw(address, (const uint8_t *)data, XMODEM_DATA_SIZE);
  if (ret != BOOTLOADER_OK) {
    return ret;
  }

  staged_bytes += XMODEM_DATA_SIZE;
  return BOOTLOADER_OK;
}

// Start a new upload. The XMODEM menu calls this when "1. upload gbl" is selected.
extern void __real_xmodem_reset(void);
void __wrap_xmodem_reset(void)
{
  staging = true;
  staged_bytes = 0;

  // "2. run" must only install a file from this upload, so erase any file from an earlier one
  erase_slot_header();

  __real_xmodem_reset();
}

// Parse an XMODEM packet as usual, then copy its data into the slot
extern int32_t __real_xmodem_parsePacket(const XmodemPacket_t *packet, uint8_t *response);
int32_t __wrap_xmodem_parsePacket(const XmodemPacket_t *packet, uint8_t *response)
{
  int32_t ret = __real_xmodem_parsePacket(packet, response);

  // __real_xmodem_parsePacket returns an error for duplicates and bad packets.
  // These don't reach the GBL parser either. Only data packets (SOH) are staged.
  if (ret != BOOTLOADER_OK || packet->header != XMODEM_CMD_SOH) {
    return ret;
  }

  // Cancel the transfer if the packet can't be staged
  ret = stage_packet(packet);
  if (ret != BOOTLOADER_OK) {
    *response = XMODEM_CMD_CAN;
  }

  return ret;
}

// The GBL parser calls this to write each chunk of application data to flash.
// During an XMODEM upload, the parser only checks the file, so drop the chunk.
// The install from the slot parses the file again and writes the application then.
extern void __real_bootload_applicationCallback(uint32_t address, uint8_t data[], size_t length, void *context);
void __wrap_bootload_applicationCallback(uint32_t address, uint8_t data[], size_t length, void *context)
{
  if (staging) {
    return;
  }

  __real_bootload_applicationCallback(address, data, length, context);
}

// The GBL parser calls this to write each chunk of a new bootloader to flash.
// During an XMODEM upload, drop the chunk like __wrap_bootload_applicationCallback does.
// When a bootloader-only file is installed, move the chunk into the slot tail.
extern void __real_bootload_bootloaderCallback(uint32_t offset, uint8_t data[], size_t length, void *context);
void __wrap_bootload_bootloaderCallback(uint32_t offset, uint8_t data[], size_t length, void *context)
{
  if (staging) {
    return;
  }

  if (stage_in_tail) {
    // Prevent overflowing into the NVM
    if (offset > SLOT_TAIL_SIZE || length > SLOT_TAIL_SIZE - offset) {
      return;
    }

    // __real_bootload_bootloaderCallback erases the application's first page when offset is 0.
    // This is never the case here.
    offset += SLOT_TAIL_ADDRESS - BTL_UPGRADE_LOCATION;
  }

  __real_bootload_bootloaderCallback(offset, data, length, context);
}

// Ask the SE to copy a new bootloader into place. During an upload, reboot into the bootloader instead.
extern bool __real_bootload_commitBootloaderUpgrade(uint32_t upgradeAddress, uint32_t size);
bool __wrap_bootload_commitBootloaderUpgrade(uint32_t upgradeAddress, uint32_t size)
{
  // "2. run" commits a newer bootloader directly. It expects the bootloader at BTL_UPGRADE_LOCATION already.
  // After an upload, the new file is only in the slot. Reboot into the bootloader instead, so __wrap_storage_main unpacks and commits it.
  if (staging) {
    enter_bootloader();
    return false;
  }

  // Commit from the slot tail, where a bootloader-only file was unpacked
  if (stage_in_tail) {
    upgradeAddress = SLOT_TAIL_ADDRESS;
  }

  return __real_bootload_commitBootloaderUpgrade(upgradeAddress, size);
}

// Never commit an SE upgrade. check_slot() refuses files with SE content before they are installed.
// Like __wrap_bootload_commitBootloaderUpgrade, reboot into the bootloader after an upload. check_slot() then refuses the file.
bool __wrap_bootload_commitSeUpgrade(uint32_t upgradeAddress)
{
  (void) upgradeAddress;

  if (staging) {
    enter_bootloader();
  }

  return false;
}

// "2. run" in the XMODEM menu resets with reason GO, which boots the application.
// After an upload, the new file is only in the slot. Reboot into the bootloader instead, so __wrap_storage_main installs it.
extern void __real_reset_resetWithReason(uint16_t resetReason);
void __wrap_reset_resetWithReason(uint16_t resetReason)
{
  if (staging && resetReason == BOOTLOADER_RESET_REASON_GO) {
    enter_bootloader();
  }

  __real_reset_resetWithReason(resetReason);
}

// Verify the file in the slot and decide what to do with it.
// check_slot must not be inlined. Its parser context has to be off the stack before __real_storage_main allocates its own.
__attribute__((noinline)) static SlotAction_t check_slot(void)
{
  // Verify the whole file
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

  // Decide from the file's contents
  const ImageProperties_t *image = &context.imageProperties;
  bool has_application = image->contents & BTL_IMAGE_CONTENT_APPLICATION;
  bool has_new_bootloader = (image->contents & BTL_IMAGE_CONTENT_BOOTLOADER)
                            && image->bootloaderVersion > bootload_getBootloaderVersion();

  // Refuse SE upgrades. The SDK unpacks them at BTL_UPGRADE_LOCATION, which is inside the application.
  if (image->contents & BTL_IMAGE_CONTENT_SE) {
    return SLOT_REFUSED;
  }

  if (has_application) {
    return SLOT_INSTALL;
  }

  if (!has_new_bootloader) {
    return SLOT_DONE;
  }

  // Refuse a bootloader that doesn't fit the tail.
  // The parser reads the file while it writes the tail, so the file must end before the tail starts.
  // `slotOffset` is where the file ends.
  if (image->bootloaderUpgradeSize > SLOT_TAIL_SIZE
      || context.slotOffset > SLOT_TAIL_ADDRESS - SLOT0_START) {
    return SLOT_REFUSED;
  }

  return SLOT_INSTALL_FROM_TAIL;
}

// Install the file in the slot if there is one. The bootloader calls this on every entry, before the XMODEM menu.
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

  // Install the file
  int32_t ret = __real_storage_main();
  stage_in_tail = false;

  // Erase the file after a successful install. The bootloader installs any valid file in the slot whenever it is entered.
  if (ret == BOOTLOADER_OK) {
    erase_slot_header();
  }

  return ret;
}
