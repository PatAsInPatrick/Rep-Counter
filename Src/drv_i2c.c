/*******************************************************************************
 * File Name    : drv_i2c.c
 * Description  : Register level I2C1 driver.
 *                Every wait loop is protected by a timeout so that a missing
 *                or badly wired device can never freeze the application.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include "drv_i2c.h"

/* Private define ------------------------------------------------------------*/
#define I2C_SCL_PIN                 (8u)    /* PB8 */
#define I2C_SDA_PIN                 (9u)    /* PB9 */
#define I2C_ALTERNATE_FUNCTION      (4u)    /* AF4 = I2C1 */

#define I2C_MODE_MASK               (0x3uL)
#define I2C_MODE_ALTERNATE          (0x2uL)
#define I2C_MODE_BIT_PER_PIN        (2u)
#define I2C_AF_MASK                 (0xFuL)
#define I2C_AF_BIT_PER_PIN          (4u)
#define I2C_PUPD_PULLUP             (0x1uL)
#define I2C_SPEED_VERY_HIGH         (0x3uL)

#define I2C_PCLK1_MHZ               (16u)
#define I2C_CCR_STANDARD_100K       (80u)   /* 16 MHz / (2 x 100 kHz)         */
#define I2C_TRISE_STANDARD          (17u)   /* 1000 ns / 62.5 ns + 1          */

#define I2C_WRITE_DIRECTION         (0u)
#define I2C_ADDRESS_SHIFT           (1u)
#define I2C_TIMEOUT_LOOP            (100000uL)

/* Private function prototypes -----------------------------------------------*/
static uint8_t u1_i2cWaitFlagSr1(uint32_t u4_flag);
static uint8_t u1_i2cWaitBusFree(void);
static uint8_t u1_i2cSendStartAndAddress(uint8_t u1_address);
static void v_i2cSendStop(void);

/* Private functions ---------------------------------------------------------*/

/*********************************************************************
 * @fn                - u1_i2cWaitFlagSr1
 * @brief             - Wait for one status flag with a timeout.
 * @param[in]         - u4_flag : mask inside register SR1
 * @return            - uint8_t I2C_RESULT_OK or I2C_RESULT_ERROR
 *//////////////////////////////////////////////////////////////////////
static uint8_t u1_i2cWaitFlagSr1(uint32_t u4_flag)
{
    uint32_t u4t_guard = I2C_TIMEOUT_LOOP;
    uint8_t  u1t_result = (uint8_t)I2C_RESULT_ERROR;

    while (u4t_guard > 0uL)
    {
        if ((I2C1->SR1 & u4_flag) != 0uL)
        {
            u1t_result = (uint8_t)I2C_RESULT_OK;
            u4t_guard = 0uL;
        }
        else
        {
            u4t_guard--;
        }
    }

    return u1t_result;
}

/*********************************************************************
 * @fn                - u1_i2cWaitBusFree
 * @brief             - Wait until the previous stop condition has really
 *                      finished. Starting a new frame too early corrupts
 *                      the data that the display receives.
 * @return            - uint8_t I2C_RESULT_OK or I2C_RESULT_ERROR
 *//////////////////////////////////////////////////////////////////////
static uint8_t u1_i2cWaitBusFree(void)
{
    uint32_t u4t_guard = I2C_TIMEOUT_LOOP;
    uint8_t  u1t_result = (uint8_t)I2C_RESULT_ERROR;

    while (u4t_guard > 0uL)
    {
        if ((I2C1->SR2 & I2C_SR2_BUSY) == 0uL)
        {
            u1t_result = (uint8_t)I2C_RESULT_OK;
            u4t_guard = 0uL;
        }
        else
        {
            u4t_guard--;
        }
    }

    return u1t_result;
}

/*********************************************************************
 * @fn                - u1_i2cSendStartAndAddress
 * @brief             - Generate a start condition and send the address.
 * @param[in]         - u1_address : 7 bit device address
 * @return            - uint8_t I2C_RESULT_OK or I2C_RESULT_ERROR
 *//////////////////////////////////////////////////////////////////////
static uint8_t u1_i2cSendStartAndAddress(uint8_t u1_address)
{
    uint8_t  u1t_result = (uint8_t)I2C_RESULT_OK;
    volatile uint32_t u4t_dummy = 0uL;

    u1t_result = u1_i2cWaitBusFree();

    if (u1t_result == (uint8_t)I2C_RESULT_OK)
    {
        I2C1->CR1 |= I2C_CR1_START;
        u1t_result = u1_i2cWaitFlagSr1(I2C_SR1_SB);
    }

    if (u1t_result == (uint8_t)I2C_RESULT_OK)
    {
        I2C1->DR = (uint32_t)(((uint32_t)u1_address << I2C_ADDRESS_SHIFT)
                 | (uint32_t)I2C_WRITE_DIRECTION);
        u1t_result = u1_i2cWaitFlagSr1(I2C_SR1_ADDR);
    }

    if (u1t_result == (uint8_t)I2C_RESULT_OK)
    {
        /* the ADDR flag is cleared by reading SR1 and then SR2 */
        u4t_dummy = I2C1->SR1;
        u4t_dummy = I2C1->SR2;
        (void)u4t_dummy;
    }
    else
    {
        v_i2cSendStop();
    }

    return u1t_result;
}

/*********************************************************************
 * @fn                - v_i2cSendStop
 * @brief             - Generate a stop condition.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_i2cSendStop(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
}

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_i2cInit
 * @brief             - Configure PB8 and PB9 as open drain alternate
 *                      function, then set I2C1 to 400 kHz.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_i2cInit(void)
{
    uint32_t u4t_shiftScl = (uint32_t)I2C_SCL_PIN * I2C_MODE_BIT_PER_PIN;
    uint32_t u4t_shiftSda = (uint32_t)I2C_SDA_PIN * I2C_MODE_BIT_PER_PIN;
    uint32_t u4t_afScl = ((uint32_t)I2C_SCL_PIN - 8uL) * I2C_AF_BIT_PER_PIN;
    uint32_t u4t_afSda = ((uint32_t)I2C_SDA_PIN - 8uL) * I2C_AF_BIT_PER_PIN;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    GPIOB->MODER &= ~(I2C_MODE_MASK << u4t_shiftScl);
    GPIOB->MODER |= (I2C_MODE_ALTERNATE << u4t_shiftScl);
    GPIOB->MODER &= ~(I2C_MODE_MASK << u4t_shiftSda);
    GPIOB->MODER |= (I2C_MODE_ALTERNATE << u4t_shiftSda);

    GPIOB->OTYPER |= (1uL << I2C_SCL_PIN);
    GPIOB->OTYPER |= (1uL << I2C_SDA_PIN);

    GPIOB->OSPEEDR |= (I2C_SPEED_VERY_HIGH << u4t_shiftScl);
    GPIOB->OSPEEDR |= (I2C_SPEED_VERY_HIGH << u4t_shiftSda);

    GPIOB->PUPDR &= ~(I2C_MODE_MASK << u4t_shiftScl);
    GPIOB->PUPDR |= (I2C_PUPD_PULLUP << u4t_shiftScl);
    GPIOB->PUPDR &= ~(I2C_MODE_MASK << u4t_shiftSda);
    GPIOB->PUPDR |= (I2C_PUPD_PULLUP << u4t_shiftSda);

    GPIOB->AFR[1] &= ~(I2C_AF_MASK << u4t_afScl);
    GPIOB->AFR[1] |= ((uint32_t)I2C_ALTERNATE_FUNCTION << u4t_afScl);
    GPIOB->AFR[1] &= ~(I2C_AF_MASK << u4t_afSda);
    GPIOB->AFR[1] |= ((uint32_t)I2C_ALTERNATE_FUNCTION << u4t_afSda);

    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->CR2 = (uint32_t)I2C_PCLK1_MHZ;
    I2C1->CCR = (uint32_t)I2C_CCR_STANDARD_100K;
    I2C1->TRISE = (uint32_t)I2C_TRISE_STANDARD;
    I2C1->CR1 |= I2C_CR1_PE;
}

/*********************************************************************
 * @fn                - u1_i2cWrite
 * @brief             - Write a short frame. Used for configuration
 *                      commands only, never for the screen buffer.
 * @param[in]         - u1_address : 7 bit device address
 * @param[in]         - pu1_data   : bytes to send
 * @param[in]         - u2_length  : number of bytes
 * @return            - uint8_t I2C_RESULT_OK or I2C_RESULT_ERROR
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_i2cWrite(uint8_t u1_address, const uint8_t * pu1_data,
                    uint16_t u2_length)
{
    uint8_t  u1t_result = (uint8_t)I2C_RESULT_ERROR;
    uint16_t u2t_index = 0u;

    if ((pu1_data != NULL) && (u2_length > 0u))
    {
        u1t_result = u1_i2cSendStartAndAddress(u1_address);

        while ((u2t_index < u2_length) && (u1t_result == (uint8_t)I2C_RESULT_OK))
        {
            u1t_result = u1_i2cWaitFlagSr1(I2C_SR1_TXE);
            if (u1t_result == (uint8_t)I2C_RESULT_OK)
            {
                I2C1->DR = (uint32_t)pu1_data[u2t_index];
                u2t_index++;
            }
        }

        if (u1t_result == (uint8_t)I2C_RESULT_OK)
        {
            u1t_result = u1_i2cWaitFlagSr1(I2C_SR1_BTF);
        }

        v_i2cSendStop();
    }

    return u1t_result;
}

/*********************************************************************
 * @fn                - u1_i2cWriteBlock
 * @brief             - Send one control byte followed by a large block of
 *                      data inside a single I2C frame. Used by the screen.
 * @param[in]         - u1_address : 7 bit device address
 * @param[in]         - u1_control : first byte sent after the address
 * @param[in]         - pu1_data   : block of data
 * @param[in]         - u2_length  : number of bytes in the block
 * @return            - uint8_t I2C_RESULT_OK or I2C_RESULT_ERROR
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_i2cWriteBlock(uint8_t u1_address, uint8_t u1_control,
                         const uint8_t * pu1_data, uint16_t u2_length)
{
    uint8_t  u1t_result = (uint8_t)I2C_RESULT_ERROR;
    uint16_t u2t_index = 0u;

    if ((pu1_data != NULL) && (u2_length > 0u))
    {
        u1t_result = u1_i2cSendStartAndAddress(u1_address);

        if (u1t_result == (uint8_t)I2C_RESULT_OK)
        {
            u1t_result = u1_i2cWaitFlagSr1(I2C_SR1_TXE);
        }

        if (u1t_result == (uint8_t)I2C_RESULT_OK)
        {
            I2C1->DR = (uint32_t)u1_control;
        }

        while ((u2t_index < u2_length) && (u1t_result == (uint8_t)I2C_RESULT_OK))
        {
            u1t_result = u1_i2cWaitFlagSr1(I2C_SR1_TXE);
            if (u1t_result == (uint8_t)I2C_RESULT_OK)
            {
                I2C1->DR = (uint32_t)pu1_data[u2t_index];
                u2t_index++;
            }
        }

        if (u1t_result == (uint8_t)I2C_RESULT_OK)
        {
            u1t_result = u1_i2cWaitFlagSr1(I2C_SR1_BTF);
        }

        v_i2cSendStop();
    }

    return u1t_result;
}
