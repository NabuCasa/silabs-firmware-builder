/*
 * qma6100p.c
 *
 * QMA6100P 3-axis accelerometer driver using the sl_i2c driver
 * Author: qian
 */

#include "qma6100p.h"
#include "sl_udelay.h"
#include "sl_i2c_instances.h"
#include <stdbool.h>

// sl_i2c's blocking transfers mask interrupts for the whole transfer (~1 ms at 100 kHz).
// That stalls the radio interrupts and can leave a scheduled transmit with no
// completion, so reads that happen while the radio runs use the non-blocking transfer.
// The blocking calls are only used during init, before the radio stack runs.

static volatile bool read_done;

static sl_status_t on_transfer_complete(sl_i2c_handle_t *i2c, void *user_data)
{
  (void)i2c;
  (void)user_data;
  read_done = true;
  return SL_STATUS_OK;
}

// Called instead of on_transfer_complete when the transfer ends with a NACK
static sl_status_t on_transfer_event(sl_i2c_handle_t *i2c, sl_i2c_event_t event, void *user_data)
{
  (void)i2c;
  (void)event;
  (void)user_data;
  read_done = true;
  return SL_STATUS_OK;
}

static sl_status_t qma6100p_read_reg(sl_i2c_handle_t *i2c,
                                     uint8_t reg,
                                     uint8_t *data,
                                     uint8_t len)
{
  // A timeout of 0 waits forever
  return sl_i2c_leader_transfer_blocking(i2c, QMA6100P_I2C_ADDR, &reg, 1, data, len, 0);
}

static sl_status_t qma6100p_write_reg(sl_i2c_handle_t *i2c,
                                      uint8_t reg,
                                      uint8_t value)
{
  uint8_t buf[2];
  buf[0] = reg;
  buf[1] = value;

  return sl_i2c_leader_send_blocking(i2c, QMA6100P_I2C_ADDR, buf, sizeof(buf), 0);
}

uint8_t qma6100p_init(sl_i2c_handle_t *i2c)
{
  uint8_t id = 0;

  sl_i2c_set_transfer_complete_callback(i2c, on_transfer_complete);
  sl_i2c_set_event_callback(i2c, on_transfer_event);

  qma6100p_read_reg(i2c, QMA6100P_CHIP_ID, &id, 1);

  /* software reset */
  qma6100p_write_reg(i2c, QMA6100P_REG_RESET, QMA6100P_RESET_CMD);
  sl_udelay_wait(5000);
  qma6100p_write_reg(i2c, QMA6100P_REG_RESET, QMA6100P_RESET_CLR);
  sl_udelay_wait(10000);

  /* recommended initialization sequence */
  qma6100p_write_reg(i2c, QMA6100P_REG_POWER_MANAGEMENT, QMA6100P_PM_MODE_ACTIVE);
  qma6100p_write_reg(i2c, QMA6100P_REG_POWER_MANAGEMENT, QMA6100P_PM_MODE_ACTIVE | QMA6100P_PM_MCLK_51_2K);
  qma6100p_write_reg(i2c, QMA6100P_REG_INTERNAL_4A, 0x20);
  qma6100p_write_reg(i2c, QMA6100P_REG_INTERNAL_56, 0x01);
  qma6100p_write_reg(i2c, QMA6100P_REG_INTERNAL_5F, 0x80);
  sl_udelay_wait(2000);
  qma6100p_write_reg(i2c, QMA6100P_REG_INTERNAL_5F, 0x00);
  sl_udelay_wait(10000);

  qma6100p_write_reg(i2c, QMA6100P_REG_RANGE, QMA6100P_RANGE_8G);
  qma6100p_write_reg(i2c, QMA6100P_REG_BW_ODR, QMA6100P_BW_100);
  qma6100p_write_reg(i2c, QMA6100P_REG_POWER_MANAGEMENT, QMA6100P_PM_MODE_ACTIVE | QMA6100P_PM_MCLK_51_2K);

  return 0;
}

void qma6100p_read_raw_xyz(sl_i2c_handle_t *i2c, int16_t data[3])
{
  uint8_t reg = QMA6100P_XOUTL;
  uint8_t buf[6];
  int16_t raw[3];

  // Waits with interrupts enabled: the I2C interrupt drives the transfer
  read_done = false;
  sl_i2c_leader_transfer_non_blocking(i2c, QMA6100P_I2C_ADDR, &reg, 1, buf, sizeof(buf), NULL);
  while (!read_done) {
  }

  raw[0] = (int16_t)((buf[1] << 8) | buf[0]);
  raw[1] = (int16_t)((buf[3] << 8) | buf[2]);
  raw[2] = (int16_t)((buf[5] << 8) | buf[4]);

  data[0] = raw[0] >> 2;
  data[1] = raw[1] >> 2;
  data[2] = raw[2] >> 2;
}

void qma6100p_read_acc_xyz(sl_i2c_handle_t *i2c, float accdata[3])
{
  int16_t rawdata[3];

  qma6100p_read_raw_xyz(i2c, rawdata);

  accdata[0] = (float)(rawdata[0] * QMA6100P_M_G * -1) / 1024;
  accdata[1] = (float)(rawdata[1] * QMA6100P_M_G * -1) / 1024;
  accdata[2] = (float)(rawdata[2] * QMA6100P_M_G * -1) / 1024;
}

void qma6100p_system_init(void)
{
  qma6100p_init(sl_i2c_inst_handle);
}
