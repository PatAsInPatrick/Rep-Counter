/*******************************************************************************
 * File Name    : drv_exti.h
 * Description  : External interrupt driver for four shield switches.
 *                D2 = PA10, D3 = PB3, D4 = PB5, D5 = PB4.
 *                The four lines belong to four different interrupt vectors,
 *                which is why the driver returns the events as a bit mask.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef DRV_EXTI_H
#define DRV_EXTI_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Public define -------------------------------------------------------------*/
#define EXTI_EVENT_START_STOP       (0x1u)  /* D2 PA10 : start or stop a set  */
#define EXTI_EVENT_OVERVIEW         (0x2u)  /* D3 PB3  : back to the overview */
#define EXTI_EVENT_NEXT_PAGE        (0x4u)  /* D4 PB5  : next detail page     */
#define EXTI_EVENT_CLEAR_FATIGUE    (0x8u)  /* D5 PB4  : new fatigue baseline */

/* Public function prototypes ------------------------------------------------*/
extern void v_extiButtonInit(void);
extern uint8_t u1_extiGetEvents(void);
extern void v_extiClearEvent(uint8_t u1_mask);

#endif /* DRV_EXTI_H */
