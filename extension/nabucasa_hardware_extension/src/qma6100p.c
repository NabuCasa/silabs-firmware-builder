/*
 * qma6100p.c
 *
 * QMA6100P 3-axis accelerometer driver using polled emlib I2C
 * Author: qian
 */

#include "qma6100p.h"
#include "qma6100p_config.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "sl_udelay.h"
#include <stddef.h>

static I2C_TransferReturn_TypeDef i2c_transfer_poll(I2C_TypeDef *i2c,
                                                     I2C_TransferSeq_TypeDef *seq)
{
  I2C_TransferReturn_TypeDef status = I2C_TransferInit(i2c, seq);
  while (status == i2cTransferInProgress) {
    status = I2C_Transfer(i2c);
  }
  return status;
}

static I2C_TransferReturn_TypeDef qma6100p_read_reg(I2C_TypeDef *i2c,
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

  return i2c_transfer_poll(i2c, &seq);
}

static I2C_TransferReturn_TypeDef qma6100p_write_reg(I2C_TypeDef *i2c,
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

  return i2c_transfer_poll(i2c, &seq);
}

uint8_t qma6100p_init(I2C_TypeDef *i2c)
{
  uint8_t id = 0;

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

void qma6100p_read_raw_xyz(I2C_TypeDef *i2c, int16_t data[3])
{
  uint8_t buf[6];
  int16_t raw[3];

  qma6100p_read_reg(i2c, QMA6100P_XOUTL, buf, 6);

  raw[0] = (int16_t)((buf[1] << 8) | buf[0]);
  raw[1] = (int16_t)((buf[3] << 8) | buf[2]);
  raw[2] = (int16_t)((buf[5] << 8) | buf[4]);

  data[0] = raw[0] >> 2;
  data[1] = raw[1] >> 2;
  data[2] = raw[2] >> 2;
}

void qma6100p_read_acc_xyz(I2C_TypeDef *i2c, float accdata[3])
{
  int16_t rawdata[3];

  qma6100p_read_raw_xyz(i2c, rawdata);

  accdata[0] = (float)(rawdata[0] * QMA6100P_M_G * -1) / 1024;
  accdata[1] = (float)(rawdata[1] * QMA6100P_M_G * -1) / 1024;
  accdata[2] = (float)(rawdata[2] * QMA6100P_M_G * -1) / 1024;
}

static void qma6100p_i2c_init(void)
{
  // Enable clocks
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_I2C0 + I2C_NUM(QMA6100P_I2C_PERIPHERAL), true);

  // Configure GPIO pins for I2C (wired-AND with pull-up)
  GPIO_PinModeSet(QMA6100P_I2C_SCL_PORT, QMA6100P_I2C_SCL_PIN, gpioModeWiredAnd, 1);
  GPIO_PinModeSet(QMA6100P_I2C_SDA_PORT, QMA6100P_I2C_SDA_PIN, gpioModeWiredAnd, 1);

  // Route I2C signals to pins
  int i2c_num = I2C_NUM(QMA6100P_I2C_PERIPHERAL);
  GPIO->I2CROUTE[i2c_num].ROUTEEN = GPIO_I2C_ROUTEEN_SDAPEN | GPIO_I2C_ROUTEEN_SCLPEN;
  GPIO->I2CROUTE[i2c_num].SCLROUTE = (QMA6100P_I2C_SCL_PIN << _GPIO_I2C_SCLROUTE_PIN_SHIFT)
                                    | (QMA6100P_I2C_SCL_PORT << _GPIO_I2C_SCLROUTE_PORT_SHIFT);
  GPIO->I2CROUTE[i2c_num].SDAROUTE = (QMA6100P_I2C_SDA_PIN << _GPIO_I2C_SDAROUTE_PIN_SHIFT)
                                    | (QMA6100P_I2C_SDA_PORT << _GPIO_I2C_SDAROUTE_PORT_SHIFT);

  // Initialize I2C peripheral at standard frequency
  I2C_Init_TypeDef i2c_init = I2C_INIT_DEFAULT;
  I2C_Init(QMA6100P_I2C_PERIPHERAL, &i2c_init);
}

void qma6100p_system_init(void)
{
  qma6100p_i2c_init();
  qma6100p_init(QMA6100P_I2C_PERIPHERAL);
}
