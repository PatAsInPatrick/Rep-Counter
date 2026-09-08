/*******************************************************************************
 * File Name    : drv_adc.h
 * Description  : ADC1 driver, hardware triggered by TIM2 and served by DMA2.
 *                No polling is used anywhere in this module.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef DRV_ADC_H
#define DRV_ADC_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Public define -------------------------------------------------------------*/
#define ADC_MAX_COUNT               (4095u)

/* Public function prototypes ------------------------------------------------*/
extern void v_adcInit(void);
extern uint8_t u1_adcIsSampleReady(void);
extern void v_adcClearSampleReady(void);
extern uint16_t u2_adcGetFiltered(void);

#endif /* DRV_ADC_H */
