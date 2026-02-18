#ifndef QMA6100P_CONFIG_H
#define QMA6100P_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h> QMA6100P I2C Configuration

// <o QMA6100P_I2C_PERIPHERAL> I2C Peripheral
// <I2C0=> I2C0
// <I2C1=> I2C1
// <d> I2C0
#ifndef QMA6100P_I2C_PERIPHERAL
#define QMA6100P_I2C_PERIPHERAL  I2C0
#endif

// <o QMA6100P_I2C_SCL_PORT> SCL Port
// <gpioPortA=> Port A
// <gpioPortB=> Port B
// <gpioPortC=> Port C
// <gpioPortD=> Port D
// <d> gpioPortC
#ifndef QMA6100P_I2C_SCL_PORT
#define QMA6100P_I2C_SCL_PORT  gpioPortC
#endif

// <o QMA6100P_I2C_SCL_PIN> SCL Pin <0-15>
// <d> 6
#ifndef QMA6100P_I2C_SCL_PIN
#define QMA6100P_I2C_SCL_PIN  6
#endif

// <o QMA6100P_I2C_SDA_PORT> SDA Port
// <gpioPortA=> Port A
// <gpioPortB=> Port B
// <gpioPortC=> Port C
// <gpioPortD=> Port D
// <d> gpioPortC
#ifndef QMA6100P_I2C_SDA_PORT
#define QMA6100P_I2C_SDA_PORT  gpioPortC
#endif

// <o QMA6100P_I2C_SDA_PIN> SDA Pin <0-15>
// <d> 7
#ifndef QMA6100P_I2C_SDA_PIN
#define QMA6100P_I2C_SDA_PIN  7
#endif

// </h>

// <<< end of configuration section >>>

#endif // QMA6100P_CONFIG_H
