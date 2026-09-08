/*******************************************************************************
 * File Name    : app_repcount.c
 * Description  : State machine that turns a stream of arm angles into
 *                repetitions, range of motion, tempo, speed and fatigue.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "app_repcount.h"

/* Private define ------------------------------------------------------------*/
#define REP_ANGLE_DOWN_DEG          (40u)   /* below this the arm is down     */
#define REP_ANGLE_UP_DEG            (130u)  /* above this the arm is up       */
#define REP_MIN_ROM_DEG             (80u)   /* smaller than this = partial    */
#define REP_MIN_ATTEMPT_DEG         (30u)   /* smaller than this = only noise */
#define REP_MIN_PHASE_MS            (150uL) /* faster than this = shaking     */
#define REP_FATIGUE_PERCENT         (80uL)  /* speed below 80 % = tired       */
#define REP_PERCENT_FULL            (100uL)
#define REP_MS_PER_SECOND           (1000uL)
#define REP_ANGLE_MAX_DEG           (180u)
#define REP_COUNT_MAX               (255u)
#define REP_VALUE_MAX               (65535u)

/* Private variables ---------------------------------------------------------*/
static RepState_t  stg_state = REP_STATE_IDLE;
static RepResult_t stg_result;
static RepStats_t  stg_stats;
static uint8_t     u1g_resultReady = 0u;

static uint16_t u2g_angleMin = REP_ANGLE_MAX_DEG;
static uint16_t u2g_angleMax = 0u;
static uint32_t u4g_timeLiftStart = 0uL;
static uint32_t u4g_timeTopReached = 0uL;
static uint16_t u2g_velocityFirst = 0u;

/* Private function prototypes -----------------------------------------------*/
static void v_repResetRepetition(uint16_t u2_angleDeg);
static void v_repUpdateStats(void);
static void v_repFinishRepetition(uint32_t u4_timeMs, uint8_t u1_reachedTop);

/* Private functions ---------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_repResetRepetition
 * @brief             - Clear the measurements of one single repetition.
 * @param[in]         - u2_angleDeg : current angle used as first sample
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_repResetRepetition(uint16_t u2_angleDeg)
{
    u2g_angleMin = u2_angleDeg;
    u2g_angleMax = u2_angleDeg;
    u4g_timeLiftStart = 0uL;
    u4g_timeTopReached = 0uL;
}

/*********************************************************************
 * @fn                - v_repUpdateStats
 * @brief             - Fold the repetition that just ended into the
 *                      minimum, maximum and running total of the set.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_repUpdateStats(void)
{
    if (stg_stats.u1_validCount < (uint8_t)REP_COUNT_MAX)
    {
        stg_stats.u1_validCount++;
    }

    if (stg_result.u2_romDeg < stg_stats.u2_romMin)
    {
        stg_stats.u2_romMin = stg_result.u2_romDeg;
    }
    if (stg_result.u2_romDeg > stg_stats.u2_romMax)
    {
        stg_stats.u2_romMax = stg_result.u2_romDeg;
    }
    stg_stats.u4_romSum += (uint32_t)stg_result.u2_romDeg;

    if (stg_result.u2_concentricMs < stg_stats.u2_upMin)
    {
        stg_stats.u2_upMin = stg_result.u2_concentricMs;
    }
    if (stg_result.u2_concentricMs > stg_stats.u2_upMax)
    {
        stg_stats.u2_upMax = stg_result.u2_concentricMs;
    }
    stg_stats.u4_upSum += (uint32_t)stg_result.u2_concentricMs;

    if (stg_result.u2_eccentricMs < stg_stats.u2_downMin)
    {
        stg_stats.u2_downMin = stg_result.u2_eccentricMs;
    }
    if (stg_result.u2_eccentricMs > stg_stats.u2_downMax)
    {
        stg_stats.u2_downMax = stg_result.u2_eccentricMs;
    }
    stg_stats.u4_downSum += (uint32_t)stg_result.u2_eccentricMs;

    if (stg_result.u2_velocityDps < stg_stats.u2_speedMin)
    {
        stg_stats.u2_speedMin = stg_result.u2_velocityDps;
    }
    if (stg_result.u2_velocityDps > stg_stats.u2_speedMax)
    {
        stg_stats.u2_speedMax = stg_result.u2_velocityDps;
    }
    stg_stats.u4_speedSum += (uint32_t)stg_result.u2_velocityDps;

    if (stg_result.u1_dropPercent > stg_stats.u1_dropMax)
    {
        stg_stats.u1_dropMax = stg_result.u1_dropPercent;
    }

    if (stg_result.u1_fatigueFlag == 1u)
    {
        if (stg_stats.u1_fatigueCount < (uint8_t)REP_COUNT_MAX)
        {
            stg_stats.u1_fatigueCount++;
        }
    }
}

/*********************************************************************
 * @fn                - v_repFinishRepetition
 * @brief             - Compute range of motion, tempo, speed and fatigue,
 *                      then publish the result to the application.
 * @param[in]         - u4_timeMs      : time when the arm came back down
 * @param[in]         - u1_reachedTop  : 1 = the top position was reached
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_repFinishRepetition(uint32_t u4_timeMs, uint8_t u1_reachedTop)
{
    uint32_t u4t_concentric = 0uL;
    uint32_t u4t_eccentric = 0uL;
    uint32_t u4t_rom = 0uL;
    uint32_t u4t_velocity = 0uL;
    uint32_t u4t_limit = 0uL;
    uint32_t u4t_drop = 0uL;

    if (u1_reachedTop == 1u)
    {
        u4t_concentric = u4g_timeTopReached - u4g_timeLiftStart;
        u4t_eccentric = u4_timeMs - u4g_timeTopReached;
    }
    else
    {
        u4t_concentric = u4_timeMs - u4g_timeLiftStart;
        u4t_eccentric = 0uL;
    }
    u4t_rom = (uint32_t)u2g_angleMax - (uint32_t)u2g_angleMin;

    stg_result.u2_romDeg = (uint16_t)u4t_rom;
    stg_result.u2_concentricMs = (uint16_t)u4t_concentric;
    stg_result.u2_eccentricMs = (uint16_t)u4t_eccentric;
    stg_result.u1_fatigueFlag = 0u;

    if (u4t_concentric >= REP_MIN_PHASE_MS)
    {
        u4t_velocity = (u4t_rom * REP_MS_PER_SECOND) / u4t_concentric;
    }
    else
    {
        u4t_velocity = 0uL;
    }
    stg_result.u2_velocityDps = (uint16_t)u4t_velocity;

    if ((u4t_rom >= (uint32_t)REP_MIN_ROM_DEG) && (u1_reachedTop == 1u))
    {
        stg_result.u1_validFlag = 1u;
        if (stg_result.u1_repCount < (uint8_t)REP_COUNT_MAX)
        {
            stg_result.u1_repCount++;
        }

        if (u2g_velocityFirst == 0u)
        {
            u2g_velocityFirst = (uint16_t)u4t_velocity;
            stg_result.u1_dropPercent = 0u;
        }
        else
        {
            u4t_limit = ((uint32_t)u2g_velocityFirst * REP_FATIGUE_PERCENT)
                      / REP_PERCENT_FULL;
            if (u4t_velocity < u4t_limit)
            {
                stg_result.u1_fatigueFlag = 1u;
            }

            if (u4t_velocity >= (uint32_t)u2g_velocityFirst)
            {
                u4t_drop = 0uL;
            }
            else
            {
                u4t_drop = (((uint32_t)u2g_velocityFirst - u4t_velocity)
                         * REP_PERCENT_FULL) / (uint32_t)u2g_velocityFirst;
            }
            stg_result.u1_dropPercent = (uint8_t)u4t_drop;
        }
    }
    else
    {
        stg_result.u1_validFlag = 0u;
        if (stg_result.u1_partialCount < (uint8_t)REP_COUNT_MAX)
        {
            stg_result.u1_partialCount++;
        }
    }

    if (stg_result.u1_validFlag == 1u)
    {
        v_repUpdateStats();
    }

    u1g_resultReady = 1u;
}

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_repInit
 * @brief             - Bring the algorithm to a known state.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_repInit(void)
{
    stg_state = REP_STATE_IDLE;
    stg_result.u1_repCount = 0u;
    stg_result.u1_partialCount = 0u;
    stg_result.u2_romDeg = 0u;
    stg_result.u2_concentricMs = 0u;
    stg_result.u2_eccentricMs = 0u;
    stg_result.u2_velocityDps = 0u;
    stg_result.u1_fatigueFlag = 0u;
    stg_result.u1_validFlag = 0u;
    stg_result.u1_dropPercent = 0u;
    u1g_resultReady = 0u;
    u2g_velocityFirst = 0u;

    stg_stats.u1_validCount = 0u;
    stg_stats.u1_fatigueCount = 0u;
    stg_stats.u1_dropMax = 0u;
    stg_stats.u2_romMin = (uint16_t)REP_VALUE_MAX;
    stg_stats.u2_romMax = 0u;
    stg_stats.u4_romSum = 0uL;
    stg_stats.u2_upMin = (uint16_t)REP_VALUE_MAX;
    stg_stats.u2_upMax = 0u;
    stg_stats.u4_upSum = 0uL;
    stg_stats.u2_downMin = (uint16_t)REP_VALUE_MAX;
    stg_stats.u2_downMax = 0u;
    stg_stats.u4_downSum = 0uL;
    stg_stats.u2_speedMin = (uint16_t)REP_VALUE_MAX;
    stg_stats.u2_speedMax = 0u;
    stg_stats.u4_speedSum = 0uL;

    v_repResetRepetition(0u);

    /* start with inverted extremes so that the very first sample sets both */
    u2g_angleMin = (uint16_t)REP_ANGLE_MAX_DEG;
    u2g_angleMax = 0u;
}

/*********************************************************************
 * @fn                - v_repStartSet
 * @brief             - Start a new training set.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_repStartSet(void)
{
    v_repInit();
    stg_state = REP_STATE_READY;
}

/*********************************************************************
 * @fn                - v_repStopSet
 * @brief             - Stop the current training set.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_repStopSet(void)
{
    stg_state = REP_STATE_IDLE;
}

/*********************************************************************
 * @fn                - v_repClearFatigue
 * @brief             - Take the fatigue reference from the current
 *                      repetition. Used after the athlete has rested.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_repClearFatigue(void)
{
    u2g_velocityFirst = 0u;
    stg_result.u1_fatigueFlag = 0u;
    stg_result.u1_dropPercent = 0u;
}

/*********************************************************************
 * @fn                - u1_repIsSetActive
 * @brief             - Tell whether a set is running.
 * @return            - uint8_t 1 = running, 0 = idle
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_repIsSetActive(void)
{
    uint8_t u1t_active = 1u;

    if (stg_state == REP_STATE_IDLE)
    {
        u1t_active = 0u;
    }

    return u1t_active;
}

/*********************************************************************
 * @fn                - st_repGetState
 * @brief             - Return the current state of the machine.
 * @return            - RepState_t current state
 *//////////////////////////////////////////////////////////////////////
RepState_t st_repGetState(void)
{
    return stg_state;
}

/*********************************************************************
 * @fn                - v_repProcessSample
 * @brief             - Feed one angle sample into the state machine.
 * @param[in]         - u2_angleDeg : arm angle 0..180 degrees
 * @param[in]         - u4_timeMs   : time stamp of this sample
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_repProcessSample(uint16_t u2_angleDeg, uint32_t u4_timeMs)
{
    if (u2_angleDeg < u2g_angleMin)
    {
        u2g_angleMin = u2_angleDeg;
    }
    if (u2_angleDeg > u2g_angleMax)
    {
        u2g_angleMax = u2_angleDeg;
    }

    switch (stg_state)
    {
        case REP_STATE_READY:
            if (u2_angleDeg >= (uint16_t)REP_ANGLE_DOWN_DEG)
            {
                u4g_timeLiftStart = u4_timeMs;
                stg_state = REP_STATE_CONCENTRIC;
            }
            else
            {
                /* arm still resting down, the minimum angle keeps updating */
            }
            break;

        case REP_STATE_CONCENTRIC:
            if (u2_angleDeg >= (uint16_t)REP_ANGLE_UP_DEG)
            {
                u4g_timeTopReached = u4_timeMs;
                stg_state = REP_STATE_TOP;
            }
            else if (u2_angleDeg < (uint16_t)REP_ANGLE_DOWN_DEG)
            {
                /* the arm went back down without reaching the top */
                if ((u2g_angleMax - u2g_angleMin) >= (uint16_t)REP_MIN_ATTEMPT_DEG)
                {
                    v_repFinishRepetition(u4_timeMs, 0u);
                }
                stg_state = REP_STATE_READY;
                v_repResetRepetition(u2_angleDeg);
            }
            else
            {
                /* still lifting, nothing to do */
            }
            break;

        case REP_STATE_TOP:
            if (u2_angleDeg < (uint16_t)REP_ANGLE_UP_DEG)
            {
                stg_state = REP_STATE_ECCENTRIC;
            }
            else
            {
                /* still at the top, nothing to do */
            }
            break;

        case REP_STATE_ECCENTRIC:
            if (u2_angleDeg < (uint16_t)REP_ANGLE_DOWN_DEG)
            {
                v_repFinishRepetition(u4_timeMs, 1u);
                stg_state = REP_STATE_READY;
                v_repResetRepetition(u2_angleDeg);
            }
            else
            {
                /* still lowering, nothing to do */
            }
            break;

        case REP_STATE_IDLE:
        default:
            /* no set is running, samples are ignored */
            break;
    }
}

/*********************************************************************
 * @fn                - u1_repIsResultReady
 * @brief             - Tell whether a finished repetition is waiting.
 * @return            - uint8_t 1 = ready, 0 = nothing new
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_repIsResultReady(void)
{
    return u1g_resultReady;
}

/*********************************************************************
 * @fn                - v_repClearResultReady
 * @brief             - Acknowledge the result.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_repClearResultReady(void)
{
    u1g_resultReady = 0u;
}

/*********************************************************************
 * @fn                - pst_repGetResult
 * @brief             - Give read only access to the last result.
 * @return            - const RepResult_t * pointer to the result
 *//////////////////////////////////////////////////////////////////////
const RepResult_t * pst_repGetResult(void)
{
    return &stg_result;
}

/*********************************************************************
 * @fn                - pst_repGetStats
 * @brief             - Give read only access to the statistics of the set.
 * @return            - const RepStats_t * pointer to the statistics
 *//////////////////////////////////////////////////////////////////////
const RepStats_t * pst_repGetStats(void)
{
    return &stg_stats;
}

/*********************************************************************
 * @fn                - u2_repAverage
 * @brief             - Divide a running total by the number of samples.
 * @param[in]         - u4_sum   : running total
 * @param[in]         - u1_count : number of samples
 * @return            - uint16_t the average, or zero when there is no sample
 *//////////////////////////////////////////////////////////////////////
uint16_t u2_repAverage(uint32_t u4_sum, uint8_t u1_count)
{
    uint16_t u2t_average = 0u;

    if (u1_count > 0u)
    {
        u2t_average = (uint16_t)(u4_sum / (uint32_t)u1_count);
    }

    return u2t_average;
}
