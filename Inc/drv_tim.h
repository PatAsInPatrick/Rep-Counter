/*******************************************************************************
 * File Name    : drv_tim.h
 * Description  : Timer driver - TIM2 = ADC trigger, TIM3 = 1 ms system tick.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef DRV_TIM_H
#define DRV_TIM_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Public function prototypes ------------------------------------------------*/
extern void v_timTriggerInit(void);
extern void v_timTickInit(void);
extern uint32_t u4_timGetTickMs(void);

#endif /* DRV_TIM_H */
