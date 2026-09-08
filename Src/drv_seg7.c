/*******************************************************************************
 * File Name    : drv_seg7.c
 * Description  : BCD lines of the training shield.
 *                bit 0 = D9  = PC7
 *                bit 1 = D7  = PA8
 *                bit 2 = D6  = PB10
 *                bit 3 = D8  = PA9
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "drv_seg7.h"

/* Private define ------------------------------------------------------------*/
#define SEG7_BIT0_PIN               (7u)    /* PC7  */
#define SEG7_BIT1_PIN               (8u)    /* PA8  */
#define SEG7_BIT2_PIN               (10u)   /* PB10 */
#define SEG7_BIT3_PIN               (9u)    /* PA9  */

#define SEG7_MODE_MASK              (0x3uL)
#define SEG7_MODE_OUTPUT            (0x1uL)
#define SEG7_MODE_BIT_PER_PIN       (2u)

#define SEG7_MASK_BIT0              (0x1u)
#define SEG7_MASK_BIT1              (0x2u)
#define SEG7_MASK_BIT2              (0x4u)
#define SEG7_MASK_BIT3              (0x8u)

#define SEG7_BSRR_RESET_OFFSET      (16u)

/* Private functions ---------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_seg7SetOutput
 * @brief             - Configure one pin as push-pull output.
 * @param[in]         - pst_port : GPIO port base address
 * @param[in]         - u1_pin   : pin number 0..15
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_seg7SetOutput(GPIO_TypeDef * pst_port, uint8_t u1_pin)
{
    uint32_t u4t_shift = (uint32_t)u1_pin * SEG7_MODE_BIT_PER_PIN;

    pst_port->MODER &= ~(SEG7_MODE_MASK << u4t_shift);
    pst_port->MODER |= (SEG7_MODE_OUTPUT << u4t_shift);
}

/*********************************************************************
 * @fn                - v_seg7WritePin
 * @brief             - Drive one BCD line high or low.
 * @param[in]         - pst_port : GPIO port base address
 * @param[in]         - u1_pin   : pin number 0..15
 * @param[in]         - u1_level : 0 or 1
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_seg7WritePin(GPIO_TypeDef * pst_port, uint8_t u1_pin, uint8_t u1_level)
{
    if (u1_level == 0u)
    {
        pst_port->BSRR = (1uL << ((uint32_t)u1_pin + SEG7_BSRR_RESET_OFFSET));
    }
    else
    {
        pst_port->BSRR = (1uL << (uint32_t)u1_pin);
    }
}

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_seg7Init
 * @brief             - Configure the four BCD lines and blank the display.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_seg7Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    v_seg7SetOutput(GPIOC, (uint8_t)SEG7_BIT0_PIN);
    v_seg7SetOutput(GPIOA, (uint8_t)SEG7_BIT1_PIN);
    v_seg7SetOutput(GPIOB, (uint8_t)SEG7_BIT2_PIN);
    v_seg7SetOutput(GPIOA, (uint8_t)SEG7_BIT3_PIN);

    v_seg7WriteDigit((uint8_t)SEG7_BLANK);
}

/*********************************************************************
 * @fn                - v_seg7WriteDigit
 * @brief             - Show one decimal digit. Any value above nine
 *                      blanks the display on a 4511 style decoder.
 * @param[in]         - u1_digit : 0..9, or SEG7_BLANK
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_seg7WriteDigit(uint8_t u1_digit)
{
    uint8_t u1t_value = u1_digit;

    if (u1t_value > (uint8_t)SEG7_DIGIT_MAX)
    {
        u1t_value = (uint8_t)SEG7_BLANK;
    }

    v_seg7WritePin(GPIOC, (uint8_t)SEG7_BIT0_PIN,
                   (uint8_t)(u1t_value & (uint8_t)SEG7_MASK_BIT0));
    v_seg7WritePin(GPIOA, (uint8_t)SEG7_BIT1_PIN,
                   (uint8_t)(u1t_value & (uint8_t)SEG7_MASK_BIT1));
    v_seg7WritePin(GPIOB, (uint8_t)SEG7_BIT2_PIN,
                   (uint8_t)(u1t_value & (uint8_t)SEG7_MASK_BIT2));
    v_seg7WritePin(GPIOA, (uint8_t)SEG7_BIT3_PIN,
                   (uint8_t)(u1t_value & (uint8_t)SEG7_MASK_BIT3));
}
