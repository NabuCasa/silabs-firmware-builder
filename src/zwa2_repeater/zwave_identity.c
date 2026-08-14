#include "zwave_identity.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "MfgTokens.h"
#include "psa/crypto.h"
#include "zpal_misc.h"

// Controller firmware stores no S2 identity or QR code. After switching to repeater firmware,
// both are missing. Reconcile them in NVM and token storage, so the end device can join a network.
// The protocol uses this fixed PSA key ID for its static S2 identity.
#define ZWAVE_ECC_KEY_ID ((psa_key_id_t)0x70000)

extern void compose_qr_code(bool region_lr,
                            const uint8_t public_key[TOKEN_MFG_ZW_PUK_SIZE],
                            uint8_t qrcode[TOKEN_MFG_ZW_QR_CODE_SIZE]);

static bool all_ff(const uint8_t *data, size_t length)
{
  for (size_t i = 0; i < length; i++) {
    if (data[i] != 0xff) {
      return false;
    }
  }
  return true;
}

static void clear_secret(uint8_t *data, size_t length)
{
  volatile uint8_t *cursor = data;
  while (length-- > 0) {
    *cursor++ = 0;
  }
}

static void set_key_attributes(psa_key_attributes_t *attributes)
{
  psa_set_key_type(attributes,
                   PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
  psa_set_key_bits(attributes, 255);
  psa_set_key_usage_flags(attributes,
                          PSA_KEY_USAGE_EXPORT | PSA_KEY_USAGE_DERIVE);
  psa_set_key_algorithm(attributes, PSA_ALG_ECDH);
}

static bool public_key_matches(mbedtls_svc_key_id_t key,
                               const uint8_t expected[TOKEN_MFG_ZW_PUK_SIZE])
{
  uint8_t actual[TOKEN_MFG_ZW_PUK_SIZE];
  size_t actual_length = 0;

  return psa_export_public_key(key, actual, sizeof(actual), &actual_length)
           == PSA_SUCCESS
         && actual_length == sizeof(actual)
         && memcmp(actual, expected, sizeof(actual)) == 0;
}

static bool validate_token_keypair(
  const uint8_t private_key[TOKEN_MFG_ZW_PRK_SIZE],
  const uint8_t public_key[TOKEN_MFG_ZW_PUK_SIZE])
{
  // Validate the token pair before replacing persistent identity storage.
  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
  mbedtls_svc_key_id_t temporary_key = MBEDTLS_SVC_KEY_ID_INIT;

  set_key_attributes(&attributes);
  zpal_psa_set_location_volatile_key(&attributes);

  psa_status_t status = psa_import_key(&attributes,
                                       private_key,
                                       TOKEN_MFG_ZW_PRK_SIZE,
                                       &temporary_key);
  bool valid = status == PSA_SUCCESS
               && public_key_matches(temporary_key, public_key);

  if (status == PSA_SUCCESS) {
    valid = psa_destroy_key(temporary_key) == PSA_SUCCESS && valid;
  }
  psa_reset_key_attributes(&attributes);
  return valid;
}

static bool reconcile_private_key(
  const uint8_t private_key[TOKEN_MFG_ZW_PRK_SIZE],
  const uint8_t public_key[TOKEN_MFG_ZW_PUK_SIZE])
{
  if (public_key_matches(ZWAVE_ECC_KEY_ID, public_key)) {
    return true;
  }

  psa_status_t status = psa_destroy_key(ZWAVE_ECC_KEY_ID);
  if (status != PSA_SUCCESS && status != PSA_ERROR_DOES_NOT_EXIST) {
    return false;
  }

  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
  mbedtls_svc_key_id_t imported_key = MBEDTLS_SVC_KEY_ID_INIT;
  set_key_attributes(&attributes);
  psa_set_key_id(&attributes, ZWAVE_ECC_KEY_ID);
  zpal_psa_set_location_persistent_key(&attributes);

  status = psa_import_key(&attributes,
                          private_key,
                          TOKEN_MFG_ZW_PRK_SIZE,
                          &imported_key);
  psa_reset_key_attributes(&attributes);

  return status == PSA_SUCCESS
         && imported_key == ZWAVE_ECC_KEY_ID
         && public_key_matches(imported_key, public_key);
}

static bool reconcile_qr_code(
  bool region_lr,
  const uint8_t public_key[TOKEN_MFG_ZW_PUK_SIZE])
{
  uint8_t stored[TOKEN_MFG_ZW_QR_CODE_SIZE];
  ZW_GetMfgTokenData(stored, TOKEN_MFG_ZW_QR_CODE_ID, sizeof(stored));

  // Static token writes are write-once, so repair only a fully erased QR token.
  if (!all_ff(stored, sizeof(stored))) {
    return true;
  }

  uint8_t expected[TOKEN_MFG_ZW_QR_CODE_SIZE];
  compose_qr_code(region_lr, public_key, expected);
  ZW_SetMfgTokenData(TOKEN_MFG_ZW_QR_CODE_ID, expected, sizeof(expected));

  memset(stored, 0xff, sizeof(stored));
  ZW_GetMfgTokenData(stored, TOKEN_MFG_ZW_QR_CODE_ID, sizeof(stored));
  return memcmp(stored, expected, sizeof(stored)) == 0;
}

bool zwave_identity_reconcile(bool region_lr)
{
  uint8_t initialized = 0xff;
  ZW_GetMfgTokenData(&initialized,
                     TOKEN_MFG_ZW_INITIALIZED_ID,
                     sizeof(initialized));

  // While this token is erased, the Z-Wave protocol initializes the QR code on its own.
  if (initialized == 0xff) {
    return true;
  }

  uint8_t private_key[TOKEN_MFG_ZW_PRK_SIZE];
  uint8_t public_key[TOKEN_MFG_ZW_PUK_SIZE];
  ZW_GetMfgTokenData(private_key,
                     TOKEN_MFG_ZW_PRK_ID,
                     sizeof(private_key));
  ZW_GetMfgTokenData(public_key,
                     TOKEN_MFG_ZW_PUK_ID,
                     sizeof(public_key));

  bool success = false;
  if (!all_ff(private_key, sizeof(private_key))
      && !all_ff(public_key, sizeof(public_key))
      && validate_token_keypair(private_key, public_key)) {
    success = reconcile_private_key(private_key, public_key)
              && reconcile_qr_code(region_lr, public_key);
  }

  clear_secret(private_key, sizeof(private_key));
  return success;
}