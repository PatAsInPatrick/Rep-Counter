/*******************************************************************************
 * File Name    : app_overheat.c
 * Description  : Holds the hook that the analog watchdog interrupt calls.
 *
 *                Everything that protects the user is done inside the
 *                handler: the four LEDs are switched on, the digit is
 *                blanked and the repetition counter is stopped. Only the
 *                screen, which needs about ninety milliseconds, is left for
 *                the main loop to draw afterwards.
 *
 *                The alarm clears itself only after the room has cooled
 *                below a lower limit, so a reading that sits exactly on the
 *                limit cannot make the alarm rattle on and off.
 * Date         : 2026-09-08
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "app_overheat.h"
#include "drv_adc.h"
#include "drv_led.h"
#include "drv_seg7.h"
#include "app_repcount.h"

/* Private define ------------------------------------------------------------*/
#define OVERHEAT_LED_ALL            (0xFu)
#define OVERHEAT_COUNT_MAX          (255u)

/* Private variables ---------------------------------------------------------*/
static volatile uint8_t u1g_active = 0u;
static volatile uint8_t u1g_newEvent = 0u;
static volatile uint8_t u1g_eventCount = 0u;

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_overheatInit
 * @brief             - Clear the alarm and arm the hardware watchdog.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_overheatInit(void)
{
    u1g_active = 0u;
    u1g_newEvent = 0u;
    u1g_eventCount = 0u;

    v_adcWatchdogInit((uint16_t)OVERHEAT_TRIP_COUNT);
}

/*********************************************************************
 * @fn                - v_adcWatchdogHook
 * @brief             - Called from ADC_IRQHandler. This runs at the highest
 *                      interrupt priority and may cut into any other
 *                      handler, so it only touches registers and flags and
 *                      never waits for anything.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_adcWatchdogHook(void)
{
    if (u1g_active == 0u)
    {
        u1g_active = 1u;
        u1g_newEvent = 1u;

        if (u1g_eventCount < (uint8_t)OVERHEAT_COUNT_MAX)
        {
            u1g_eventCount++;
        }

        /* stop the training immediately */
        v_repSetBlocked(1u);

        /* warn with what can be driven in a few microseconds */
        v_ledWriteAll((uint8_t)OVERHEAT_LED_ALL);
        v_seg7WriteDigit((uint8_t)SEG7_BLANK);
    }
}

/*********************************************************************
 * @fn                - u1_overheatIsActive
 * @brief             - Tell whether the emergency stop is still in force.
 * @return            - uint8_t 1 = too hot
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_overheatIsActive(void)
{
    return u1g_active;
}

/*********************************************************************
 * @fn                - u1_overheatHasNewEvent
 * @brief             - Tell the main loop that the screen has to change.
 * @return            - uint8_t 1 = something new happened
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_overheatHasNewEvent(void)
{
    return u1g_newEvent;
}

/*********************************************************************
 * @fn                - v_overheatClearNewEvent
 * @brief             - Acknowledge the event once the screen is drawn.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_overheatClearNewEvent(void)
{
    u1g_newEvent = 0u;
}

/*********************************************************************
 * @fn                - u1_overheatGetCount
 * @brief             - How many times the alarm has fired since start-up.
 * @return            - uint8_t number of events
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_overheatGetCount(void)
{
    return u1g_eventCount;
}

/*********************************************************************
 * @fn                - v_overheatUpdate
 * @brief             - Called from the main loop with every new reading.
 *                      When the room has cooled past the lower limit the
 *                      alarm is released and the hardware watchdog is armed
 *                      again, ready for the next time.
 * @param[in]         - u2_rawCount : latest thermistor count
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_overheatUpdate(uint16_t u2_rawCount)
{
    if ((u1g_active == 1u) && (u2_rawCount > (uint16_t)OVERHEAT_REARM_COUNT))
    {
        u1g_active = 0u;
        u1g_newEvent = 1u;
        v_repSetBlocked(0u);
        v_repAbortCurrent();
        v_adcWatchdogArm();
    }
}
