/*! ----------------------------------------------------------------------------
 * @file    deca_spi.c
 * @brief   SPI access functions
 *
 *          Adapted from Qorvo's STM_Nucleo_F429 reference platform for the
 *          Nucleo-F446RE + DWS3000 shield: single SPI1 instance, CS on PB6
 *          (SPI1_CSn), software NSS, active-low chip select.
 */

#include <deca_device_api.h>
#include <deca_spi.h>
#include <port.h>

/* Declared in Core/Src/main.c, initialized by CubeMX-generated MX_SPI1_Init(). */
extern SPI_HandleTypeDef hspi1;

/****************************************************************************
 *
 *
 *                              DWxxx SPI section
 *
 *******************************************************************************/
/*! ------------------------------------------------------------------------------------------------------------------
 * Function: openspi()
 *
 * Low level abstract function to open and initialise access to the SPI device.
 * returns 0 for success, or -1 for error
 */
int openspi(void)
{
    return 0;
} // end openspi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: closespi()
 *
 * Low level abstract function to close the the SPI device.
 * returns 0 for success, or -1 for error
 */
int closespi(void)
{
    return 0;
} // end closespi()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: writetospiwithcrc()
 *
 * Low level abstract function to write to the SPI when SPI CRC mode is used
 * Takes two separate byte buffers for write header and write data, and a CRC8 byte which is written last
 * returns 0 for success, or -1 for error
 */
int32_t writetospiwithcrc(uint16_t headerLength, const uint8_t *headerBuffer, uint16_t bodyLength, const uint8_t *bodyBuffer, uint8_t crc8)
{
#ifdef DWT_ENABLE_CRC
    decaIrqStatus_t stat;
    stat = decamutexon();
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);

    HAL_GPIO_WritePin(SPI1_CSn_GPIO_Port, SPI1_CSn_Pin, GPIO_PIN_RESET); /**< Put chip select line low */

    HAL_SPI_Transmit(&hspi1, (uint8_t *)headerBuffer, headerLength, 10); /* Send header in polling mode */
    HAL_SPI_Transmit(&hspi1, (uint8_t *)bodyBuffer, bodyLength, 10);     /* Send data in polling mode */
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&crc8, 1, 10);                  /* Send CRC in polling mode */

    HAL_GPIO_WritePin(SPI1_CSn_GPIO_Port, SPI1_CSn_Pin, GPIO_PIN_SET); /**< Put chip select line high */

    decamutexoff(stat);
#endif //DWT_ENABLE_CRC
    return 0;
} // end writetospiwithcrc()

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: writetospi()
 *
 * Low level abstract function to write to the SPI
 * Takes two separate byte buffers for write header and write data
 * returns 0 for success, or -1 for error
 */
int32_t writetospi(uint16_t headerLength, const uint8_t *headerBuffer, uint16_t bodyLength, const uint8_t *bodyBuffer)
{
    decaIrqStatus_t stat;
    stat = decamutexon();

    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);

    HAL_GPIO_WritePin(SPI1_CSn_GPIO_Port, SPI1_CSn_Pin, GPIO_PIN_RESET); /**< Put chip select line low */

    HAL_SPI_Transmit(&hspi1, (uint8_t *)headerBuffer, headerLength, HAL_MAX_DELAY); /* Send header in polling mode */

    if (bodyLength != 0)
    {
        HAL_SPI_Transmit(&hspi1, (uint8_t *)bodyBuffer, bodyLength, HAL_MAX_DELAY); /* Send data in polling mode */
    }

    HAL_GPIO_WritePin(SPI1_CSn_GPIO_Port, SPI1_CSn_Pin, GPIO_PIN_SET); /**< Put chip select line high */

    decamutexoff(stat);

    return 0;
} // end writetospi()

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn spi_cs_low_delay()
 *
 * @brief This function sets the CS to '0' for ms delay and than raises it up
 *
 * input parameters:
 * @param delay_ms - The delay for CS to be in '0' state
 *
 * no return value
 */
uint16_t spi_cs_low_delay(uint16_t delay_ms)
{
    /* Blocking: Check whether previous transfer has been finished */
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);
    /* Process Locked */
    __HAL_LOCK(&hspi1);
    HAL_GPIO_WritePin(SPI1_CSn_GPIO_Port, SPI1_CSn_Pin, GPIO_PIN_RESET); /**< Put chip select line low */
    Sleep(delay_ms);
    HAL_GPIO_WritePin(SPI1_CSn_GPIO_Port, SPI1_CSn_Pin, GPIO_PIN_SET); /**< Put chip select line high */
    /* Process Unlocked */
    __HAL_UNLOCK(&hspi1);

    return 0;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * Function: readfromspi()
 *
 * Low level abstract function to read from the SPI
 * Takes two separate byte buffers for write header and read data
 * returns the offset into read buffer where first byte of read data may be found,
 * or returns -1 if there was an error
 */
int32_t readfromspi(uint16_t headerLength, uint8_t *headerBuffer, uint16_t readlength, uint8_t *readBuffer)
{
    decaIrqStatus_t stat;
    stat = decamutexon();

    /* Blocking: Check whether previous transfer has been finished */
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);

    HAL_GPIO_WritePin(SPI1_CSn_GPIO_Port, SPI1_CSn_Pin, GPIO_PIN_RESET); /**< Put chip select line low */

    /* Send header */
    HAL_SPI_Transmit(&hspi1, (uint8_t *)headerBuffer, headerLength, HAL_MAX_DELAY); //No timeout

    while (__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_TXE) == RESET) //Verify that the transmit was ended
    {
    }

    /* for the data buffer use LL-style register access directly as the HAL SPI read function
     * has issues reading single bytes */
    while (readlength-- > 0)
    {
        /* Wait until TXE flag is set to send data */
        while (__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_TXE) == RESET)
        {
        }

        hspi1.Instance->DR = 0;
        /* set output to 0 (MOSI), this is necessary for
        e.g. when waking up DW3000 from DEEPSLEEP via dwt_spicswakeup() function.
        */

        /* Wait until RXNE flag is set to read data */
        while (__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_RXNE) == RESET)
        {
        }

        (*readBuffer++) = hspi1.Instance->DR; //copy data read from (MISO)
    }

    HAL_GPIO_WritePin(SPI1_CSn_GPIO_Port, SPI1_CSn_Pin, GPIO_PIN_SET); /**< Put chip select line high */

    decamutexoff(stat);

    return 0;
} // end readfromspi()

/****************************************************************************
 *
 *                              END OF DW3xxx SPI section
 *
 *******************************************************************************/
