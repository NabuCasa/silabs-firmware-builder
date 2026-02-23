/*
 * qma6100p.h
 *
 * QMA6100P 3-axis accelerometer driver
 * Author: qian
 */

#ifndef QMA6100P_H_
#define QMA6100P_H_

#include <stdint.h>
#include "sl_i2cspm.h"

#define QMA6100P_M_G                   9.80665f
#define QMA6100P_I2C_ADDR              0x24

#define QMA6100P_CHIP_ID               0x00
#define QMA6100P_DEVICE_ID             0x90

#define QMA6100P_XOUTL                 0x01
#define QMA6100P_XOUTH                 0x02
#define QMA6100P_YOUTL                 0x03
#define QMA6100P_YOUTH                 0x04
#define QMA6100P_ZOUTL                 0x05
#define QMA6100P_ZOUTH                 0x06

#define QMA6100P_REG_CHIP_ID           0x00
#define QMA6100P_REG_RANGE             0x0f
#define QMA6100P_REG_BW_ODR            0x10
#define QMA6100P_REG_POWER_MANAGEMENT  0x11
#define QMA6100P_REG_RESET             0x36

// Undocumented
#define QMA6100P_REG_INTERNAL_4A       0x4A 
#define QMA6100P_REG_INTERNAL_56       0x56 
#define QMA6100P_REG_INTERNAL_5F       0x5F

#define QMA6100P_RESET_CMD             0xB6  // Magic value to trigger soft reset 
#define QMA6100P_RESET_CLR             0x00  // Value to write back after reset

#define QMA6100P_PM_MODE_ACTIVE        0x80
#define QMA6100P_PM_MCLK_51_2K         0x04

typedef enum {
  QMA6100P_RANGE_2G = 0x01,
  QMA6100P_RANGE_4G = 0x02,
  QMA6100P_RANGE_8G = 0x04,
  QMA6100P_RANGE_16G = 0x08,
  QMA6100P_RANGE_32G = 0x0f
} qma6100p_range_t;

typedef enum {
  QMA6100P_BW_100 = 0,
  QMA6100P_BW_200 = 1,
  QMA6100P_BW_400 = 2,
  QMA6100P_BW_800 = 3,
  QMA6100P_BW_1600 = 4,
  QMA6100P_BW_50 = 5,
  QMA6100P_BW_25 = 6,
  QMA6100P_BW_12_5 = 7
} qma6100p_bw_t;

/**
 * @brief Initialize QMA6100P accelerometer
 * @param i2cspm Pointer to I2CSPM instance to use
 */
void qma6100p_system_init(void);

/**
 * @brief Read raw 3-axis acceleration data
 * @param i2cspm Pointer to I2CSPM instance to use
 * @param data Array to store 3-axis raw data
 */
void qma6100p_read_raw_xyz(sl_i2cspm_t *i2cspm, int16_t data[3]);

/**
 * @brief Read calibrated 3-axis acceleration data in m/s^2
 * @param i2cspm Pointer to I2CSPM instance to use
 * @param accdata Array to store 3-axis acceleration in m/s^2
 */
void qma6100p_read_acc_xyz(sl_i2cspm_t *i2cspm, float accdata[3]);

#endif /* QMA6100P_H_ */
