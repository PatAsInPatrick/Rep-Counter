/*******************************************************************************
 * File Name    : app_energy.h
 * Description  : Holds the dumbbell weight set by the user and turns the
 *                lifting work into an estimate of the energy burned.
 * Date         : 2026-09-08
 ******************************************************************************/
#ifndef APP_ENERGY_H
#define APP_ENERGY_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Public define -------------------------------------------------------------*/
#define ENERGY_WEIGHT_MIN_HG        (25u)   /* 2.5 kg, stored in 0.1 kg units */
#define ENERGY_WEIGHT_MAX_HG        (300u)  /* 30.0 kg                        */
#define ENERGY_WEIGHT_STEP_HG       (25u)   /* 2.5 kg per button press        */

/* Public function prototypes ------------------------------------------------*/
extern void v_energyInit(void);
extern void v_energyWeightUp(void);
extern void v_energyWeightDown(void);
extern uint16_t u2_energyGetWeightHg(void);
extern void v_energyResetSet(void);
extern void v_energyAddRepetition(uint16_t u2_romDeg);
extern void v_energySetTemperature(int16_t s2_temperatureDeci);
extern uint16_t u2_energyGetKcalX100(void);
extern uint8_t u1_energyGetHeatPercent(void);

#endif /* APP_ENERGY_H */
