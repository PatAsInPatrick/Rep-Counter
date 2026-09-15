/*******************************************************************************
 * File Name    : app_thermal.c
 * Description  : The thermistor is read through the same ADC sequence as the
 *                angle. A lookup table with linear interpolation is used
 *                instead of a logarithm, so no floating point is needed.
 *
 *                Wiring assumed on the shield:
 *                3.3V --[ 10k ]-- PA0 --[ NTC 10k ]-- GND
 *                A higher count therefore means a colder sensor.
 * Date         : 2026-09-08
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "app_thermal.h"

/* Private define ------------------------------------------------------------*/
#define THERMAL_TABLE_SIZE          (12u)
#define THERMAL_STEP_DECI           (50)    /* 5.0 degC between two entries   */
#define THERMAL_FIRST_DECI          (0)     /* first entry is 0.0 degC        */
#define THERMAL_FILTER_SHIFT        (3u)    /* smooth over eight readings     */
#define THERMAL_LAST_INDEX          (THERMAL_TABLE_SIZE - 1u)

/* Private variables ---------------------------------------------------------*/
/* ADC count measured at 0, 5, 10 ... 55 degC. The list always goes down. */
static const uint16_t u2g_thermalTable[THERMAL_TABLE_SIZE] =
{
    3156u, 2955u, 2738u, 2510u, 2278u, 2048u,
    1825u, 1614u, 1419u, 1241u, 1081u, 940u
};

static int16_t  s2g_temperatureDeci = 250;  /* start at a sensible 25.0 degC  */
static uint32_t u4g_filterSum = 0uL;
static uint8_t  u1g_filterReady = 0u;

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_thermalInit
 * @brief             - Bring the module to a known state.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_thermalInit(void)
{
    s2g_temperatureDeci = 250;
    u4g_filterSum = 0uL;
    u1g_filterReady = 0u;
}

/*********************************************************************
 * @fn                - v_thermalUpdate
 * @brief             - Feed one raw reading, smooth it, then convert it
 *                      into a temperature by walking the lookup table.
 * @param[in]         - u2_rawCount : ADC count of the thermistor channel
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_thermalUpdate(uint16_t u2_rawCount)
{
    uint16_t u2t_value = 0u;
    uint8_t  u1t_index = 0u;
    uint16_t u2t_high = 0u;
    uint16_t u2t_low = 0u;
    int32_t  s4t_temp = 0;

    if (u1g_filterReady == 0u)
    {
        u4g_filterSum = (uint32_t)u2_rawCount << THERMAL_FILTER_SHIFT;
        u1g_filterReady = 1u;
    }
    else
    {
        u4g_filterSum -= (u4g_filterSum >> THERMAL_FILTER_SHIFT);
        u4g_filterSum += (uint32_t)u2_rawCount;
    }

    u2t_value = (uint16_t)(u4g_filterSum >> THERMAL_FILTER_SHIFT);

    if (u2t_value >= u2g_thermalTable[0])
    {
        s2g_temperatureDeci = (int16_t)THERMAL_FIRST_DECI;
    }
    else if (u2t_value <= u2g_thermalTable[THERMAL_LAST_INDEX])
    {
        s2g_temperatureDeci = (int16_t)(THERMAL_FIRST_DECI
                            + ((int16_t)THERMAL_LAST_INDEX * THERMAL_STEP_DECI));
    }
    else
    {
        for (u1t_index = 0u; u1t_index < (uint8_t)THERMAL_LAST_INDEX; u1t_index++)
        {
            u2t_high = u2g_thermalTable[u1t_index];
            u2t_low = u2g_thermalTable[u1t_index + 1u];

            if ((u2t_value <= u2t_high) && (u2t_value > u2t_low))
            {
                s4t_temp = (int32_t)THERMAL_FIRST_DECI
                         + ((int32_t)u1t_index * (int32_t)THERMAL_STEP_DECI);
                s4t_temp += (((int32_t)u2t_high - (int32_t)u2t_value)
                          * (int32_t)THERMAL_STEP_DECI)
                          / ((int32_t)u2t_high - (int32_t)u2t_low);
                s2g_temperatureDeci = (int16_t)s4t_temp;
                u1t_index = (uint8_t)THERMAL_LAST_INDEX;
            }
        }
    }
}

/*********************************************************************
 * @fn                - s2_thermalGetDeci
 * @brief             - Return the temperature in tenths of a degree.
 * @return            - int16_t for example 253 means 25.3 degC
 *//////////////////////////////////////////////////////////////////////
int16_t s2_thermalGetDeci(void)
{
    return s2g_temperatureDeci;
}

/*********************************************************************
 * @fn                - u1_thermalIsHot
 * @brief             - Tell whether the room is warm enough to be a risk.
 * @return            - uint8_t 1 = too hot, 0 = acceptable
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_thermalIsHot(void)
{
    uint8_t u1t_hot = 0u;

    if (s2g_temperatureDeci >= (int16_t)THERMAL_HOT_LIMIT_DECI)
    {
        u1t_hot = 1u;
    }

    return u1t_hot;
}
