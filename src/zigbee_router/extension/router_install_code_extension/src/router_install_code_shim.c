/*
 * Thin shims for the manufacturing install-code token. halCommonGet/SetMfgToken are macros,
 * so bindgen can't bind them; expose the one token's read/write as real functions. All the
 * provisioning logic (CRC, randomness, flags) lives in Rust (crates/ohf-router).
 */
#include <stdint.h>
#include <string.h>
#include "sl_token_api.h"
#include "sl_token_manufacturing_api.h"

#define INSTALL_CODE_VALUE_SIZE 16

uint16_t ohf_mfg_install_code_flags(void)
{
    tokTypeMfgInstallationCode tok;
    halCommonGetMfgToken(&tok, TOKEN_MFG_INSTALLATION_CODE);
    return tok.flags;
}

void ohf_mfg_install_code_set(uint16_t flags, const uint8_t *value, uint16_t crc)
{
    tokTypeMfgInstallationCode tok;
    memset(&tok, 0xFF, sizeof(tok));
    tok.flags = flags;
    memcpy(tok.value, value, INSTALL_CODE_VALUE_SIZE);
    tok.crc = crc;
    halCommonSetMfgToken(TOKEN_MFG_INSTALLATION_CODE, &tok);
}
