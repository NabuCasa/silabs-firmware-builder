//! QMA6100P accelerometer driver.
#![no_std]

use ohf_sys::{
    sl_udelay_wait, I2CSPM_Transfer, I2C_TransferReturn_TypeDef, I2C_TransferSeq_TypeDef,
    I2C_TransferSeq_TypeDef__bindgen_ty_1 as I2cBuf, I2C_TypeDef, I2C_FLAG_WRITE,
    I2C_FLAG_WRITE_READ,
};

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
    // From the project's autogen
    static sl_i2cspm_inst: *mut I2C_TypeDef;
}

// Two buffers: written then read for WRITE_READ, or only the first for WRITE
fn transfer(flags: u32, first: &mut [u8], second: &mut [u8]) -> I2C_TransferReturn_TypeDef {
    let mut seq = I2C_TransferSeq_TypeDef {
        addr: QMA6100P_I2C_ADDR,
        flags: flags as u16,
        buf: [
            I2cBuf {
                data: first.as_mut_ptr(),
                len: first.len() as u16,
            },
            I2cBuf {
                data: second.as_mut_ptr(),
                len: second.len() as u16,
            },
        ],
    };
    unsafe { I2CSPM_Transfer(sl_i2cspm_inst, &mut seq) }
}

fn read_reg(reg: u8, data: &mut [u8]) -> I2C_TransferReturn_TypeDef {
    transfer(I2C_FLAG_WRITE_READ, &mut [reg], data)
}

fn write_reg(reg: u8, value: u8) -> I2C_TransferReturn_TypeDef {
    transfer(I2C_FLAG_WRITE, &mut [reg, value], &mut [])
}

fn delay_us(us: u32) {
    unsafe { sl_udelay_wait(us) }
}

fn init() {
    let mut id = [0];
    read_reg(QMA6100P_CHIP_ID, &mut id);

    // software reset
    write_reg(QMA6100P_REG_RESET, QMA6100P_RESET_CMD);
    delay_us(5000);
    write_reg(QMA6100P_REG_RESET, QMA6100P_RESET_CLR);
    delay_us(10000);

    // recommended initialization sequence
    write_reg(QMA6100P_REG_POWER_MANAGEMENT, QMA6100P_PM_MODE_ACTIVE);
    write_reg(
        QMA6100P_REG_POWER_MANAGEMENT,
        QMA6100P_PM_MODE_ACTIVE | QMA6100P_PM_MCLK_51_2K,
    );
    write_reg(QMA6100P_REG_INTERNAL_4A, 0x20);
    write_reg(QMA6100P_REG_INTERNAL_56, 0x01);
    write_reg(QMA6100P_REG_INTERNAL_5F, 0x80);
    delay_us(2000);
    write_reg(QMA6100P_REG_INTERNAL_5F, 0x00);
    delay_us(10000);

    write_reg(QMA6100P_REG_RANGE, QMA6100P_RANGE_8G);
    write_reg(QMA6100P_REG_BW_ODR, QMA6100P_BW_100);
    write_reg(
        QMA6100P_REG_POWER_MANAGEMENT,
        QMA6100P_PM_MODE_ACTIVE | QMA6100P_PM_MCLK_51_2K,
    );
}

fn read_raw() -> [i16; 3] {
    let mut buf = [0u8; 6];
    read_reg(QMA6100P_XOUTL, &mut buf);
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

// For the Z-Wave LED code
#[no_mangle]
pub unsafe extern "C" fn qma6100p_read_raw_xyz(_i2cspm: *mut I2C_TypeDef, data: *mut i16) {
    data.copy_from_nonoverlapping(read_raw().as_ptr(), 3);
}

#[no_mangle]
pub extern "C" fn qma6100p_system_init() {
    init()
}
