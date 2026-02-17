/***************************************************************************//**
 * @file
 * @brief Configuration header for QMA6100P Accelerometer Driver
 ******************************************************************************/
#ifndef QMA6100P_CONFIG_H_
#define QMA6100P_CONFIG_H_

// I2C peripheral and pin assignments for QMA6100P
#ifndef QMA6100P_I2C_PERIPHERAL
#define QMA6100P_I2C_PERIPHERAL     I2C0
#endif

#ifndef QMA6100P_I2C_SCL_PORT
#define QMA6100P_I2C_SCL_PORT       gpioPortC
#define QMA6100P_I2C_SCL_PIN        6
#endif

#ifndef QMA6100P_I2C_SDA_PORT
#define QMA6100P_I2C_SDA_PORT       gpioPortC
#define QMA6100P_I2C_SDA_PIN        7
#endif

#endif // QMA6100P_CONFIG_H_
