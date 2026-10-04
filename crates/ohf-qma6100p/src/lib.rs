//! QMA6100P 3-axis accelerometer driver — Rust port of `qma6100p.c`.
//!
//! Register-level I2C driver over I2CSPM. `qma6100p_system_init` (the stack_init hook) and
//! `qma6100p_read_raw_xyz` stay C exports (the latter is called by the Z-Wave LED code); Rust
//! callers use the safe `read_acceleration()`.
#![no_std]

use ohf_sys::{
    sl_udelay_wait, I2CSPM_Transfer, I2C_TransferReturn_TypeDef, I2C_TransferSeq_TypeDef,
    I2C_TransferSeq_TypeDef__bindgen_ty_1 as I2cBuf, I2C_TypeDef, I2C_FLAG_WRITE, I2C_FLAG_WRITE_READ,
};

// Device constants (from the QMA6100P datasheet; the driver's own, not the SDK's).
const QMA6100P_I2C_ADDR: u16 = 0x24;
const QMA6100P_M_G: f32 = 9.80665;

const QMA6100P_CHIP_ID: u8 = 0x00;
const QMA6100P_XOUTL: u8 = 0x01;
const QMA6100P_REG_RANGE: u8 = 0x0f;
const QMA6100P_REG_BW_ODR: u8 = 0x10;
const QMA6100P_REG_POWER_MANAGEMENT: u8 = 0x11;
const QMA6100P_REG_RESET: u8 = 0x36;
const QMA6100P_REG_INTERNAL_4A: u8 = 0x4a;
const QMA6100P_REG_INTERNAL_56: u8 = 0x56;
const QMA6100P_REG_INTERNAL_5F: u8 = 0x5f;
const QMA6100P_RESET_CMD: u8 = 0xb6;
const QMA6100P_RESET_CLR: u8 = 0x00;
const QMA6100P_PM_MODE_ACTIVE: u8 = 0x80;
const QMA6100P_PM_MCLK_51_2K: u8 = 0x04;
const QMA6100P_RANGE_8G: u8 = 0x04;
const QMA6100P_BW_100: u8 = 0;

extern "C" {
    // Per-instance handle from the project autogen (sl_i2cspm_instances.h), referenced
    // by name like any other instance global.
    static sl_i2cspm_inst: *mut I2C_TypeDef;
}

unsafe fn read_reg(
    i2cspm: *mut I2C_TypeDef,
    reg: u8,
    data: *mut u8,
    len: u16,
) -> I2C_TransferReturn_TypeDef {
    let mut reg = reg;
    let mut seq = I2C_TransferSeq_TypeDef {
        addr: QMA6100P_I2C_ADDR,
        flags: I2C_FLAG_WRITE_READ as u16,
        buf: [
            I2cBuf { data: &mut reg, len: 1 },
            I2cBuf { data, len },
        ],
    };
    I2CSPM_Transfer(i2cspm, &mut seq)
}

unsafe fn write_reg(i2cspm: *mut I2C_TypeDef, reg: u8, value: u8) -> I2C_TransferReturn_TypeDef {
    let mut buf = [reg, value];
    let mut seq = I2C_TransferSeq_TypeDef {
        addr: QMA6100P_I2C_ADDR,
        flags: I2C_FLAG_WRITE as u16,
        buf: [
            I2cBuf { data: buf.as_mut_ptr(), len: 2 },
            I2cBuf { data: core::ptr::null_mut(), len: 0 },
        ],
    };
    I2CSPM_Transfer(i2cspm, &mut seq)
}

unsafe fn init(i2cspm: *mut I2C_TypeDef) {
    let mut id: u8 = 0;
    read_reg(i2cspm, QMA6100P_CHIP_ID, &mut id, 1);

    // software reset
    write_reg(i2cspm, QMA6100P_REG_RESET, QMA6100P_RESET_CMD);
    sl_udelay_wait(5000);
    write_reg(i2cspm, QMA6100P_REG_RESET, QMA6100P_RESET_CLR);
    sl_udelay_wait(10000);

    // recommended initialization sequence
    write_reg(i2cspm, QMA6100P_REG_POWER_MANAGEMENT, QMA6100P_PM_MODE_ACTIVE);
    write_reg(i2cspm, QMA6100P_REG_POWER_MANAGEMENT, QMA6100P_PM_MODE_ACTIVE | QMA6100P_PM_MCLK_51_2K);
    write_reg(i2cspm, QMA6100P_REG_INTERNAL_4A, 0x20);
    write_reg(i2cspm, QMA6100P_REG_INTERNAL_56, 0x01);
    write_reg(i2cspm, QMA6100P_REG_INTERNAL_5F, 0x80);
    sl_udelay_wait(2000);
    write_reg(i2cspm, QMA6100P_REG_INTERNAL_5F, 0x00);
    sl_udelay_wait(10000);

    write_reg(i2cspm, QMA6100P_REG_RANGE, QMA6100P_RANGE_8G);
    write_reg(i2cspm, QMA6100P_REG_BW_ODR, QMA6100P_BW_100);
    write_reg(i2cspm, QMA6100P_REG_POWER_MANAGEMENT, QMA6100P_PM_MODE_ACTIVE | QMA6100P_PM_MCLK_51_2K);
}

fn read_raw() -> [i16; 3] {
    let mut buf = [0u8; 6];
    unsafe { read_reg(sl_i2cspm_inst, QMA6100P_XOUTL, buf.as_mut_ptr(), 6) };
    // 14-bit left-justified samples; shift the sign-extended value down by 2.
    [
        i16::from_be_bytes([buf[1], buf[0]]) >> 2,
        i16::from_be_bytes([buf[3], buf[2]]) >> 2,
        i16::from_be_bytes([buf[5], buf[4]]) >> 2,
    ]
}

pub fn read_acceleration() -> [f32; 3] {
    read_raw().map(|v| (v as f32 * QMA6100P_M_G * -1.0) / 1024.0)
}

// C ABI for the Z-Wave LED code (the only cross-language caller); Rust uses read_raw directly.
#[no_mangle]
pub unsafe extern "C" fn qma6100p_read_raw_xyz(_i2cspm: *mut I2C_TypeDef, data: *mut i16) {
    data.copy_from_nonoverlapping(read_raw().as_ptr(), 3);
}

#[no_mangle]
pub extern "C" fn qma6100p_system_init() {
    unsafe { init(sl_i2cspm_inst) }
}
