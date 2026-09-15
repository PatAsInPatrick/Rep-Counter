/*******************************************************************************
 * File Name    : drv_adc.h
 * Description  : ADC1 driver, hardware triggered by TIM2 and served by DMA2.
 *                Two channels are converted in one sequence, the arm angle
 *                on PA4 and the thermistor on PA0.
 *                No polling is used anywhere in this module.
 *
 *                The analog watchdog of the ADC compares every converted
 *                value against a limit in hardware. The CPU is only
 *                interrupted when the value leaves the allowed window, which
 *                makes it an emergency path that works even when the main
 *                loop is busy somewhere else.
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
extern uint16_t u2_adcGetAngleRaw(void);
extern uint16_t u2_adcGetThermalRaw(void);
extern void v_adcWatchdogInit(uint16_t u2_lowLimit);
extern void v_adcWatchdogArm(void);
extern void v_adcWatchdogHook(void);

#endif /* DRV_ADC_H */
