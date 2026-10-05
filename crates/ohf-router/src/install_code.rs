//! Provision an install code once. The ZBT-2 ships without one, and R23 routers need it for DLK.
use crate::bindings::{
    halCommonCrc16, psa_crypto_init, psa_generate_random, psa_status_t, PSA_SUCCESS,
};

const INSTALL_CODE_VALUE_SIZE: usize = 16;
// flags: bit0 = 0 (valid), bits1-2 = size code 3 (16 bytes) => 0x0006.
const INSTALL_CODE_FLAGS_16B: u16 = 0x0006;

// The Zigbee install-code CRC and halCommonCrc16 differ in bit and byte order. This matches
// sli_zigbee_af_install_code_to_key(), so the coordinator derives the same key.
fn reverse(b: u8) -> u8 {
    let b = b as u32;
    (((b * 0x0802 & 0x22110) | (b * 0x8020 & 0x88440)) * 0x10101 >> 16) as u8
}

fn install_code_crc(value: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;
    for &byte in value {
        crc = unsafe { halCommonCrc16(reverse(byte), crc) };
    }
    !(((reverse((crc & 0xFF) as u8) as u16) << 8) | reverse(((crc >> 8) & 0xFF) as u8) as u16)
}

#[no_mangle]
pub extern "C" fn router_install_code_init(_init_level: u8) {
    // Only act on a factory-blank token; erased MFG flags read as 0xFFFF.
    let tok: ohf_sys::tokTypeMfgInstallationCode =
        ohf_sys::token::get_mfg(ohf_sys::TOKEN_MFG_INSTALLATION_CODE);
    if tok.flags != 0xFFFF {
        return;
    }

    let mut value = [0u8; INSTALL_CODE_VALUE_SIZE];
    // psa_generate_random blocks until real entropy is available; on any failure bail without
    // writing and retry next boot rather than program a weak code.
    unsafe {
        if psa_crypto_init() != PSA_SUCCESS as psa_status_t {
            return;
        }
        if psa_generate_random(value.as_mut_ptr(), value.len()) != PSA_SUCCESS as psa_status_t {
            return;
        }
    }

    let tok = ohf_sys::tokTypeMfgInstallationCode {
        flags: INSTALL_CODE_FLAGS_16B,
        value,
        crc: install_code_crc(&value),
    };
    ohf_sys::token::set_mfg(ohf_sys::TOKEN_MFG_INSTALLATION_CODE, &tok);

    // Reset so the stack re-reads the now-valid token from a clean boot.
    ohf_sys::ohf_system_reset();
}
