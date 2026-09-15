/*******************************************************************************
 * File Name    : main.c
 * Description  : Smart Rep Counter.
 *                The potentiometer on PA4 carries the arm angle and the
 *                thermistor on PA0 carries the room temperature. Both are
 *                converted in the same ADC sequence.
 * Date         : 2026-09-08
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Private includes ----------------------------------------------------------*/
#include "drv_gpio.h"
#include "drv_tim.h"
#include "drv_adc.h"
#include "drv_uart.h"
#include "drv_exti.h"
#include "app_repcount.h"
#include "app_report.h"
#include "app_display.h"
#include "app_thermal.h"
#include "app_energy.h"
#include "app_safety.h"
#include "app_overheat.h"

/* Private define ------------------------------------------------------------*/
#define MAIN_ANGLE_MAX_DEG          (180uL)

/* Private variables ---------------------------------------------------------*/
static uint8_t u1g_setActive = 0u;

/* Private function prototypes -----------------------------------------------*/
static void v_mainInitAll(void);
static uint16_t u2_mainReadAngle(void);
static void v_mainHandleButton(void);
static void v_mainHandleSample(void);
static void v_mainHandleResult(void);
static void v_mainHandleOverheat(void);

/* Private user code ---------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_mainInitAll
 * @brief             - Initialise every driver and every application module.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_mainInitAll(void)
{
    v_gpioAnalogInit();
    v_gpioUartPinInit();

    v_timTickInit();
    v_timTriggerInit();
    v_uartInit();
    v_adcInit();
    v_extiButtonInit();

    v_repInit();
    v_thermalInit();
    v_energyInit();
    v_safetyInit();
    v_overheatInit();
    v_reportInit();
    v_displayInit();
    v_displayShowIdle();
    v_reportBanner();
}

/*********************************************************************
 * @fn                - u2_mainReadAngle
 * @brief             - SENSOR SOURCE. Convert the potentiometer reading
 *                      into an arm angle. Replace the body of this function
 *                      to use a different sensor.
 * @return            - uint16_t angle in degrees, 0..180
 *//////////////////////////////////////////////////////////////////////
static uint16_t u2_mainReadAngle(void)
{
    uint32_t u4t_raw = (uint32_t)u2_adcGetAngleRaw();
    uint32_t u4t_angle = (u4t_raw * MAIN_ANGLE_MAX_DEG) / (uint32_t)ADC_MAX_COUNT;

    return (uint16_t)u4t_angle;
}

/*********************************************************************
 * @fn                - v_mainHandleButton
 * @brief             - Act on the four switches of the training shield.
 *                      What a button does depends on the current state.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_mainHandleButton(void)
{
    uint8_t u1t_events = u1_extiGetEvents();
    uint32_t u4t_now = u4_timGetTickMs();

    if ((u1t_events & (uint8_t)EXTI_EVENT_START_STOP) != 0u)
    {
        v_extiClearEvent((uint8_t)EXTI_EVENT_START_STOP);

        if (u1_repIsSetActive() == 0u)
        {
            v_repStartSet();
            v_safetyInit();
            v_energyResetSet();
            v_repSetBlocked(0u);
            v_reportSetStarted();
            v_displayShowRunning(pst_repGetResult());
        }
        else
        {
            v_reportSetSummary(pst_repGetResult());
            v_repStopSet();
            v_displayShowSummary(pst_repGetResult());
        }
    }

    if ((u1t_events & (uint8_t)EXTI_EVENT_OVERVIEW) != 0u)
    {
        v_extiClearEvent((uint8_t)EXTI_EVENT_OVERVIEW);

        if (u1_repIsSetActive() == 1u)
        {
            /* the screen belongs to the live view while lifting */
        }
        else if (u1_displayIsResultView() == 1u)
        {
            v_displayBack();
        }
        else
        {
            v_energyWeightDown();
            v_displayShowIdle();
        }
    }

    if ((u1t_events & (uint8_t)EXTI_EVENT_NEXT_PAGE) != 0u)
    {
        v_extiClearEvent((uint8_t)EXTI_EVENT_NEXT_PAGE);

        if (u1_repIsSetActive() == 1u)
        {
            /* the screen belongs to the live view while lifting */
        }
        else if (u1_displayIsResultView() == 1u)
        {
            v_displayNextPage();
        }
        else
        {
            v_energyWeightUp();
            v_displayShowIdle();
        }
    }

    if ((u1t_events & (uint8_t)EXTI_EVENT_CLEAR_FATIGUE) != 0u)
    {
        v_extiClearEvent((uint8_t)EXTI_EVENT_CLEAR_FATIGUE);

        if (u1_repIsSetActive() == 0u)
        {
            /* outside a set this button is the shortcut back to the setup */
            v_displayShowIdle();
        }
        else if (u1_safetyAcknowledge(u4t_now) == 1u)
        {
            v_repClearFatigue();
            v_repSetBlocked(0u);
            /* drop the movement that was in progress during the lock, so a
             * repetition started while resting is never counted            */
            v_repAbortCurrent();
            v_reportSafetyCleared();
            v_displayShowRunning(pst_repGetResult());
        }
        else
        {
            /* still resting, the request is refused on purpose */
        }
    }
}

/*********************************************************************
 * @fn                - v_mainHandleSample
 * @brief             - Feed a new angle sample into the algorithm and a new
 *                      temperature reading into the thermal module.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_mainHandleSample(void)
{
    uint16_t u2t_angle = 0u;

    if (u1_adcIsSampleReady() == 1u)
    {
        v_adcClearSampleReady();

        v_thermalUpdate(u2_adcGetThermalRaw());
        v_energySetTemperature(s2_thermalGetDeci());
        v_overheatUpdate(u2_adcGetThermalRaw());

        u2t_angle = u2_mainReadAngle();
        v_repProcessSample(u2t_angle, u4_timGetTickMs());
        v_displaySetAngle(u2t_angle);
    }
}

/*********************************************************************
 * @fn                - v_mainHandleResult
 * @brief             - Handle a repetition that has just finished. A refused
 *                      repetition is reported but never counted.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_mainHandleResult(void)
{
    const RepResult_t * pst_result = pst_repGetResult();

    if (u1_repIsResultReady() == 1u)
    {
        v_repClearResultReady();

        if (pst_result->u1_blockedFlag == 1u)
        {
            uint8_t u1t_wasWarn = 0u;

            if (st_safetyGetState() == SAFETY_STATE_WARN)
            {
                u1t_wasWarn = 1u;
            }

            v_safetyNotifyViolation(u4_timGetTickMs());
            v_reportBlocked(u1_safetyGetBlockedCount());

            if (u1t_wasWarn == 1u)
            {
                v_reportSafetyLocked();
            }
        }
        else
        {
            v_reportRepetition(pst_result);

            if (pst_result->u1_validFlag == 1u)
            {
                v_energyAddRepetition(pst_result->u2_romDeg);
                v_safetyNotifyRepetition(pst_result->u1_fatigueFlag,
                                         u4_timGetTickMs());

                if (st_safetyGetState() == SAFETY_STATE_WARN)
                {
                    /* counting stops until the user rests or presses D5,
                     * so a repetition done while tired is never counted   */
                    v_repSetBlocked(1u);
                }
            }

            if (u1_safetyIsLocked() == 1u)
            {
                /* the locked screen is drawn by the display task */
            }
            else if (u1_repIsTargetReached() == 1u)
            {
                v_reportSetSummary(pst_repGetResult());
                v_repStopSet();
                v_displayShowSummary(pst_repGetResult());
            }
            else
            {
                v_displayShowLive(pst_result);
            }
        }
    }
}

/*********************************************************************
 * @fn                - v_mainHandleOverheat
 * @brief             - Send one line to the terminal whenever the hardware
 *                      watchdog changes the state of the training.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_mainHandleOverheat(void)
{
    static uint8_t u1s_lastActive = 0u;
    uint8_t u1t_active = u1_overheatIsActive();

    if (u1t_active != u1s_lastActive)
    {
        u1s_lastActive = u1t_active;
        v_reportOverheat(u1t_active, s2_thermalGetDeci());
    }
}

/* Main function -------------------------------------------------------------*/
int main(void)
{
    /* Initialization */
    v_mainInitAll();

    while (1)
    {
        /* Main application */
        v_mainHandleButton();
        v_mainHandleSample();
        v_mainHandleResult();
        v_mainHandleOverheat();
        v_reportPump();

        u1g_setActive = u1_repIsSetActive();
        v_displayTask(u4_timGetTickMs(), u1g_setActive, pst_repGetResult());
    }
}
