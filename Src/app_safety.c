/*******************************************************************************
 * File Name    : app_safety.c
 * Description  : Three step supervisor.
 *
 *                SAFE   nothing wrong.
 *                WARN   a repetition came back slower than the fatigue limit.
 *                       Counting stops right away. The user must rest or
 *                       press the clear button to carry on.
 *                LOCKED the user lifted again instead of resting. That
 *                       repetition is refused, the screen shows a warning,
 *                       and the clear button is ignored until the forced
 *                       rest time has passed. Lifting during the rest
 *                       starts the countdown again from the beginning.
 *
 *                The time base is the millisecond tick of TIM3, which is
 *                produced by an interrupt, so the countdown keeps running
 *                even while the screen is being written.
 * Date         : 2026-09-08
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "app_safety.h"

/* Private define ------------------------------------------------------------*/
#define SAFETY_MS_PER_SECOND        (1000uL)
#define SAFETY_REST_MS              ((uint32_t)SAFETY_REST_SECONDS \
                                     * SAFETY_MS_PER_SECOND)
#define SAFETY_BLOCKED_MAX          (255u)

/* Private variables ---------------------------------------------------------*/
static SafetyState_t stg_state = SAFETY_STATE_SAFE;
static uint32_t u4g_lockStartMs = 0uL;
static uint8_t  u1g_blockedCount = 0u;

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_safetyInit
 * @brief             - Bring the supervisor to a known state.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_safetyInit(void)
{
    stg_state = SAFETY_STATE_SAFE;
    u4g_lockStartMs = 0uL;
    u1g_blockedCount = 0u;
}

/*********************************************************************
 * @fn                - v_safetyNotifyRepetition
 * @brief             - Called once for every repetition that was counted.
 * @param[in]         - u1_fatigueFlag : 1 = that repetition was too slow
 * @param[in]         - u4_timeMs      : current millisecond counter
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_safetyNotifyRepetition(uint8_t u1_fatigueFlag, uint32_t u4_timeMs)
{
    (void)u4_timeMs;

    if ((stg_state == SAFETY_STATE_SAFE) && (u1_fatigueFlag == 1u))
    {
        stg_state = SAFETY_STATE_WARN;
    }
    else
    {
        /* the other states are changed by a violation, not by a good rep */
    }
}

/*********************************************************************
 * @fn                - v_safetyNotifyViolation
 * @brief             - Called for a repetition that was refused. From the
 *                      warning state it locks the counter, and while already
 *                      locked it starts the rest countdown again, so the
 *                      user really has to stay still.
 * @param[in]         - u4_timeMs : current millisecond counter
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_safetyNotifyViolation(uint32_t u4_timeMs)
{
    if (stg_state == SAFETY_STATE_WARN)
    {
        stg_state = SAFETY_STATE_LOCKED;
    }

    if (stg_state == SAFETY_STATE_LOCKED)
    {
        u4g_lockStartMs = u4_timeMs;

        if (u1g_blockedCount < (uint8_t)SAFETY_BLOCKED_MAX)
        {
            u1g_blockedCount++;
        }
    }
}

/*********************************************************************
 * @fn                - u1_safetyAcknowledge
 * @brief             - The user pressed the clear button. In the warning
 *                      state it is accepted at once. In the locked state it
 *                      is refused until the forced rest time has passed.
 * @param[in]         - u4_timeMs : current millisecond counter
 * @return            - uint8_t 1 = accepted, 0 = refused
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_safetyAcknowledge(uint32_t u4_timeMs)
{
    uint8_t u1t_accepted = 0u;

    if (stg_state == SAFETY_STATE_WARN)
    {
        stg_state = SAFETY_STATE_SAFE;
        u1t_accepted = 1u;
    }
    else if (stg_state == SAFETY_STATE_LOCKED)
    {
        if ((u4_timeMs - u4g_lockStartMs) >= SAFETY_REST_MS)
        {
            stg_state = SAFETY_STATE_SAFE;
            u1g_blockedCount = 0u;
            u1t_accepted = 1u;
        }
        else
        {
            /* still resting, the button is ignored on purpose */
        }
    }
    else
    {
        /* already safe, nothing to acknowledge */
    }

    return u1t_accepted;
}

/*********************************************************************
 * @fn                - st_safetyGetState
 * @brief             - Return the current supervisor state.
 * @return            - SafetyState_t current state
 *//////////////////////////////////////////////////////////////////////
SafetyState_t st_safetyGetState(void)
{
    return stg_state;
}

/*********************************************************************
 * @fn                - u1_safetyIsLocked
 * @brief             - Tell whether counting must be suspended.
 * @return            - uint8_t 1 = locked, 0 = counting allowed
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_safetyIsLocked(void)
{
    uint8_t u1t_locked = 0u;

    if (stg_state == SAFETY_STATE_LOCKED)
    {
        u1t_locked = 1u;
    }

    return u1t_locked;
}

/*********************************************************************
 * @fn                - u2_safetyRestRemainSec
 * @brief             - Seconds left before the clear button is accepted.
 * @param[in]         - u4_timeMs : current millisecond counter
 * @return            - uint16_t remaining seconds, zero when ready
 *//////////////////////////////////////////////////////////////////////
uint16_t u2_safetyRestRemainSec(uint32_t u4_timeMs)
{
    uint32_t u4t_elapsed = 0uL;
    uint16_t u2t_remain = 0u;

    if (stg_state == SAFETY_STATE_LOCKED)
    {
        u4t_elapsed = u4_timeMs - u4g_lockStartMs;
        if (u4t_elapsed < SAFETY_REST_MS)
        {
            u2t_remain = (uint16_t)((SAFETY_REST_MS - u4t_elapsed)
                       / SAFETY_MS_PER_SECOND);
            u2t_remain = (uint16_t)(u2t_remain + 1u);
        }
    }

    return u2t_remain;
}

/*********************************************************************
 * @fn                - u1_safetyGetBlockedCount
 * @brief             - Return how many repetitions were refused.
 * @return            - uint8_t number of refused repetitions
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_safetyGetBlockedCount(void)
{
    return u1g_blockedCount;
}
