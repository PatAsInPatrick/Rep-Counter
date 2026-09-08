/*******************************************************************************
 * File Name    : drv_led.h
 * Description  : The four LEDs of the STEO training shield.
 *                D13 = PA5 blue, D12 = PA6 red, D11 = PA7 yellow,
 *                D10 = PB6 green.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef DRV_LED_H
#define DRV_LED_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Public define -------------------------------------------------------------*/
#define LED_GREEN                   (0u)    /* D10 PB6, bottom of the row     */
#define LED_YELLOW                  (1u)    /* D11 PA7                        */
#define LED_RED                     (2u)    /* D12 PA6                        */
#define LED_BLUE                    (3u)    /* D13 PA5, top of the row        */
#define LED_COUNT                   (4u)

#define LED_OFF                     (0u)
#define LED_ON                      (1u)

/* Public function prototypes ------------------------------------------------*/
extern void v_ledInit(void);
extern void v_ledWrite(uint8_t u1_index, uint8_t u1_state);
extern void v_ledWriteAll(uint8_t u1_mask);

#endif /* DRV_LED_H */
