/*******************************************************************************
 * File Name    : app_overheat.h
 * Description  : Emergency shutdown when the room becomes too hot.
 *
 *                The comparison is made by the analog watchdog inside the
 *                ADC, not by software, so the alarm still works while the
 *                main loop is busy drawing the screen or waiting on the
 *                I2C bus. The interrupt handler acts on the hardware
 *                straight away instead of leaving a flag for later.
 * Date         : 2026-09-08
 ******************************************************************************/
#ifndef APP_OVERHEAT_H
#define APP_OVERHEAT_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Public define -------------------------------------------------------------*/
/* Counts of the thermistor channel. The resistance drops when it warms up,
 * so a hot room gives a LOW count and the alarm sits on the lower limit.
 *
 *      degC    count
 *       26     2002
 *       27     1957
 *       28     1913
 *       29     1868
 *       30     1825
 *       32     1739
 *       35     1614
 *
 * The pair below is set low on purpose so that the alarm can be reached by
 * holding a finger on the thermistor during a demonstration. Put the values
 * of 35 and 32 back for normal use. */
#define OVERHEAT_TRIP_COUNT         (1868u) /* about 29.0 degC               */
#define OVERHEAT_REARM_COUNT        (1957u) /* about 27.0 degC, hysteresis   */
#define OVERHEAT_TRIP_DECI          (290)   /* shown on the screen           */

/* Public function prototypes ------------------------------------------------*/
extern void v_overheatInit(void);
extern uint8_t u1_overheatIsActive(void);
extern uint8_t u1_overheatHasNewEvent(void);
extern void v_overheatClearNewEvent(void);
extern uint8_t u1_overheatGetCount(void);
extern void v_overheatUpdate(uint16_t u2_rawCount);

#endif /* APP_OVERHEAT_H */
