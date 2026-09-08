/*******************************************************************************
 * File Name    : main.c
 * Description  : Smart Rep Counter - demonstration version.
 *                The potentiometer of the training shield plays the role of
 *                the arm angle, so the whole algorithm can be developed and
 *                shown with the board alone. When the MPU6050 arrives, only
 *                the line marked SENSOR SOURCE has to change.
 * Date         : 2026-09-01
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
    v_reportInit();
    v_displayInit();
    v_displayShowIdle();
    v_reportBanner();
}

/*********************************************************************
 * @fn                - u2_mainReadAngle
 * @brief             - SENSOR SOURCE. Today the angle comes from the
 *                      potentiometer. Replace the body of this function by a
 *                      call to the MPU6050 driver to use the real sensor.
 * @return            - uint16_t angle in degrees, 0..180
 *//////////////////////////////////////////////////////////////////////
static uint16_t u2_mainReadAngle(void)
{
    uint32_t u4t_raw = (uint32_t)u2_adcGetFiltered();
    uint32_t u4t_angle = (u4t_raw * MAIN_ANGLE_MAX_DEG) / (uint32_t)ADC_MAX_COUNT;

    return (uint16_t)u4t_angle;
}

/*********************************************************************
 * @fn                - v_mainHandleButton
 * @brief             - Act on the four switches of the training shield.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_mainHandleButton(void)
{
    uint8_t u1t_events = u1_extiGetEvents();

    if ((u1t_events & (uint8_t)EXTI_EVENT_START_STOP) != 0u)
    {
        v_extiClearEvent((uint8_t)EXTI_EVENT_START_STOP);

        if (u1_repIsSetActive() == 0u)
        {
            v_repStartSet();
            v_reportSetStarted();
            v_displayShowRunning();
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
        v_displayShowOverview();
    }

    if ((u1t_events & (uint8_t)EXTI_EVENT_NEXT_PAGE) != 0u)
    {
        v_extiClearEvent((uint8_t)EXTI_EVENT_NEXT_PAGE);
        v_displayNextPage();
    }

    if ((u1t_events & (uint8_t)EXTI_EVENT_CLEAR_FATIGUE) != 0u)
    {
        v_extiClearEvent((uint8_t)EXTI_EVENT_CLEAR_FATIGUE);
        v_repClearFatigue();
    }
}

/*********************************************************************
 * @fn                - v_mainHandleSample
 * @brief             - Feed a new angle sample into the algorithm.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_mainHandleSample(void)
{
    uint16_t u2t_angle = 0u;

    if (u1_adcIsSampleReady() == 1u)
    {
        v_adcClearSampleReady();
        u2t_angle = u2_mainReadAngle();
        v_repProcessSample(u2t_angle, u4_timGetTickMs());
        v_displaySetAngle(u2t_angle);
    }
}

/*********************************************************************
 * @fn                - v_mainHandleResult
 * @brief             - Report a repetition as soon as one is finished.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_mainHandleResult(void)
{
    const RepResult_t * pst_result = pst_repGetResult();

    if (u1_repIsResultReady() == 1u)
    {
        v_repClearResultReady();
        v_reportRepetition(pst_result);
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
        v_reportPump();

        u1g_setActive = u1_repIsSetActive();
        v_displayTask(u4_timGetTickMs(), u1g_setActive, pst_repGetResult());
    }
}
