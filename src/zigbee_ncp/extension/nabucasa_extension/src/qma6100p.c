/*
 * qma6100p.c
 *
 * QMA6100P 3-axis accelerometer driver using I2CSPM
 * Author: qian
 */

#include "qma6100p.h"
#include "sl_udelay.h"
#include "sl_i2cspm_instances.h"
#include <stddef.h>

static I2C_TransferReturn_TypeDef qma6100p_read_reg(sl_i2cspm_t *i2cspm,
                                                     uint8_t reg,
                                                     uint8_t *data,
                                                     uint8_t len)
{
  I2C_TransferSeq_TypeDef seq;
  seq.addr = QMA6100P_I2C_ADDR;
  seq.flags = I2C_FLAG_WRITE_READ;
  seq.buf[0].data = &reg;
  seq.buf[0].len = 1;
  seq.buf[1].data = data;
  seq.buf[1].len = len;

  return I2CSPM_Transfer(i2cspm, &seq);
}

static I2C_TransferReturn_TypeDef qma6100p_write_reg(sl_i2cspm_t *i2cspm,
                                                      uint8_t reg,
                                                      uint8_t value)
{
  uint8_t buf[2];
  buf[0] = reg;
  buf[1] = value;

  I2C_TransferSeq_TypeDef seq;
  seq.addr = QMA6100P_I2C_ADDR;
  seq.flags = I2C_FLAG_WRITE;
  seq.buf[0].data = buf;
  seq.buf[0].len = sizeof(buf);
  seq.buf[1].data = NULL;
  seq.buf[1].len = 0;

  return I2CSPM_Transfer(i2cspm, &seq);
}

uint8_t qma6100p_init(sl_i2cspm_t *i2cspm)
{
  uint8_t id = 0;

  qma6100p_read_reg(i2cspm, QMA6100P_CHIP_ID, &id, 1);

  /* software reset */
  qma6100p_write_reg(i2cspm, QMA6100P_REG_RESET, QMA6100P_RESET_CMD);
  sl_udelay_wait(5000);  
  qma6100p_write_reg(i2cspm, QMA6100P_REG_RESET, QMA6100P_RESET_CLR);
  sl_udelay_wait(10000);

  /* recommended initialization sequence */
  qma6100p_write_reg(i2cspm, QMA6100P_REG_POWER_MANAGEMENT, QMA6100P_PM_MODE_ACTIVE);
  qma6100p_write_reg(i2cspm, QMA6100P_REG_POWER_MANAGEMENT, QMA6100P_PM_MODE_ACTIVE | QMA6100P_PM_MCLK_51_2K);
  qma6100p_write_reg(i2cspm, QMA6100P_REG_INTERNAL_4A, 0x20);
  qma6100p_write_reg(i2cspm, QMA6100P_REG_INTERNAL_56, 0x01);
  qma6100p_write_reg(i2cspm, QMA6100P_REG_INTERNAL_5F, 0x80);
  sl_udelay_wait(2000);
  qma6100p_write_reg(i2cspm, QMA6100P_REG_INTERNAL_5F, 0x00);
  sl_udelay_wait(10000);

  qma6100p_write_reg(i2cspm, QMA6100P_REG_RANGE, QMA6100P_RANGE_8G);
  qma6100p_write_reg(i2cspm, QMA6100P_REG_BW_ODR, QMA6100P_BW_100);
  qma6100p_write_reg(i2cspm, QMA6100P_REG_POWER_MANAGEMENT, QMA6100P_PM_MODE_ACTIVE | QMA6100P_PM_MCLK_51_2K);

  return 0;
}

void qma6100p_read_raw_xyz(sl_i2cspm_t *i2cspm, int16_t data[3])
{
  uint8_t buf[6];
  int16_t raw[3];

  qma6100p_read_reg(i2cspm, QMA6100P_XOUTL, buf, 6);

  raw[0] = (int16_t)((buf[1] << 8) | buf[0]);
  raw[1] = (int16_t)((buf[3] << 8) | buf[2]);
  raw[2] = (int16_t)((buf[5] << 8) | buf[4]);

  data[0] = raw[0] >> 2;
  data[1] = raw[1] >> 2;
  data[2] = raw[2] >> 2;
}

void qma6100p_read_acc_xyz(sl_i2cspm_t *i2cspm, float accdata[3])
{
  int16_t rawdata[3];

  qma6100p_read_raw_xyz(i2cspm, rawdata);

  accdata[0] = (float)(rawdata[0] * QMA6100P_M_G * -1) / 1024;
  accdata[1] = (float)(rawdata[1] * QMA6100P_M_G * -1) / 1024;
  accdata[2] = (float)(rawdata[2] * QMA6100P_M_G * -1) / 1024;
}

void qma6100p_system_init(uint8_t init_level)
{
  (void)init_level;
  qma6100p_init(sl_i2cspm_inst);
}
