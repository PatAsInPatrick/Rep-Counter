/*******************************************************************************
 * File Name    : drv_seg7.h
 * Description  : 7-Segment driver of the STEO training shield.
 *                The display is driven through a BCD decoder, so only four
 *                GPIO lines are needed to show one decimal digit.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef DRV_SEG7_H
#define DRV_SEG7_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Public define -------------------------------------------------------------*/
#define SEG7_BLANK                  (15u)   /* value that blanks the display  */
#define SEG7_DIGIT_MAX              (9u)

/* Public function prototypes ------------------------------------------------*/
extern void v_seg7Init(void);
extern void v_seg7WriteDigit(uint8_t u1_digit);

#endif /* DRV_SEG7_H */
