/*******************************************************************************
 * File Name    : drv_i2c.h
 * Description  : I2C1 driver on PB8 (SCL) and PB9 (SDA), fast mode 400 kHz.
 *                All transfers are done by direct register access. The screen
 *                is refreshed only a few times per set, so the transfer time
 *                never disturbs the measurement of the lifting tempo.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef DRV_I2C_H
#define DRV_I2C_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Public define -------------------------------------------------------------*/
#define I2C_RESULT_OK               (0u)
#define I2C_RESULT_ERROR            (1u)

/* Public function prototypes ------------------------------------------------*/
extern void v_i2cInit(void);
extern uint8_t u1_i2cWrite(uint8_t u1_address, const uint8_t * pu1_data,
                           uint16_t u2_length);
extern uint8_t u1_i2cWriteBlock(uint8_t u1_address, uint8_t u1_control,
                                const uint8_t * pu1_data, uint16_t u2_length);

#endif /* DRV_I2C_H */
