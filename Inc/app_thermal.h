/*******************************************************************************
 * File Name    : app_thermal.h
 * Description  : Converts the raw reading of the NTC thermistor on PA0 into
 *                a temperature in tenths of a degree Celsius.
 * Date         : 2026-09-08
 ******************************************************************************/
#ifndef APP_THERMAL_H
#define APP_THERMAL_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Public define -------------------------------------------------------------*/
#define THERMAL_HOT_LIMIT_DECI      (320)   /* 32.0 degC, warn above this     */

/* Public function prototypes ------------------------------------------------*/
extern void v_thermalInit(void);
extern void v_thermalUpdate(uint16_t u2_rawCount);
extern int16_t s2_thermalGetDeci(void);
extern uint8_t u1_thermalIsHot(void);

#endif /* APP_THERMAL_H */
