/*******************************************************************************
 * File Name    : drv_gpio.c
 * Description  : GPIO driver - register level, no HAL.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "drv_gpio.h"

/* Private define ------------------------------------------------------------*/
/* --- CHANGE HERE when moving to the shield LEDs / shield switches --------- */
#define GPIO_ANALOG_PIN             (4u)    /* PA4  : potentiometer ADC1_IN4  */
/* Shield revision note : if the potentiometer is on PB1 use ADC1_IN9 instead */
#define GPIO_UART_TX_PIN            (2u)    /* PA2  : USART2_TX               */
#define GPIO_UART_RX_PIN            (3u)    /* PA3  : USART2_RX               */

#define GPIO_MODE_MASK              (0x3u)
#define GPIO_MODE_INPUT             (0x0u)
#define GPIO_MODE_ALTERNATE         (0x2u)
#define GPIO_MODE_ANALOG            (0x3u)  /* analog is 0b11, not 0b10       */

#define GPIO_PUPD_MASK              (0x3u)
#define GPIO_PUPD_NONE              (0x0u)

#define GPIO_AF7_USART2             (0x7u)
#define GPIO_AF_MASK                (0xFu)

#define GPIO_MODE_BIT_PER_PIN       (2u)
#define GPIO_AF_BIT_PER_PIN         (4u)

/* Private functions ---------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_gpioSetMode
 * @brief             - Write the 2-bit mode field of one pin.
 * @param[in]         - pst_port : GPIO port base address
 * @param[in]         - u1_pin   : pin number 0..15
 * @param[in]         - u1_mode  : GPIO_MODE_xxx
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_gpioSetMode(GPIO_TypeDef * pst_port, uint8_t u1_pin, uint8_t u1_mode)
{
    uint32_t u4t_shift = (uint32_t)u1_pin * GPIO_MODE_BIT_PER_PIN;

    pst_port->MODER &= ~(GPIO_MODE_MASK << u4t_shift);
    pst_port->MODER |= ((uint32_t)u1_mode << u4t_shift);
}

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_gpioAnalogInit
 * @brief             - Configure the potentiometer pin as analog input.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_gpioAnalogInit(void)
{
    uint32_t u4t_shift = (uint32_t)GPIO_ANALOG_PIN * GPIO_MODE_BIT_PER_PIN;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    v_gpioSetMode(GPIOA, (uint8_t)GPIO_ANALOG_PIN, (uint8_t)GPIO_MODE_ANALOG);
    GPIOA->PUPDR &= ~(GPIO_PUPD_MASK << u4t_shift);
}

/*********************************************************************
 * @fn                - v_gpioUartPinInit
 * @brief             - Configure PA2 and PA3 as USART2 alternate function.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_gpioUartPinInit(void)
{
    uint32_t u4t_shiftTx = (uint32_t)GPIO_UART_TX_PIN * GPIO_AF_BIT_PER_PIN;
    uint32_t u4t_shiftRx = (uint32_t)GPIO_UART_RX_PIN * GPIO_AF_BIT_PER_PIN;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    v_gpioSetMode(GPIOA, (uint8_t)GPIO_UART_TX_PIN, (uint8_t)GPIO_MODE_ALTERNATE);
    v_gpioSetMode(GPIOA, (uint8_t)GPIO_UART_RX_PIN, (uint8_t)GPIO_MODE_ALTERNATE);

    GPIOA->AFR[0] &= ~(GPIO_AF_MASK << u4t_shiftTx);
    GPIOA->AFR[0] |= (GPIO_AF7_USART2 << u4t_shiftTx);
    GPIOA->AFR[0] &= ~(GPIO_AF_MASK << u4t_shiftRx);
    GPIOA->AFR[0] |= (GPIO_AF7_USART2 << u4t_shiftRx);
}
