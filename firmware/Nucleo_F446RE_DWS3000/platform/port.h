/*! ----------------------------------------------------------------------------
 * @file    port.h
 * @brief   HW specific definitions and functions for portability
 *
 *          Adapted from Qorvo's STM_Nucleo_F429 reference platform for the
 *          Nucleo-F446RE + DWS3000 shield (single SPI, no USB/LCD/second device).
 */

#ifndef PORT_H_
#define PORT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <string.h>

#include <stm32f4xx_hal.h>
#include <main.h>

    /* DW IC IRQ handler type. */
    typedef void (*port_dwic_isr_t)(void);

    /*! ------------------------------------------------------------------------------------------------------------------
     * @fn port_set_dwic_isr()
     *
     * @brief This function is used to install the handling function for DW3xxx/QM33xx IRQ.
     *
     * NOTE:
     *   - The user application shall ensure that a proper handler is set by calling this function before any
     *     DW IC IRQ occurs.
     *   - This function deactivates the DW IC IRQ line while the handler is installed.
     *
     * @param isr function pointer to DW IC interrupt handler to install
     *
     * @return none
     */
    void port_set_dwic_isr(port_dwic_isr_t isr);

/* DW_IRQn is on PA9, which is on the shared EXTI9_5 vector (see main.h). */
#define DECAIRQ_EXTI_IRQn DW_IRQn_EXTI_IRQn

    /****************************************************************************
     *
     *                              port function prototypes
     *
     *******************************************************************************/

    int usleep(uint32_t usec);
    void Sleep(uint32_t Delay);
    uint32_t portGetTickCnt(void);

    void port_set_dw_ic_spi_slowrate(void);
    void port_set_dw_ic_spi_fastrate(void);

    void process_deca_irq(void);

    int peripherals_init(void);
    void spi_peripheral_init(void);

    void setup_DWICRSTnIRQ(int enable);
    void reset_DWIC(void);

    ITStatus EXTI_GetITEnStatus(IRQn_Type x);

    uint32_t port_GetEXT_IRQStatus(void);
    uint32_t port_CheckEXT_IRQ(void);
    void port_DisableEXT_IRQ(void);
    void port_EnableEXT_IRQ(void);

    /*! ------------------------------------------------------------------------------------------------------------------
     * @fn wakeup_device_with_io()
     *
     * @brief This function wakes up the device by toggling the WAKEUP io with a delay.
     */
    void wakeup_device_with_io(void);

    /*! ------------------------------------------------------------------------------------------------------------------
     * @fn make_very_short_wakeup_io()
     *
     * @brief This will toggle the wakeup pin for a very short time. The device should not wake up.
     */
    void make_very_short_wakeup_io(void);

#define SET_WAKEUP_PIN_IO_LOW  HAL_GPIO_WritePin(WAKEUP_GPIO_Port, WAKEUP_Pin, GPIO_PIN_RESET)
#define SET_WAKEUP_PIN_IO_HIGH HAL_GPIO_WritePin(WAKEUP_GPIO_Port, WAKEUP_Pin, GPIO_PIN_SET)

#define WAIT_500uSEC Sleep(1) /* should be >=500us; Sleep(1) (1ms) comfortably covers it */
#define WAIT_200uSEC Sleep(1) /* should be >=200us; Sleep(1) (1ms) comfortably covers it */

#ifdef __cplusplus
}
#endif

#endif /* PORT_H_ */
