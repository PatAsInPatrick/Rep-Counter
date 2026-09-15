/*******************************************************************************
 * File Name    : app_energy.c
 * Description  : Energy model of one training set.
 *
 *                Mechanical work of one repetition
 *                    W = m * g * h
 *                where h is the vertical travel of the dumbbell, taken as
 *                proportional to the range of motion of that repetition.
 *
 *                The lowering phase also costs energy, so the mechanical work
 *                is scaled up. Muscles turn only about a fifth of the food
 *                energy into work, so the result is divided by that
 *                efficiency to obtain the energy the body actually spends.
 *
 *                A warm room raises the cost because the body must also cool
 *                itself, so the total is increased above a comfort limit.
 *
 *                Everything is computed with integers. Weight is kept in
 *                units of 0.1 kg and energy in millijoules.
 * Date         : 2026-09-08
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "app_energy.h"

/* Private define ------------------------------------------------------------*/
#define ENERGY_TRAVEL_FULL_MM       (600u)  /* travel of a full 180 deg lift  */
#define ENERGY_ANGLE_FULL_DEG       (180u)
#define ENERGY_GRAVITY_SCALED       (981u)  /* 9.81 m/s2 times one hundred    */
#define ENERGY_GRAVITY_DIVIDER      (1000u)
#define ENERGY_ECCENTRIC_PERCENT    (133u)  /* lowering adds a third          */
#define ENERGY_EFFICIENCY_PERCENT   (22u)   /* muscle efficiency              */
#define ENERGY_PERCENT_FULL         (100u)
#define ENERGY_MJ_PER_KCAL_X100     (41840uL)

#define ENERGY_COMFORT_DECI         (250)   /* 25.0 degC                      */
#define ENERGY_HEAT_PER_DEGREE      (1u)    /* one percent for each degree    */
#define ENERGY_HEAT_MAX_PERCENT     (15u)
#define ENERGY_DECI_PER_DEGREE      (10)

/* Private variables ---------------------------------------------------------*/
static uint16_t u2g_weightHg = 100u;        /* 10.0 kg by default             */
static uint32_t u4g_workMilliJoule = 0uL;
static uint8_t  u1g_heatPercent = 0u;

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_energyInit
 * @brief             - Bring the module to a known state.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_energyInit(void)
{
    u2g_weightHg = 100u;
    u4g_workMilliJoule = 0uL;
    u1g_heatPercent = 0u;
}

/*********************************************************************
 * @fn                - v_energyWeightUp
 * @brief             - Raise the configured weight by one step.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_energyWeightUp(void)
{
    if (u2g_weightHg < (uint16_t)ENERGY_WEIGHT_MAX_HG)
    {
        u2g_weightHg = (uint16_t)(u2g_weightHg + ENERGY_WEIGHT_STEP_HG);
    }
}

/*********************************************************************
 * @fn                - v_energyWeightDown
 * @brief             - Lower the configured weight by one step.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_energyWeightDown(void)
{
    if (u2g_weightHg > (uint16_t)ENERGY_WEIGHT_MIN_HG)
    {
        u2g_weightHg = (uint16_t)(u2g_weightHg - ENERGY_WEIGHT_STEP_HG);
    }
}

/*********************************************************************
 * @fn                - u2_energyGetWeightHg
 * @brief             - Return the configured weight in units of 0.1 kg.
 * @return            - uint16_t for example 125 means 12.5 kg
 *//////////////////////////////////////////////////////////////////////
uint16_t u2_energyGetWeightHg(void)
{
    return u2g_weightHg;
}

/*********************************************************************
 * @fn                - v_energyResetSet
 * @brief             - Clear the energy accumulated in the previous set.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_energyResetSet(void)
{
    u4g_workMilliJoule = 0uL;
}

/*********************************************************************
 * @fn                - v_energyAddRepetition
 * @brief             - Add the work of one counted repetition.
 * @param[in]         - u2_romDeg : range of motion of that repetition
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_energyAddRepetition(uint16_t u2_romDeg)
{
    uint32_t u4t_travelMm = 0uL;
    uint32_t u4t_milliJoule = 0uL;

    u4t_travelMm = ((uint32_t)u2_romDeg * (uint32_t)ENERGY_TRAVEL_FULL_MM)
                 / (uint32_t)ENERGY_ANGLE_FULL_DEG;

    /* work in millijoules = weight * travel * gravity, all scaled */
    u4t_milliJoule = ((uint32_t)u2g_weightHg * u4t_travelMm
                   * (uint32_t)ENERGY_GRAVITY_SCALED)
                   / (uint32_t)ENERGY_GRAVITY_DIVIDER;

    u4t_milliJoule = (u4t_milliJoule * (uint32_t)ENERGY_ECCENTRIC_PERCENT)
                   / (uint32_t)ENERGY_PERCENT_FULL;

    u4t_milliJoule = (u4t_milliJoule * (uint32_t)ENERGY_PERCENT_FULL)
                   / (uint32_t)ENERGY_EFFICIENCY_PERCENT;

    u4g_workMilliJoule += u4t_milliJoule;
}

/*********************************************************************
 * @fn                - v_energySetTemperature
 * @brief             - Store how much the room temperature adds to the cost.
 * @param[in]         - s2_temperatureDeci : temperature in tenths of a degree
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_energySetTemperature(int16_t s2_temperatureDeci)
{
    int16_t s2t_excess = 0;
    uint16_t u2t_percent = 0u;

    s2t_excess = (int16_t)(s2_temperatureDeci - (int16_t)ENERGY_COMFORT_DECI);

    if (s2t_excess <= 0)
    {
        u1g_heatPercent = 0u;
    }
    else
    {
        u2t_percent = (uint16_t)((s2t_excess / ENERGY_DECI_PER_DEGREE)
                    * (int16_t)ENERGY_HEAT_PER_DEGREE);
        if (u2t_percent > (uint16_t)ENERGY_HEAT_MAX_PERCENT)
        {
            u2t_percent = (uint16_t)ENERGY_HEAT_MAX_PERCENT;
        }
        u1g_heatPercent = (uint8_t)u2t_percent;
    }
}

/*********************************************************************
 * @fn                - u2_energyGetKcalX100
 * @brief             - Return the energy of the set in hundredths of a
 *                      kilocalorie, temperature correction included.
 * @return            - uint16_t for example 255 means 2.55 kcal
 *//////////////////////////////////////////////////////////////////////
uint16_t u2_energyGetKcalX100(void)
{
    uint32_t u4t_kcalX100 = 0uL;

    u4t_kcalX100 = u4g_workMilliJoule / ENERGY_MJ_PER_KCAL_X100;
    u4t_kcalX100 = (u4t_kcalX100
                 * ((uint32_t)ENERGY_PERCENT_FULL + (uint32_t)u1g_heatPercent))
                 / (uint32_t)ENERGY_PERCENT_FULL;

    return (uint16_t)u4t_kcalX100;
}

/*********************************************************************
 * @fn                - u1_energyGetHeatPercent
 * @brief             - Return how many percent the heat adds to the cost.
 * @return            - uint8_t 0 to 15
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_energyGetHeatPercent(void)
{
    return u1g_heatPercent;
}
