/*******************************************************************************
 * File Name    : drv_led.c
 * Description  : Register level driver of the four shield LEDs.
 *                All of them are driven high to light up.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "drv_led.h"

/* Private define ------------------------------------------------------------*/
#define LED_GREEN_PIN               (6u)    /* PB6 */
#define LED_YELLOW_PIN              (7u)    /* PA7 */
#define LED_RED_PIN                 (6u)    /* PA6 */
#define LED_BLUE_PIN                (5u)    /* PA5 */

#define LED_MODE_MASK               (0x3uL)
#define LED_MODE_OUTPUT             (0x1uL)
#define LED_MODE_BIT_PER_PIN        (2u)
#define LED_BSRR_RESET_OFFSET       (16u)

/* Private variables ---------------------------------------------------------*/
static const uint8_t u1g_ledPin[LED_COUNT] =
{
    LED_GREEN_PIN, LED_YELLOW_PIN, LED_RED_PIN, LED_BLUE_PIN
};

/* Private function prototypes -----------------------------------------------*/
static GPIO_TypeDef * pst_ledPort(uint8_t u1_index);

/* Private functions ---------------------------------------------------------*/

/*********************************************************************
 * @fn                - pst_ledPort
 * @brief             - Return the port that owns one LED.
 * @param[in]         - u1_index : LED_GREEN to LED_BLUE
 * @return            - GPIO_TypeDef * port base address
 *//////////////////////////////////////////////////////////////////////
static GPIO_TypeDef * pst_ledPort(uint8_t u1_index)
{
    GPIO_TypeDef * pst_port = GPIOA;

    if (u1_index == (uint8_t)LED_GREEN)
    {
        pst_port = GPIOB;
    }

    return pst_port;
}

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_ledInit
 * @brief             - Configure the four LED pins as output and clear them.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_ledInit(void)
{
    uint8_t  u1t_index = 0u;
    uint32_t u4t_shift = 0uL;
    GPIO_TypeDef * pst_port = GPIOA;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    for (u1t_index = 0u; u1t_index < (uint8_t)LED_COUNT; u1t_index++)
    {
        pst_port = pst_ledPort(u1t_index);
        u4t_shift = (uint32_t)u1g_ledPin[u1t_index] * LED_MODE_BIT_PER_PIN;

        pst_port->MODER &= ~(LED_MODE_MASK << u4t_shift);
        pst_port->MODER |= (LED_MODE_OUTPUT << u4t_shift);
    }

    v_ledWriteAll(0u);
}

/*********************************************************************
 * @fn                - v_ledWrite
 * @brief             - Turn one LED on or off.
 * @param[in]         - u1_index : LED_GREEN to LED_BLUE
 * @param[in]         - u1_state : LED_ON or LED_OFF
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_ledWrite(uint8_t u1_index, uint8_t u1_state)
{
    GPIO_TypeDef * pst_port = GPIOA;

    if (u1_index < (uint8_t)LED_COUNT)
    {
        pst_port = pst_ledPort(u1_index);

        if (u1_state == (uint8_t)LED_OFF)
        {
            pst_port->BSRR = (1uL << ((uint32_t)u1g_ledPin[u1_index]
                           + LED_BSRR_RESET_OFFSET));
        }
        else
        {
            pst_port->BSRR = (1uL << (uint32_t)u1g_ledPin[u1_index]);
        }
    }
}

/*********************************************************************
 * @fn                - v_ledWriteAll
 * @brief             - Write the four LEDs at once from a bit mask.
 *                      bit 0 = green, bit 1 = yellow, bit 2 = red,
 *                      bit 3 = blue.
 * @param[in]         - u1_mask : pattern to display
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_ledWriteAll(uint8_t u1_mask)
{
    uint8_t u1t_index = 0u;
    uint8_t u1t_state = 0u;

    for (u1t_index = 0u; u1t_index < (uint8_t)LED_COUNT; u1t_index++)
    {
        if ((u1_mask & (uint8_t)(1u << u1t_index)) != 0u)
        {
            u1t_state = (uint8_t)LED_ON;
        }
        else
        {
            u1t_state = (uint8_t)LED_OFF;
        }

        v_ledWrite(u1t_index, u1t_state);
    }
}
