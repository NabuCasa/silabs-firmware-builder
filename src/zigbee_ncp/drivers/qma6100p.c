/*
 * 3axis.c
 *
 *  Created on: 2024年11月1日
 *      Author: qian
 */
#include "qma6100p.h"

#define I2C_TXBUFFER_SIZE                 10
#define I2C_RXBUFFER_SIZE                 10

// Buffers
uint8_t i2c_txBuffer[I2C_TXBUFFER_SIZE];
uint8_t i2c_rxBuffer[I2C_RXBUFFER_SIZE];

/***************************************************************************//**
 * @brief Enable clocks
 ******************************************************************************/
static void initCMU(void)
{
  // Enable clocks to the I2C and GPIO
  CMU_ClockEnable(cmuClock_I2C0, true);
  CMU_ClockEnable(cmuClock_GPIO, true);
  /*
   * Note: For EFR32xG21 radio devices, library function calls to
   * CMU_ClockEnable() have no effect as oscillators are automatically turned
   * on/off based on demand from the peripherals; CMU_ClockEnable() is a dummy
   * function for EFR32xG21 for library consistency/compatibility.
   */
}

/***************************************************************************//**
 * @brief GPIO initialization
 ******************************************************************************/
static void initGPIO(void)
{
  // Using PA5 (SDA) and PA6 (SCL)
  GPIO_PinModeSet(I2C0SDA_PORT, I2C0SDA_PIN, gpioModeWiredAndPullUpFilter, 1);
  GPIO_PinModeSet(I2C0SCL_PORT, I2C0SCL_PIN, gpioModeWiredAndPullUpFilter, 1);
}

/***************************************************************************//**
 * @brief Setup I2C
 ******************************************************************************/
static void initI2C(void)
{
  // Use default settings
  I2C_Init_TypeDef i2cInit = I2C_INIT_DEFAULT;

  // Route I2C pins to GPIO
  GPIO->I2CROUTE[0].SDAROUTE = (GPIO->I2CROUTE[0].SDAROUTE & ~_GPIO_I2C_SDAROUTE_MASK)
                        | (I2C0SDA_PORT << _GPIO_I2C_SDAROUTE_PORT_SHIFT
                        | (I2C0SDA_PIN << _GPIO_I2C_SDAROUTE_PIN_SHIFT));
  GPIO->I2CROUTE[0].SCLROUTE = (GPIO->I2CROUTE[0].SCLROUTE & ~_GPIO_I2C_SCLROUTE_MASK)
                        | (I2C0SCL_PORT << _GPIO_I2C_SCLROUTE_PORT_SHIFT
                        | (I2C0SCL_PIN << _GPIO_I2C_SCLROUTE_PIN_SHIFT));
  GPIO->I2CROUTE[0].ROUTEEN = GPIO_I2C_ROUTEEN_SDAPEN | GPIO_I2C_ROUTEEN_SCLPEN;

  // Initialize the I2C
  I2C_Init(I2C0, &i2cInit);

  // Enable automatic STOP on NACK
  I2C0->CTRL = I2C_CTRL_AUTOSN;

  I2C_Enable(I2C0, true);
}

void initqma6100p(void)
{
  // Initialize the I2C
  initCMU();
  initGPIO();
  initI2C();
  qma6100p_init();
}
/***************************************************************************//**
 * @brief I2C read numBytes from follower device starting at target address
 ******************************************************************************/
void I2C_LeaderRead(uint8_t targetAddress, uint8_t *rxBuff, uint8_t numBytes)
{
  // Transfer structure
  I2C_TransferSeq_TypeDef i2cTransfer;
  I2C_TransferReturn_TypeDef result;

  // Initialize I2C transfer
  i2cTransfer.addr          = IMU_DEV_ADDR;
  i2cTransfer.flags         = I2C_FLAG_WRITE_READ; // must write target address before reading
  i2cTransfer.buf[0].data   = &targetAddress;
  i2cTransfer.buf[0].len    = 1;
  i2cTransfer.buf[1].data   = rxBuff;
  i2cTransfer.buf[1].len    = numBytes;

  result = I2C_TransferInit(I2C0, &i2cTransfer);

  // Send data
  while (result == i2cTransferInProgress) {
    result = I2C_Transfer(I2C0);
  }
}

/***************************************************************************//**
 * @brief I2C write numBytes to follower device starting at target address
 ******************************************************************************/
void I2C_LeaderWrite(uint8_t targetAddress, uint8_t txBuff)
{
  // Transfer structure
  I2C_TransferSeq_TypeDef i2cTransfer;
  I2C_TransferReturn_TypeDef result;
  uint8_t txBuffer[I2C_TXBUFFER_SIZE + 1];

  txBuffer[0] = targetAddress;
  txBuffer[1] = txBuff;

  // Initialize I2C transfer
  i2cTransfer.addr          = IMU_DEV_ADDR;
  i2cTransfer.flags         = I2C_FLAG_WRITE;
  i2cTransfer.buf[0].data   = txBuffer;
  i2cTransfer.buf[0].len    = 2;
  i2cTransfer.buf[1].data   = NULL;
  i2cTransfer.buf[1].len    = 0;

  result = I2C_TransferInit(I2C0, &i2cTransfer);

  // Send data
  while (result == i2cTransferInProgress) {
    result = I2C_Transfer(I2C0);
  }
}

/**
* @brief       configure qma6100p
* @param       None
* @retval      0:succeed; !0:failed
*/
uint8_t qma6100p_init(void)
{
    uint8_t id = 0;


    I2C_LeaderRead(QMA6100P_CHIP_ID, &id, 1);    /* read qma6100p ID */
    /* software reset */
    I2C_LeaderWrite(QMA6100P_REG_RESET, 0xb6);
    sl_udelay_wait(5 * 1000);
    I2C_LeaderWrite(QMA6100P_REG_RESET, 0x00);
    sl_udelay_wait(10 * 1000);

    /* recommended initialization sequence */
    I2C_LeaderWrite(0x11, 0x80);
    I2C_LeaderWrite(0x11, 0x84);
    I2C_LeaderWrite(0x4a, 0x20);
    I2C_LeaderWrite(0x56, 0x01);
    I2C_LeaderWrite(0x5f, 0x80);
    sl_udelay_wait(2 * 1000);
    I2C_LeaderWrite(0x5f, 0x00);
    sl_udelay_wait(10 * 1000);

    /* set qma6100p configuration parameters (range, output frequency, operating mode) */
    I2C_LeaderWrite(QMA6100P_REG_RANGE, QMA6100P_RANGE_8G);
    I2C_LeaderWrite(QMA6100P_REG_BW_ODR, (uint8_t)(QMA6100P_BW_100 | QMA6100P_LPF_OFF));
    I2C_LeaderWrite(QMA6100P_REG_POWER_MANAGE, (uint8_t)QMA6100P_MCLK_51_2K | 0x80);

    return 0;
}

/**
 * @brief       read data from QMA6100P
 * @param       data  : 3-axis data
 * @retval      None
 */
void qma6100p_read_raw_xyz(int16_t data[3])
{
    uint8_t databuf[6] = {0};
    int16_t raw_data[3];

    I2C_LeaderRead(QMA6100P_XOUTL, databuf, 6);

    raw_data[0] = (int16_t)(((databuf[1] << 8)) | (databuf[0]));
    raw_data[1] = (int16_t)(((databuf[3] << 8)) | (databuf[2]));
    raw_data[2] = (int16_t)(((databuf[5] << 8)) | (databuf[4]));

    data[0] = raw_data[0] >> 2;
    data[1] = raw_data[1] >> 2;
    data[2] = raw_data[2] >> 2;
}

/**
 * @brief       calculate the x,y, and z axis data of the accelerometer
 * @param       accdata  : 3-axis data
 * @retval      None
 */
void qma6100p_read_acc_xyz(float accdata[3])
{
    int16_t rawdata[3];

    qma6100p_read_raw_xyz(rawdata);

    accdata[0] = (float)(rawdata[0] * M_G * -1) / 1024;
    accdata[1] = (float)(rawdata[1] * M_G * -1) / 1024;
    accdata[2] = (float)(rawdata[2] * M_G * -1) / 1024;
}

