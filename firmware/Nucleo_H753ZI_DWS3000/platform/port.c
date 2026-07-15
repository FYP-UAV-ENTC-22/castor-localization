/*! ----------------------------------------------------------------------------
 * @file    port.c
 * @brief   HW specific definitions and functions for portability
 *
 *          Adapted from Qorvo's STM_Nucleo_F429 reference platform for the
 *          Nucleo-H753ZI + DWS3000 shield (single SPI, no USB/LCD/second device).
 */

#include <port.h>

/* Declared in Core/Src/main.c, initialized by CubeMX-generated MX_SPI1_Init(). */
extern SPI_HandleTypeDef hspi1;

/****************************************************************************
 *
 *                  Port private variables and function prototypes
 *
 *******************************************************************************/

/* DW IC IRQ handler installed by the driver via port_set_dwic_isr(). */
static port_dwic_isr_t port_dwic_isr = NULL;

/****************************************************************************
 *
 *                              Time section
 *
 *******************************************************************************/

/* @fn    portGetTickCnt
 * @brief wrapper to read the SysTick-driven millisecond counter.
 * */
uint32_t portGetTickCnt(void)
{
    return HAL_GetTick();
}

/* @fn    usleep
 * @brief approximate busy-wait delay in microseconds
 * */
#pragma GCC optimize("O0")
int usleep(uint32_t usec)
{
    unsigned int i;

    usec *= 12;
    for (i = 0; i < usec; i++)
    {
        __NOP();
    }
    return 0;
}

/* @fn    Sleep
 * @brief Sleep delay in ms using SysTick timer
 * */
void Sleep(uint32_t x)
{
    HAL_Delay(x);
}

/****************************************************************************
 *
 *                              Configuration section
 *
 *******************************************************************************/

/* @fn    peripherals_init
 * */
int peripherals_init(void)
{
    /* All peripherals are initialized by the CubeMX-generated code in main.c */
    return 0;
}

/* @fn    spi_peripheral_init
 * */
void spi_peripheral_init(void)
{
    /* SPI1 is initialized by the CubeMX-generated code in main.c */
}

/**
 * @brief  Checks whether the specified IRQn line is enabled or not.
 * @param  IRQn: specifies the IRQn line to check.
 * @return "0" when IRQn is "not enabled" and !0 otherwise
 */
ITStatus EXTI_GetITEnStatus(IRQn_Type IRQn)
{
    return ((NVIC->ISER[(((uint32_t)(int32_t)IRQn) >> 5UL)] & (uint32_t)(1UL << (((uint32_t)(int32_t)IRQn) & 0x1FUL))) == (uint32_t)RESET) ? (RESET) : (SET);
}

/****************************************************************************
 *
 *                          DW IC port section
 *
 *******************************************************************************/

/* @fn      reset_DWIC
 * @brief   RSTn pin on DW IC has 2 functions:
 *          In general it is an output, but it can also be used to reset the digital
 *          part of DW IC by driving this pin low.
 *          Note: the RSTn pin should not be driven high externally, so it is
 *          configured as open-drain - driving it low asserts reset, releasing it
 *          (tri-state) lets the DW IC pull it back high itself.
 * */
void reset_DWIC(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    GPIO_InitStruct.Pin = RSTn_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RSTn_GPIO_Port, &GPIO_InitStruct);

    /* drive the RSTn pin low */
    HAL_GPIO_WritePin(RSTn_GPIO_Port, RSTn_Pin, GPIO_PIN_RESET);

    usleep(1);

    /* release RSTn back to its idle (not asserting reset) state */
    setup_DWICRSTnIRQ(0);
    Sleep(2);
}

/* @fn      setup_DWICRSTnIRQ
 * @brief   Release RSTn back to idle open-drain output (not asserting reset).
 *          The "enable" parameter is kept for API compatibility with the
 *          Qorvo reference platform; this port does not use RSTn-edge
 *          detection.
 * */
void setup_DWICRSTnIRQ(int enable)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    UNUSED(enable);

    GPIO_InitStruct.Pin = RSTn_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(RSTn_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(RSTn_GPIO_Port, RSTn_Pin, GPIO_PIN_SET);
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn wakeup_device_with_io()
 *
 * @brief This function wakes up the device by toggling io with a delay.
 */
void wakeup_device_with_io(void)
{
    SET_WAKEUP_PIN_IO_HIGH;
    WAIT_200uSEC;
    SET_WAKEUP_PIN_IO_LOW;
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn make_very_short_wakeup_io()
 *
 * @brief This will toggle the wakeup pin for a very short time. The device should not wake up.
 */
void make_very_short_wakeup_io(void)
{
    uint8_t cnt;

    SET_WAKEUP_PIN_IO_HIGH;
    for (cnt = 0; cnt < 10; cnt++)
        __NOP();
    SET_WAKEUP_PIN_IO_LOW;
}

/* @fn      port_set_dw_ic_spi_slowrate
 * @brief   ~4.7 MHz - safe rate before the DW IC crystal oscillator is confirmed running
 *          (SPI1 kernel clock = 150 MHz / 32)
 * */
void port_set_dw_ic_spi_slowrate(void)
{
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    HAL_SPI_Init(&hspi1);
}

/* @fn      port_set_dw_ic_spi_fastrate
 * @brief   ~9.4 MHz - steady-state rate once the DW IC is running
 *          (SPI1 kernel clock = 150 MHz / 16, safely under the DW3000's SPI clock limit)
 * */
void port_set_dw_ic_spi_fastrate(void)
{
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    HAL_SPI_Init(&hspi1);
}

/****************************************************************************
 *
 *                          End APP port section
 *
 *******************************************************************************/

/****************************************************************************
 *
 *                              IRQ section
 *
 *******************************************************************************/

/* @fn         HAL_GPIO_EXTI_Callback
 * @brief      EXTI line detection callback from HAL layer
 * @param      GPIO_Pin: Specifies the port pin connected to corresponding EXTI line.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == DW_IRQn_Pin)
    {
        process_deca_irq();
    }
}

/* @fn      process_deca_irq
 * @brief   main call-back for processing of DW3000 IRQ
 *          it re-enters the IRQ routine and processes all events.
 *          After processing of all events, DW3000 will clear the IRQ line.
 * */
void process_deca_irq(void)
{
    while (port_CheckEXT_IRQ() != 0)
    {
        if (port_dwic_isr)
        {
            port_dwic_isr();
        }
    } /* while DW3000 IRQ line active */
}

/* @fn      port_DisableEXT_IRQ
 * @brief   wrapper to disable DW_IRQn pin IRQ
 * */
void port_DisableEXT_IRQ(void)
{
    HAL_NVIC_DisableIRQ(DECAIRQ_EXTI_IRQn);
}

/* @fn      port_EnableEXT_IRQ
 * @brief   wrapper to enable DW_IRQn pin IRQ
 * */
void port_EnableEXT_IRQ(void)
{
    HAL_NVIC_EnableIRQ(DECAIRQ_EXTI_IRQn);
}

/* @fn      port_GetEXT_IRQStatus
 * @brief   wrapper to read DW_IRQn pin IRQ status
 * */
uint32_t port_GetEXT_IRQStatus(void)
{
    return EXTI_GetITEnStatus(DECAIRQ_EXTI_IRQn);
}

/* @fn      port_CheckEXT_IRQ
 * @brief   wrapper to read DW_IRQn input pin state
 * */
uint32_t port_CheckEXT_IRQ(void)
{
    return HAL_GPIO_ReadPin(DW_IRQn_GPIO_Port, DW_IRQn_Pin);
}

/****************************************************************************
 *
 *                              END OF IRQ section
 *
 *******************************************************************************/

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn port_set_dwic_isr()
 *
 * @brief This function is used to install the handling function for DW IC IRQ.
 *
 * @param dwic_isr function pointer to DW IC interrupt handler to install
 */
void port_set_dwic_isr(port_dwic_isr_t dwic_isr)
{
    /* Check DW IC IRQ activation status. */
    ITStatus en = port_GetEXT_IRQStatus();

    /* Deactivate DW IC IRQ while the new handler is installed. */
    port_DisableEXT_IRQ();

    port_dwic_isr = dwic_isr;

    if (!en)
    {
        port_EnableEXT_IRQ();
    }
}
