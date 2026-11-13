/**
 * @file cmd_firmware_update.c
 * @brief Z-Wave Host API Firmware Update Command (FUNC_ID_FIRMWARE_UPDATE, 0x3E)
 *
 * The host writes a GBL file into the bootloader's storage slot while the
 * controller keeps running. Perform verifies the file, checks it against the
 * requested target and reboots into the bootloader, which installs it.
 *
 * HOST->ZW: 0x3E | 0x00 (Prepare) | sessionId
 * HOST->ZW: 0x3E | 0x01 (Write) | offset[4, MSB first] | length[2, MSB first] | data[length] | sessionId
 * HOST->ZW: 0x3E | 0x02 (Perform) | target | sessionId
 * ZW->HOST (RES): 0x3E | subCommand | accepted
 * ZW->HOST (REQ): 0x3E | sessionId | status
 *
 * The response is the generic Response Status field: 0x01 accepts the request
 * and 0x00 rejects it. Only frames that can't be parsed and requests that
 * arrive while the reboot is pending are rejected. An accepted request reports
 * its result in the callback, as a value from the firmware update status table.
 * Rejected requests and session ID 0 get no callback.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "em_device.h"
#include "app.h"
#include "btl_interface.h"
#include "btl_interface_storage.h"
#include "cmd_handlers.h"
#include "comm_interface.h"
#include "AppTimer.h"
#include "SwTimer.h"
#include "zpal_bootloader.h"
#include "zpal_watchdog.h"

#ifndef FUNC_ID_FIRMWARE_UPDATE
#define FUNC_ID_FIRMWARE_UPDATE 0x3E
#endif

#define FW_UPDATE_SLOT_ID 0

#define FW_UPDATE_WRITE_HEADER_LEN 7 // subCommand + offset + length

// The write buffer holds up to 3 bytes from the previous chunk plus one full chunk
#define FW_UPDATE_WRITE_BUFFER_SIZE (3 + RECEIVE_BUFFER_SIZE)

// The bootloader unpacks bootloader-only files into this many bytes at the slot end.
// The value must match BTL_MAIN_STAGE_MAX_SIZE of the bootloader.
#define FW_UPDATE_SLOT_TAIL_SIZE 0x6000UL

// Values from the series 2 GBL parser, which doesn't export them to applications
#define BTL_IMAGE_CONTENT_APPLICATION 0x01U
#define BTL_IMAGE_CONTENT_BOOTLOADER  0x02U
#define BTL_IMAGE_CONTENT_SE          0x04U

// `ImageProperties_t` of the series 2 GBL parser, which starts the verification context.
// The offsets are taken from the DWARF info of bootloader 3.2.2 (Simplicity SDK 2026.6.1).
typedef struct
{
  uint8_t contents;
  uint8_t instructions;
  bool imageCompleted;
  bool imageVerified;
  uint32_t bootloaderVersion;
  ApplicationData_t application;
  uint32_t bootloaderUpgradeSize;
} staged_image_properties_t;

_Static_assert(offsetof(staged_image_properties_t, bootloaderVersion) == 4, "ImageProperties_t layout");
_Static_assert(offsetof(staged_image_properties_t, application) == 8, "ImageProperties_t layout");
_Static_assert(offsetof(staged_image_properties_t, bootloaderUpgradeSize) == 36, "ImageProperties_t layout");
_Static_assert(sizeof(staged_image_properties_t) <= BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE, "context size");

#define FW_UPDATE_REBOOT_POLL_MS 10
// The callback and its MAX_SERIAL_RETRY retransmissions take up to 6 s at the 1.5 s ACK timeout
#define FW_UPDATE_REBOOT_MAX_WAIT_MS 10000

typedef enum
{
  FW_UPDATE_CMD_PREPARE = 0x00,
  FW_UPDATE_CMD_WRITE = 0x01,
  FW_UPDATE_CMD_PERFORM = 0x02,
} fw_update_cmd_t;

typedef enum
{
  FW_UPDATE_TARGET_FIRMWARE = 0x01,
  FW_UPDATE_TARGET_BOOTLOADER = 0x02,
} fw_update_target_t;

typedef enum
{
  FW_UPDATE_STATUS_OK = 0x00,
  FW_UPDATE_STATUS_TOO_BIG = 0x01,
  FW_UPDATE_STATUS_FIRMWARE_UPDATE_NOT_SUPPORTED = 0x02,
  FW_UPDATE_STATUS_BOOTLOADER_UPDATE_NOT_SUPPORTED = 0x03,
  FW_UPDATE_STATUS_WRONG_CHECKSUM = 0x04,
  FW_UPDATE_STATUS_INVALID_HEADER = 0x05,
  FW_UPDATE_STATUS_INVALID_SIGNATURE = 0x06,
  FW_UPDATE_STATUS_FIRMWARE_MISMATCH = 0x07,
  FW_UPDATE_STATUS_HARDWARE_MISMATCH = 0x08,
  FW_UPDATE_STATUS_DOWNGRADE_NOT_SUPPORTED = 0x09,
  FW_UPDATE_STATUS_UNSUPPORTED_TARGET = 0x0A,
  FW_UPDATE_STATUS_FAILED_TO_PREPARE = 0x0B,
  FW_UPDATE_STATUS_NOT_SUPPORTED = 0xFF,
} fw_update_status_t;

typedef enum
{
  FW_UPDATE_STATE_IDLE,
  // Prepare erased the slot. Write accepts chunks.
  FW_UPDATE_STATE_WRITING,
  // Perform wrote the last partial word. A failed Perform erases the file and returns to IDLE.
  FW_UPDATE_STATE_FINALIZED,
  // The reboot into the bootloader is pending. Every request is rejected.
  FW_UPDATE_STATE_REBOOTING,
} fw_update_state_t;

static fw_update_state_t update_state = FW_UPDATE_STATE_IDLE;
static uint32_t slot_address;
static uint32_t slot_length;
// Number of file bytes accepted so far. Chunks must continue from here.
static uint32_t received_length;
// Bytes after the last full flash word. They are written once 4 bytes are complete.
static uint8_t pending_length;
static uint8_t write_buffer[FW_UPDATE_WRITE_BUFFER_SIZE] __attribute__((aligned(4)));
static uint8_t verify_context[BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE] __attribute__((aligned(4)));

static SSwTimer reboot_timer;
static bool reboot_timer_registered = false;
static uint32_t reboot_wait_ms;

#define FW_UPDATE_RESPONSE_REJECTED 0x00
#define FW_UPDATE_RESPONSE_ACCEPTED 0x01

static void reject(uint8_t sub_command)
{
  const uint8_t response[] = { sub_command, FW_UPDATE_RESPONSE_REJECTED };
  Respond(FUNC_ID_FIRMWARE_UPDATE, response, sizeof(response));
}

static void accept(uint8_t sub_command, uint8_t session_id, fw_update_status_t status)
{
  const uint8_t response[] = { sub_command, FW_UPDATE_RESPONSE_ACCEPTED };
  Respond(FUNC_ID_FIRMWARE_UPDATE, response, sizeof(response));

  if (session_id != 0) {
    const uint8_t callback[] = { session_id, (uint8_t)status };
    Request(FUNC_ID_FIRMWARE_UPDATE, (uint8_t *)callback, sizeof(callback));
  }
}

static fw_update_status_t prepare(void)
{
  update_state = FW_UPDATE_STATE_IDLE;

  // Bootloader 3.2.1 and older have no storage, and their table has no storage functions
  BootloaderInformation_t info = { 0 };
  bootloader_getInfo(&info);
  if ((info.capabilities & BOOTLOADER_CAPABILITY_STORAGE) == 0) {
    return FW_UPDATE_STATUS_FIRMWARE_UPDATE_NOT_SUPPORTED;
  }

  if (zpal_bootloader_init() != ZPAL_STATUS_OK) {
    return FW_UPDATE_STATUS_FAILED_TO_PREPARE;
  }

  BootloaderStorageSlot_t slot = { 0 };
  if (bootloader_getStorageSlotInfo(FW_UPDATE_SLOT_ID, &slot) != BOOTLOADER_OK || slot.length == 0) {
    return FW_UPDATE_STATUS_FIRMWARE_UPDATE_NOT_SUPPORTED;
  }

  if (zpal_bootloader_erase_storage_slot() != ZPAL_STATUS_OK) {
    return FW_UPDATE_STATUS_FAILED_TO_PREPARE;
  }

  slot_address = slot.address;
  slot_length = slot.length;
  received_length = 0;
  pending_length = 0;
  update_state = FW_UPDATE_STATE_WRITING;
  return FW_UPDATE_STATUS_OK;
}

// Write `length` bytes from `write_buffer` at slot offset `offset`
static bool write_words(uint32_t offset, uint16_t length)
{
  // The internal flash storage driver rejects writes whose address or length is not a multiple of 4.
  // zpal_bootloader_write_data() splits writes into 255-byte parts, so `length` must also stay below 256.
  return zpal_bootloader_write_data(offset, write_buffer, length) == ZPAL_STATUS_OK;
}

static fw_update_status_t write_chunk(uint32_t offset, const uint8_t *data, uint16_t length)
{
  if (update_state != FW_UPDATE_STATE_WRITING) {
    return FW_UPDATE_STATUS_FAILED_TO_PREPARE;
  }

  if ((offset > slot_length) || (length > slot_length - offset)) {
    return FW_UPDATE_STATUS_TOO_BIG;
  }

  // Flash can't be rewritten without an erase, so chunks must arrive in order
  if (offset > received_length) {
    return FW_UPDATE_STATUS_TOO_BIG;
  }

  // Skip bytes the host sent again, for example after a lost response
  uint32_t skip = received_length - offset;
  if (skip >= length) {
    return FW_UPDATE_STATUS_OK;
  }
  data += skip;
  length -= skip;

  memcpy(&write_buffer[pending_length], data, length);
  uint16_t buffered = pending_length + length;
  uint16_t aligned = buffered & ~3U;
  uint32_t flash_offset = received_length - pending_length;

  if (aligned > 0) {
    if (!write_words(flash_offset, aligned)) {
      update_state = FW_UPDATE_STATE_IDLE;
      return FW_UPDATE_STATUS_FAILED_TO_PREPARE;
    }
    memmove(write_buffer, &write_buffer[aligned], buffered - aligned);
  }

  pending_length = buffered - aligned;
  received_length += length;
  return FW_UPDATE_STATUS_OK;
}

static fw_update_status_t finalize_write(void)
{
  if (update_state == FW_UPDATE_STATE_FINALIZED) {
    return FW_UPDATE_STATUS_OK;
  }
  if (update_state != FW_UPDATE_STATE_WRITING) {
    return FW_UPDATE_STATUS_FAILED_TO_PREPARE;
  }

  if (pending_length > 0) {
    // Pad with the erased flash value, which the GBL parser never reads past the end tag
    memset(&write_buffer[pending_length], 0xFF, 4 - pending_length);
    if (!write_words(received_length - pending_length, 4)) {
      update_state = FW_UPDATE_STATE_IDLE;
      return FW_UPDATE_STATUS_FAILED_TO_PREPARE;
    }
    pending_length = 0;
  }

  update_state = FW_UPDATE_STATE_FINALIZED;
  return FW_UPDATE_STATUS_OK;
}

static fw_update_status_t map_verify_error(int32_t error)
{
  switch (error) {
    case BOOTLOADER_ERROR_PARSER_SIGNATURE:
    case BOOTLOADER_ERROR_PARSER_KEYERROR:
    case BOOTLOADER_ERROR_SECURITY_REJECTED:
      return FW_UPDATE_STATUS_INVALID_SIGNATURE;
    case BOOTLOADER_ERROR_PARSER_CRC:
      return FW_UPDATE_STATUS_WRONG_CHECKSUM;
    default:
      return FW_UPDATE_STATUS_INVALID_HEADER;
  }
}

// Call the bootloader API directly because zpal_bootloader_verify_image() returns only OK or FAIL
static fw_update_status_t verify_image(void)
{
  int32_t result = bootloader_initVerifyImage(FW_UPDATE_SLOT_ID, verify_context, sizeof(verify_context));
  if (result != BOOTLOADER_OK) {
    return FW_UPDATE_STATUS_INVALID_HEADER;
  }

  do {
    zpal_feed_watchdog();
    result = bootloader_continueVerifyImage(verify_context, NULL);
  } while (result == BOOTLOADER_ERROR_PARSE_CONTINUE);

  if (result != BOOTLOADER_ERROR_PARSE_SUCCESS) {
    return map_verify_error(result);
  }
  return FW_UPDATE_STATUS_OK;
}

static fw_update_status_t check_contents(uint8_t target)
{
  const staged_image_properties_t *image = (const staged_image_properties_t *)verify_context;

  // Compare with the public API to make sure the parser context still has this layout
  ApplicationData_t app_info;
  uint32_t bootloader_version = 0;
  if (bootloader_getImageInfo(FW_UPDATE_SLOT_ID, &app_info, &bootloader_version) != BOOTLOADER_OK) {
    return FW_UPDATE_STATUS_INVALID_HEADER;
  }
  bool has_app = image->contents & BTL_IMAGE_CONTENT_APPLICATION;
  bool has_bootloader = image->contents & BTL_IMAGE_CONTENT_BOOTLOADER;
  if (!image->imageCompleted || !image->imageVerified
      || (has_bootloader ? image->bootloaderVersion : 0) != bootloader_version
      || (has_app && memcmp(&image->application, &app_info, sizeof(app_info)) != 0)) {
    return FW_UPDATE_STATUS_INVALID_HEADER;
  }

  // The bootloader refuses SE upgrades and stops in its menu
  if (image->contents & BTL_IMAGE_CONTENT_SE) {
    return FW_UPDATE_STATUS_UNSUPPORTED_TARGET;
  }

  switch (target) {
    case FW_UPDATE_TARGET_FIRMWARE:
      return (has_app && !has_bootloader) ? FW_UPDATE_STATUS_OK : FW_UPDATE_STATUS_UNSUPPORTED_TARGET;
    case FW_UPDATE_TARGET_BOOTLOADER:
      if (!has_bootloader) {
        return FW_UPDATE_STATUS_UNSUPPORTED_TARGET;
      }
      break;
    default:
      return FW_UPDATE_STATUS_UNSUPPORTED_TARGET;
  }

  // A file with an application installs it even when its bootloader isn't newer
  if (has_app) {
    return FW_UPDATE_STATUS_OK;
  }

  // The bootloader skips a bootloader-only file unless it is newer
  BootloaderInformation_t info = { 0 };
  bootloader_getInfo(&info);
  if (image->bootloaderVersion <= info.version) {
    return FW_UPDATE_STATUS_DOWNGRADE_NOT_SUPPORTED;
  }

  // The bootloader unpacks a bootloader-only file into the slot tail. It refuses the file when the
  // bootloader is larger than the tail or when the parser read past the tail start. The parser reads
  // in 128-byte blocks, and the tail start is a multiple of 128, so the file length gives the same result.
  if (image->bootloaderUpgradeSize > FW_UPDATE_SLOT_TAIL_SIZE
      || received_length > slot_length - FW_UPDATE_SLOT_TAIL_SIZE) {
    return FW_UPDATE_STATUS_TOO_BIG;
  }

  return FW_UPDATE_STATUS_OK;
}

static fw_update_status_t perform(uint8_t target)
{
  fw_update_status_t status = finalize_write();
  if (status != FW_UPDATE_STATUS_OK) {
    return status;
  }

  status = verify_image();
  if (status == FW_UPDATE_STATUS_OK) {
    status = check_contents(target);
  }

  // Erase the start of a refused file because the bootloader installs any valid file on its next entry
  if (status != FW_UPDATE_STATUS_OK) {
    bootloader_eraseRawStorage(slot_address, FLASH_PAGE_SIZE);
    update_state = FW_UPDATE_STATE_IDLE;
  }

  return status;
}

static void reboot_timer_callback(__attribute__((unused)) SSwTimer *timer)
{
  // The callback leaves the queue once the host ACKed it or all retransmissions failed
  if ((GetCallbackCnt() > 0) && (reboot_wait_ms < FW_UPDATE_REBOOT_MAX_WAIT_MS)) {
    reboot_wait_ms += FW_UPDATE_REBOOT_POLL_MS;
    TimerStart(&reboot_timer, FW_UPDATE_REBOOT_POLL_MS);
    return;
  }

  comm_interface_wait_transmit_done();
  zpal_bootloader_reboot_and_install();
}

static void schedule_reboot(void)
{
  update_state = FW_UPDATE_STATE_REBOOTING;

  if (!reboot_timer_registered) {
    reboot_timer_registered = AppTimerRegister(&reboot_timer, false, reboot_timer_callback);
  }

  if (!reboot_timer_registered) {
    comm_interface_wait_transmit_done();
    zpal_bootloader_reboot_and_install();
    return;
  }

  // Reboot no earlier than one poll interval later, so the host can ACK the response
  reboot_wait_ms = 0;
  TimerStart(&reboot_timer, FW_UPDATE_REBOOT_POLL_MS);
}

ZW_ADD_CMD(FUNC_ID_FIRMWARE_UPDATE)
{
  const uint8_t length = frame_payload_len(frame);
  const uint8_t *payload = frame->payload;

  if (length < 1) {
    reject(FW_UPDATE_STATUS_NOT_SUPPORTED);
    return;
  }

  const uint8_t sub_command = payload[0];

  if (update_state == FW_UPDATE_STATE_REBOOTING) {
    reject(sub_command);
    return;
  }

  switch (sub_command) {
    case FW_UPDATE_CMD_PREPARE:
    {
      if (length != 2) {
        break;
      }
      accept(sub_command, payload[1], prepare());
      return;
    }

    case FW_UPDATE_CMD_WRITE:
    {
      if (length < FW_UPDATE_WRITE_HEADER_LEN + 1) {
        break;
      }
      const uint32_t offset = ((uint32_t)payload[1] << 24) | ((uint32_t)payload[2] << 16)
                              | ((uint32_t)payload[3] << 8) | payload[4];
      const uint16_t data_length = (uint16_t)((payload[5] << 8) | payload[6]);
      if (length != FW_UPDATE_WRITE_HEADER_LEN + data_length + 1) {
        break;
      }
      const uint8_t session_id = payload[FW_UPDATE_WRITE_HEADER_LEN + data_length];
      accept(sub_command, session_id, write_chunk(offset, &payload[FW_UPDATE_WRITE_HEADER_LEN], data_length));
      return;
    }

    case FW_UPDATE_CMD_PERFORM:
    {
      if (length != 3) {
        break;
      }
      const fw_update_status_t status = perform(payload[1]);
      accept(sub_command, payload[2], status);
      if (status == FW_UPDATE_STATUS_OK) {
        schedule_reboot();
      }
      return;
    }

    default:
      // Every defined sub-command ends with the session ID, so unknown ones are assumed to as well
      if (length >= 2) {
        accept(sub_command, payload[length - 1], FW_UPDATE_STATUS_NOT_SUPPORTED);
        return;
      }
      break;
  }

  reject(sub_command);
}
