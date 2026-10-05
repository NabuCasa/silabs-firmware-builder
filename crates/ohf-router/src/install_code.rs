//! Rust port of `router_install_code.c` — one-time self-provisioning of the install-code MFG
//! token. The ZBT-2 ships without one, so an R23 router has no pre-shared secret for DLK; if
//! the token is blank, generate one from the TRNG, write it, and reset.
//!
//! The MFG-token read/write are macros, so they go through a thin C shim
//! (router_install_code_shim.c); everything else is Rust.
use crate::bindings::{halCommonCrc16, psa_crypto_init, psa_generate_random, psa_status_t, PSA_SUCCESS};

const INSTALL_CODE_VALUE_SIZE: usize = 16;
// flags: bit0 = 0 (valid), bits1-2 = size code 3 (16 bytes) => 0x0006.
const INSTALL_CODE_FLAGS_16B: u16 = 0x0006;

extern "C" {
    fn ohf_mfg_install_code_flags() -> u16;
    fn ohf_mfg_install_code_set(flags: u16, value: *const u8, crc: u16);
}

// Bit-reverse a byte. The Zigbee install-code CRC and halCommonCrc16 differ in bit/byte order,
// so inputs are reversed in and the result reversed + ones'-complemented out — matching
// sli_zigbee_af_install_code_to_key() so the coordinator derives the same key.
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
    if unsafe { ohf_mfg_install_code_flags() } != 0xFFFF {
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
        ohf_mfg_install_code_set(INSTALL_CODE_FLAGS_16B, value.as_ptr(), install_code_crc(&value));
    }

    // Reset so the stack re-reads the now-valid token from a clean boot.
    ohf_sys::ohf_system_reset();
}
