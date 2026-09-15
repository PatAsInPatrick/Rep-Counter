/*******************************************************************************
 * File Name    : app_display.c
 * Description  : The 7-Segment shows the repetition count and alternates the
 *                two digits when the count passes nine.
 *                The green LED tells that a set is running. The other three
 *                form a ladder that follows how high the arm was lifted and
 *                hold the highest step reached until the arm comes back down.
 *                The OLED shows a fixed screen while the set runs and the
 *                full result when the set is stopped.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include "app_display.h"
#include "drv_seg7.h"
#include "drv_oled.h"
#include "drv_i2c.h"
#include "drv_led.h"
#include "app_safety.h"
#include "app_energy.h"
#include "app_thermal.h"
#include "app_overheat.h"

/* Private define ------------------------------------------------------------*/
#define DISPLAY_SEG7_SLOT_MS        (700uL) /* time one digit stays visible   */
#define DISPLAY_SEG7_GAP_MS         (200uL) /* blank gap between two digits   */
#define DISPLAY_SEG7_CYCLE_MS       (DISPLAY_SEG7_SLOT_MS + DISPLAY_SEG7_GAP_MS)
#define DISPLAY_SEG7_TOTAL_MS       (DISPLAY_SEG7_CYCLE_MS * 2uL)

#define DISPLAY_DECIMAL_BASE        (10u)
#define DISPLAY_CHAR_STEP           (6u)
#define DISPLAY_COUNT_MAX           (99u)

/* Green is the power indicator of the set, the other three form the ladder. */
#define DISPLAY_STEP_YELLOW_DEG     (45u)   /* the arm has clearly moved up   */
#define DISPLAY_STEP_RED_DEG        (90u)   /* half way                       */
#define DISPLAY_STEP_BLUE_DEG       (130u)  /* full range reached             */
#define DISPLAY_RESET_DEG           (25u)   /* below this the ladder resets   */

#define DISPLAY_LED_MASK_NONE       (0x0u)
#define DISPLAY_LED_MASK_RUNNING    (0x1u)  /* green only                     */
#define DISPLAY_LED_MASK_STEP_1     (0x2u)  /* yellow                         */
#define DISPLAY_LED_MASK_STEP_2     (0x6u)  /* yellow and red                 */
#define DISPLAY_LED_MASK_STEP_3     (0xEu)  /* yellow, red and blue           */

#define DISPLAY_TITLE_Y             (0u)
#define DISPLAY_LINE1_Y             (16u)
#define DISPLAY_LINE2_Y             (26u)
#define DISPLAY_LINE3_Y             (36u)
#define DISPLAY_LINE4_Y             (46u)
#define DISPLAY_STATUS_Y            (56u)
#define DISPLAY_MARGIN_X            (4u)    /* the glass hides the very edge  */

/* Result pages. Page zero is the overview, the others show one value each. */
#define DISPLAY_PAGE_OVERVIEW       (0u)
#define DISPLAY_PAGE_ROM            (1u)
#define DISPLAY_PAGE_UP             (2u)
#define DISPLAY_PAGE_DOWN           (3u)
#define DISPLAY_PAGE_SPEED          (4u)
#define DISPLAY_PAGE_DROP           (5u)
#define DISPLAY_PAGE_ENERGY         (6u)
#define DISPLAY_PAGE_LAST           (6u)
#define DISPLAY_PAGE_FIRST_DETAIL   (1u)

#define DISPLAY_STAT_LABEL_X        (4u)
#define DISPLAY_STAT_VALUE_X        (46u)
#define DISPLAY_STAT_ROW1_Y         (14u)
#define DISPLAY_STAT_ROW2_Y         (26u)
#define DISPLAY_STAT_ROW3_Y         (38u)
#define DISPLAY_ROW_REPS_Y          (16u)
#define DISPLAY_ROW_PARTIAL_Y       (28u)
#define DISPLAY_ROW_FATIGUE_Y       (40u)
#define DISPLAY_UNIT_X              (86u)
#define DISPLAY_ALARM_PERIOD_MS     (150uL)
#define DISPLAY_LOCK_REDRAW_MS      (1000uL)
#define DISPLAY_IDLE_REDRAW_MS      (1000uL)
#define DISPLAY_LIVE_REDRAW_MS      (2000uL)
#define DISPLAY_BLINK_HALF          (2uL)
#define DISPLAY_LED_MASK_ALL        (0xFu)

/* What the screen is currently showing. It is not the same thing as the
 * state of the counter, because the result stays on screen after the set. */
#define DISPLAY_VIEW_IDLE           (0u)
#define DISPLAY_VIEW_LIVE           (1u)
#define DISPLAY_VIEW_RESULT         (2u)
#define DISPLAY_VALUE_X             (72u)
#define DISPLAY_BIG_X               (46u)
#define DISPLAY_BIG_Y               (18u)

/* Private variables ---------------------------------------------------------*/
static uint16_t u2g_angleDeg = 0u;
static uint8_t  u1g_ladderMask = 0u;
static uint8_t  u1g_page = 0u;
static uint8_t  u1g_view = DISPLAY_VIEW_IDLE;
static uint32_t u4g_alarmTimeMs = 0uL;
static uint32_t u4g_lastLockDrawMs = 0uL;
static uint16_t u2g_lastRestSec = 0xFFFFu;
static int16_t  s2g_lastShownTemp = -999;
static uint32_t u4g_lastTempDrawMs = 0uL;
static const RepResult_t * pstg_lastResult = NULL;

/* Private function prototypes -----------------------------------------------*/
static void v_displaySeg7Task(uint32_t u4_timeMs, uint8_t u1_count);
static uint8_t u1_displayDrawDecimal(uint8_t u1_x, uint8_t u1_y,
                                     uint16_t u2_tenth, uint8_t u1_scale);
static void v_displayLedTask(uint8_t u1_setActive);
static void v_displayTempTask(uint32_t u4_timeMs,
                              const RepResult_t * pst_result);
static uint8_t u1_displayLadderMask(uint16_t u2_angleDeg);
static void v_displayDrawStatPage(const char * pc_title, const char * pc_unit,
                                  uint16_t u2_min, uint16_t u2_avg,
                                  uint16_t u2_max, uint8_t u1_isTime);

/* Private functions ---------------------------------------------------------*/

/*********************************************************************
 * @fn                - u1_displayLadderMask
 * @brief             - Convert one angle into the LED pattern of that step.
 * @param[in]         - u2_angleDeg : arm angle 0..180
 * @return            - uint8_t LED bit mask
 *//////////////////////////////////////////////////////////////////////
static uint8_t u1_displayLadderMask(uint16_t u2_angleDeg)
{
    uint8_t u1t_mask = (uint8_t)DISPLAY_LED_MASK_NONE;

    if (u2_angleDeg >= (uint16_t)DISPLAY_STEP_BLUE_DEG)
    {
        u1t_mask = (uint8_t)DISPLAY_LED_MASK_STEP_3;
    }
    else if (u2_angleDeg >= (uint16_t)DISPLAY_STEP_RED_DEG)
    {
        u1t_mask = (uint8_t)DISPLAY_LED_MASK_STEP_2;
    }
    else if (u2_angleDeg >= (uint16_t)DISPLAY_STEP_YELLOW_DEG)
    {
        u1t_mask = (uint8_t)DISPLAY_LED_MASK_STEP_1;
    }
    else
    {
        u1t_mask = (uint8_t)DISPLAY_LED_MASK_NONE;
    }

    return u1t_mask;
}

/*********************************************************************
 * @fn                - v_displaySeg7Task
 * @brief             - Show the repetition count on the single digit.
 *                      Values below ten stay steady, larger values show
 *                      the tens first and then the units.
 * @param[in]         - u4_timeMs : current millisecond counter
 * @param[in]         - u1_count  : value to display
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_displaySeg7Task(uint32_t u4_timeMs, uint8_t u1_count)
{
    uint8_t  u1t_value = u1_count;
    uint32_t u4t_phase = 0uL;

    if (u1t_value > (uint8_t)DISPLAY_COUNT_MAX)
    {
        u1t_value = (uint8_t)DISPLAY_COUNT_MAX;
    }

    if (u1t_value < (uint8_t)DISPLAY_DECIMAL_BASE)
    {
        v_seg7WriteDigit(u1t_value);
    }
    else
    {
        u4t_phase = u4_timeMs % DISPLAY_SEG7_TOTAL_MS;

        if (u4t_phase < DISPLAY_SEG7_SLOT_MS)
        {
            v_seg7WriteDigit((uint8_t)(u1t_value / DISPLAY_DECIMAL_BASE));
        }
        else if (u4t_phase < DISPLAY_SEG7_CYCLE_MS)
        {
            v_seg7WriteDigit((uint8_t)SEG7_BLANK);
        }
        else if (u4t_phase < (DISPLAY_SEG7_CYCLE_MS + DISPLAY_SEG7_SLOT_MS))
        {
            v_seg7WriteDigit((uint8_t)(u1t_value % DISPLAY_DECIMAL_BASE));
        }
        else
        {
            v_seg7WriteDigit((uint8_t)SEG7_BLANK);
        }
    }
}

/*********************************************************************
 * @fn                - v_displayLedTask
 * @brief             - Green stays lit for the whole set. The other three
 *                      light up step by step as the arm goes higher, and the
 *                      pattern only grows, so a short lift stays visible at
 *                      the step it reached until the arm comes back down.
 * @param[in]         - u1_setActive : 1 = a set is running
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_displayLedTask(uint8_t u1_setActive)
{
    uint8_t u1t_mask = 0u;

    if (u1_overheatIsActive() == 1u)
    {
        /* the interrupt already switched them on, keep them that way */
        v_ledWriteAll((uint8_t)DISPLAY_LED_MASK_ALL);
    }
    else if (u1_safetyIsLocked() == 1u)
    {
        if (u2_safetyRestRemainSec(u4g_alarmTimeMs) == 0u)
        {
            /* the rest is over, the alarm stops flashing and stays lit */
            v_ledWriteAll((uint8_t)DISPLAY_LED_MASK_ALL);
        }
        else if (((u4g_alarmTimeMs / DISPLAY_ALARM_PERIOD_MS)
                  % DISPLAY_BLINK_HALF) == 0uL)
        {
            v_ledWriteAll((uint8_t)DISPLAY_LED_MASK_ALL);
        }
        else
        {
            v_ledWriteAll(0u);
        }
    }
    else if (u1_setActive == 0u)
    {
        u1g_ladderMask = (uint8_t)DISPLAY_LED_MASK_NONE;
        v_ledWriteAll((uint8_t)DISPLAY_LED_MASK_NONE);
    }
    else
    {
        if (u2g_angleDeg < (uint16_t)DISPLAY_RESET_DEG)
        {
            u1g_ladderMask = (uint8_t)DISPLAY_LED_MASK_NONE;
        }
        else
        {
            u1t_mask = u1_displayLadderMask(u2g_angleDeg);
            if (u1t_mask > u1g_ladderMask)
            {
                u1g_ladderMask = u1t_mask;
            }
        }

        v_ledWriteAll((uint8_t)(u1g_ladderMask
                               | (uint8_t)DISPLAY_LED_MASK_RUNNING));
    }
}

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_displayInit
 * @brief             - Prepare the digit, the LEDs and the screen.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayInit(void)
{
    v_seg7Init();
    v_ledInit();
    v_i2cInit();
    v_oledInit();

    u2g_angleDeg = 0u;
    u1g_ladderMask = 0u;
    u1g_page = (uint8_t)DISPLAY_PAGE_OVERVIEW;
    u1g_view = (uint8_t)DISPLAY_VIEW_IDLE;
    u4g_alarmTimeMs = 0uL;
    u4g_lastLockDrawMs = 0uL;
    u2g_lastRestSec = 0xFFFFu;
    s2g_lastShownTemp = -999;
    u4g_lastTempDrawMs = 0uL;
    pstg_lastResult = NULL;
}

/*********************************************************************
 * @fn                - v_displaySetAngle
 * @brief             - Give the current arm angle used by the LED ladder.
 * @param[in]         - u2_angleDeg : angle 0..180
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displaySetAngle(uint16_t u2_angleDeg)
{
    u2g_angleDeg = u2_angleDeg;
}

/*********************************************************************
 * @fn                - v_displayDrawDecimal
 * @brief             - Draw a value that carries one decimal digit, for
 *                      example 125 is printed as 12.5
 * @param[in]         - u1_x     : left position
 * @param[in]         - u1_y     : top position
 * @param[in]         - u2_tenth : value in tenths
 * @param[in]         - u1_scale : text size
 * @return            - uint8_t position just after the last digit
 *//////////////////////////////////////////////////////////////////////
static uint8_t u1_displayDrawDecimal(uint8_t u1_x, uint8_t u1_y,
                                     uint16_t u2_tenth, uint8_t u1_scale)
{
    uint8_t  u1t_x = u1_x;
    uint16_t u2t_whole = (uint16_t)(u2_tenth / DISPLAY_DECIMAL_BASE);
    uint16_t u2t_part = (uint16_t)(u2_tenth % DISPLAY_DECIMAL_BASE);
    uint8_t  u1t_step = (uint8_t)(DISPLAY_CHAR_STEP * u1_scale);

    v_oledDrawNumber(u1t_x, u1_y, u2t_whole, u1_scale);
    if (u2t_whole >= (uint16_t)DISPLAY_DECIMAL_BASE)
    {
        u1t_x = (uint8_t)(u1t_x + u1t_step);
    }
    u1t_x = (uint8_t)(u1t_x + u1t_step);

    v_oledDrawString(u1t_x, u1_y, ".", u1_scale);
    u1t_x = (uint8_t)(u1t_x + u1t_step);

    v_oledDrawNumber(u1t_x, u1_y, u2t_part, u1_scale);
    u1t_x = (uint8_t)(u1t_x + u1t_step);

    return u1t_x;
}

/*********************************************************************
 * @fn                - v_displayShowIdle
 * @brief             - Setup screen. The weight is the main figure here.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayShowIdle(void)
{
    uint8_t u1t_x = 0u;

    u1g_view = (uint8_t)DISPLAY_VIEW_IDLE;

    v_oledClear();
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 0u, "Setup", 1u);

    u1t_x = u1_displayDrawDecimal((uint8_t)DISPLAY_MARGIN_X, 12u,
                                  u2_energyGetWeightHg(), 2u);
    v_oledDrawString((uint8_t)(u1t_x + 4u), 19u, "kg", 1u);

    s2g_lastShownTemp = s2_thermalGetDeci();

    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 34u, "Room", 1u);
    u1t_x = u1_displayDrawDecimal(34u, 34u,
                                  (uint16_t)s2_thermalGetDeci(), 1u);
    v_oledDrawString((uint8_t)(u1t_x + 2u), 34u, "C", 1u);

    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 46u, "D3 -    D4 +", 1u);
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_STATUS_Y,
                     "D2 Start", 1u);
    v_oledFlush();
}

/*********************************************************************
 * @fn                - v_displayShowLocked
 * @brief             - Full screen warning shown when the counter has been
 *                      locked because the user kept lifting while tired.
 * @param[in]         - u4_timeMs : current millisecond counter
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayShowLocked(uint32_t u4_timeMs)
{
    uint16_t u2t_remain = u2_safetyRestRemainSec(u4_timeMs);

    v_oledClear();
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 0u, "STOP", 2u);
    v_oledDrawString(62u, 7u, "Overtrain", 1u);

    if (u2t_remain > 0u)
    {
        v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 22u, "Rest", 1u);
        v_oledDrawNumber(40u, 20u, u2t_remain, 3u);
        v_oledDrawString(82u, 34u, "s", 1u);
        v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_STATUS_Y,
                         "Do not lift", 1u);
    }
    else
    {
        v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 24u, "Rest done", 2u);
        v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_STATUS_Y,
                         "D5 Resume", 1u);
    }

    v_oledFlush();
}

/*********************************************************************
 * @fn                - v_displayShowOverheat
 * @brief             - Screen drawn after the analog watchdog has stopped
 *                      the training. The lights were already switched on
 *                      inside the interrupt handler long before this.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayShowOverheat(void)
{
    uint8_t u1t_x = 0u;

    v_oledClear();
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 0u, "TOO HOT", 2u);

    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 22u, "Room", 1u);
    u1t_x = u1_displayDrawDecimal(34u, 22u, (uint16_t)s2_thermalGetDeci(), 1u);
    v_oledDrawString((uint8_t)(u1t_x + 2u), 22u, "C", 1u);

    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 34u, "Limit", 1u);
    u1t_x = u1_displayDrawDecimal(40u, 34u, (uint16_t)OVERHEAT_TRIP_DECI, 1u);
    v_oledDrawString((uint8_t)(u1t_x + 2u), 34u, "C", 1u);

    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 44u, "Training stopped", 1u);
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_STATUS_Y,
                     "Wait to cool down", 1u);
    v_oledFlush();
}

/*********************************************************************
 * @fn                - v_displayShowLive
 * @brief             - Screen shown while the set is running. It is redrawn
 *                      once for every repetition, so the count and the
 *                      fatigue warning appear the moment they happen.
 * @param[in]         - pst_result : measurements of the last repetition
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayShowLive(const RepResult_t * pst_result)
{
    uint8_t u1t_x = 0u;

    if (pst_result != NULL)
    {
        u1g_view = (uint8_t)DISPLAY_VIEW_LIVE;
        s2g_lastShownTemp = s2_thermalGetDeci();

        v_oledClear();

        v_oledDrawNumber((uint8_t)DISPLAY_MARGIN_X, 0u,
                         (uint16_t)pst_result->u1_repCount, 3u);
        v_oledDrawString(44u, 6u, "/", 1u);
        v_oledDrawNumber(52u, 6u, (uint16_t)REP_TARGET_COUNT, 1u);
        v_oledDrawString(44u, 16u, "reps", 1u);

        u1t_x = u1_displayDrawDecimal(84u, 0u,
                                      (uint16_t)s2_thermalGetDeci(), 1u);
        v_oledDrawString(u1t_x, 0u, "C", 1u);

        v_oledDrawString(84u, 12u, "kcal", 1u);
        (void)u1_displayDrawDecimal(84u, 22u,
                                    (uint16_t)(u2_energyGetKcalX100()
                                    / DISPLAY_DECIMAL_BASE), 1u);

        v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 32u, "Rom", 1u);
        v_oledDrawNumber(30u, 32u, pst_result->u2_romDeg, 1u);
        v_oledDrawString(66u, 32u, "Spd", 1u);
        v_oledDrawNumber(92u, 32u, pst_result->u2_velocityDps, 1u);

        v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 42u, "Drop", 1u);
        v_oledDrawNumber(34u, 42u, (uint16_t)pst_result->u1_dropPercent, 1u);
        v_oledDrawString(54u, 42u, "%", 1u);
        v_oledDrawString(66u, 42u, "Up", 1u);
        v_oledDrawSeconds(86u, 42u, pst_result->u2_concentricMs, 1u);

        if (st_safetyGetState() == SAFETY_STATE_WARN)
        {
            v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_STATUS_Y,
                             "Tired - D5 to clear", 1u);
        }
        else if (pst_result->u1_validFlag == 0u)
        {
            v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_STATUS_Y,
                             "Lift higher", 1u);
        }
        else
        {
            v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_STATUS_Y,
                             "Good  D2 to end", 1u);
        }

        v_oledFlush();
    }
}

/*********************************************************************
 * @fn                - v_displayShowRunning
 * @brief             - First live screen drawn when the set starts.
 * @param[in]         - pst_result : measurements, still empty at this point
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayShowRunning(const RepResult_t * pst_result)
{
    v_displayShowLive(pst_result);
}

/*********************************************************************
 * @fn                - v_displayDrawStatPage
 * @brief             - Draw one detail page with the minimum, the average
 *                      and the maximum of a value over the whole set.
 * @param[in]         - pc_title  : name of the value
 * @param[in]         - pc_unit   : unit written after each number
 * @param[in]         - u2_min    : smallest value of the set
 * @param[in]         - u2_avg    : average value of the set
 * @param[in]         - u2_max    : largest value of the set
 * @param[in]         - u1_isTime : 1 = print as seconds, 0 = plain number
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_displayDrawStatPage(const char * pc_title, const char * pc_unit,
                                  uint16_t u2_min, uint16_t u2_avg,
                                  uint16_t u2_max, uint8_t u1_isTime)
{
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_TITLE_Y,
                     pc_title, 1u);

    v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X, (uint8_t)DISPLAY_STAT_ROW1_Y,
                     "Min", 1u);
    v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X, (uint8_t)DISPLAY_STAT_ROW2_Y,
                     "Avg", 1u);
    v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X, (uint8_t)DISPLAY_STAT_ROW3_Y,
                     "Max", 1u);

    if (u1_isTime == 1u)
    {
        v_oledDrawSeconds((uint8_t)DISPLAY_STAT_VALUE_X,
                          (uint8_t)DISPLAY_STAT_ROW1_Y, u2_min, 1u);
        v_oledDrawSeconds((uint8_t)DISPLAY_STAT_VALUE_X,
                          (uint8_t)DISPLAY_STAT_ROW2_Y, u2_avg, 1u);
        v_oledDrawSeconds((uint8_t)DISPLAY_STAT_VALUE_X,
                          (uint8_t)DISPLAY_STAT_ROW3_Y, u2_max, 1u);
    }
    else
    {
        v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                         (uint8_t)DISPLAY_STAT_ROW1_Y, u2_min, 1u);
        v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                         (uint8_t)DISPLAY_STAT_ROW2_Y, u2_avg, 1u);
        v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                         (uint8_t)DISPLAY_STAT_ROW3_Y, u2_max, 1u);
    }

    v_oledDrawString((uint8_t)DISPLAY_UNIT_X, (uint8_t)DISPLAY_STAT_ROW1_Y,
                     pc_unit, 1u);
    v_oledDrawString((uint8_t)DISPLAY_UNIT_X, (uint8_t)DISPLAY_STAT_ROW2_Y,
                     pc_unit, 1u);
    v_oledDrawString((uint8_t)DISPLAY_UNIT_X, (uint8_t)DISPLAY_STAT_ROW3_Y,
                     pc_unit, 1u);
}

/*********************************************************************
 * @fn                - v_displayShowSummary
 * @brief             - Draw the page that is currently selected. Called
 *                      when the set stops and every time a page button
 *                      is pressed.
 * @param[in]         - pst_result : totals of the set
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayShowSummary(const RepResult_t * pst_result)
{
    const RepStats_t * pst_stats = pst_repGetStats();

    if ((pst_result != NULL) && (pst_stats != NULL))
    {
        pstg_lastResult = pst_result;
        u1g_view = (uint8_t)DISPLAY_VIEW_RESULT;

        v_oledClear();

        switch (u1g_page)
        {
            case DISPLAY_PAGE_ROM:
                v_displayDrawStatPage("Range of motion", "deg",
                                      pst_stats->u2_romMin,
                                      u2_repAverage(pst_stats->u4_romSum,
                                                    pst_stats->u1_validCount),
                                      pst_stats->u2_romMax, 0u);
                break;

            case DISPLAY_PAGE_UP:
                v_displayDrawStatPage("Lift up time", "s",
                                      pst_stats->u2_upMin,
                                      u2_repAverage(pst_stats->u4_upSum,
                                                    pst_stats->u1_validCount),
                                      pst_stats->u2_upMax, 1u);
                break;

            case DISPLAY_PAGE_DOWN:
                v_displayDrawStatPage("Lower down time", "s",
                                      pst_stats->u2_downMin,
                                      u2_repAverage(pst_stats->u4_downSum,
                                                    pst_stats->u1_validCount),
                                      pst_stats->u2_downMax, 1u);
                break;

            case DISPLAY_PAGE_SPEED:
                v_displayDrawStatPage("Lift speed", "deg/s",
                                      pst_stats->u2_speedMin,
                                      u2_repAverage(pst_stats->u4_speedSum,
                                                    pst_stats->u1_validCount),
                                      pst_stats->u2_speedMax, 0u);
                break;

            case DISPLAY_PAGE_DROP:
                v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                                 (uint8_t)DISPLAY_TITLE_Y, "Speed drop", 1u);
                v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X,
                                 (uint8_t)DISPLAY_STAT_ROW1_Y, "Last", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                                 (uint8_t)DISPLAY_STAT_ROW1_Y,
                                 (uint16_t)pst_result->u1_dropPercent, 1u);
                v_oledDrawString((uint8_t)DISPLAY_UNIT_X,
                                 (uint8_t)DISPLAY_STAT_ROW1_Y, "%", 1u);

                v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X,
                                 (uint8_t)DISPLAY_STAT_ROW2_Y, "Worst", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                                 (uint8_t)DISPLAY_STAT_ROW2_Y,
                                 (uint16_t)pst_stats->u1_dropMax, 1u);
                v_oledDrawString((uint8_t)DISPLAY_UNIT_X,
                                 (uint8_t)DISPLAY_STAT_ROW2_Y, "%", 1u);

                v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X,
                                 (uint8_t)DISPLAY_STAT_ROW3_Y, "Tired", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                                 (uint8_t)DISPLAY_STAT_ROW3_Y,
                                 (uint16_t)pst_stats->u1_fatigueCount, 1u);
                v_oledDrawString((uint8_t)DISPLAY_UNIT_X,
                                 (uint8_t)DISPLAY_STAT_ROW3_Y, "reps", 1u);
                break;

            case DISPLAY_PAGE_ENERGY:
                v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                                 (uint8_t)DISPLAY_TITLE_Y, "Energy", 1u);

                v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X,
                                 (uint8_t)DISPLAY_STAT_ROW1_Y, "kcal", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                                 (uint8_t)DISPLAY_STAT_ROW1_Y,
                                 (uint16_t)(u2_energyGetKcalX100() / 100u), 1u);
                v_oledDrawString(58u, (uint8_t)DISPLAY_STAT_ROW1_Y, ".", 1u);
                v_oledDrawNumber(64u, (uint8_t)DISPLAY_STAT_ROW1_Y,
                                 (uint16_t)(u2_energyGetKcalX100() % 100u), 1u);

                v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X,
                                 (uint8_t)DISPLAY_STAT_ROW2_Y, "Load", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                                 (uint8_t)DISPLAY_STAT_ROW2_Y,
                                 (uint16_t)(u2_energyGetWeightHg() / 10u), 1u);
                v_oledDrawString((uint8_t)DISPLAY_UNIT_X,
                                 (uint8_t)DISPLAY_STAT_ROW2_Y, "kg", 1u);

                v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X,
                                 (uint8_t)DISPLAY_STAT_ROW3_Y, "Heat", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                                 (uint8_t)DISPLAY_STAT_ROW3_Y,
                                 (uint16_t)u1_energyGetHeatPercent(), 1u);
                v_oledDrawString((uint8_t)DISPLAY_UNIT_X,
                                 (uint8_t)DISPLAY_STAT_ROW3_Y, "%", 1u);
                break;

            case DISPLAY_PAGE_OVERVIEW:
            default:
                if (u1_repIsTargetReached() == 1u)
                {
                    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                                     (uint8_t)DISPLAY_TITLE_Y, "Set done", 1u);
                }
                else
                {
                    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                                     (uint8_t)DISPLAY_TITLE_Y, "Set result", 1u);
                }

                v_oledDrawNumber((uint8_t)DISPLAY_MARGIN_X, 12u,
                                 (uint16_t)pst_result->u1_repCount, 3u);
                v_oledDrawString(46u, 20u, "reps", 1u);

                v_oledDrawString(82u, 12u, "kcal", 1u);
                (void)u1_displayDrawDecimal(82u, 24u,
                                            (uint16_t)(u2_energyGetKcalX100()
                                            / DISPLAY_DECIMAL_BASE), 1u);

                v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 40u, "Partial", 1u);
                v_oledDrawNumber(52u, 40u,
                                 (uint16_t)pst_result->u1_partialCount, 1u);
                v_oledDrawString(70u, 40u, "Tired", 1u);
                v_oledDrawNumber(106u, 40u,
                                 (uint16_t)pst_stats->u1_fatigueCount, 1u);
                break;
        }

        if (u1g_page == (uint8_t)DISPLAY_PAGE_OVERVIEW)
        {
            v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                             (uint8_t)DISPLAY_STATUS_Y, "D4 more   D5 setup", 1u);
        }
        else
        {
            v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                             (uint8_t)DISPLAY_STATUS_Y, "D4 next   D3 back", 1u);
        }

        v_oledFlush();
    }
}

/*********************************************************************
 * @fn                - v_displayShowOverview
 * @brief             - Jump straight back to the overview page.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayShowOverview(void)
{
    u1g_page = (uint8_t)DISPLAY_PAGE_OVERVIEW;

    if (pstg_lastResult != NULL)
    {
        v_displayShowSummary(pstg_lastResult);
    }
}

/*********************************************************************
 * @fn                - v_displayBack
 * @brief             - One step backwards. From a detail page it returns to
 *                      the overview, and from the overview it leaves the
 *                      result view so that the weight can be set again.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayBack(void)
{
    if (u1g_page != (uint8_t)DISPLAY_PAGE_OVERVIEW)
    {
        v_displayShowOverview();
    }
    else
    {
        v_displayShowIdle();
    }
}

/*********************************************************************
 * @fn                - u1_displayIsResultView
 * @brief             - Tell whether the result of a finished set is on
 *                      screen. The buttons act differently in that case.
 * @return            - uint8_t 1 = showing a result page
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_displayIsResultView(void)
{
    uint8_t u1t_result = 0u;

    if (u1g_view == (uint8_t)DISPLAY_VIEW_RESULT)
    {
        u1t_result = 1u;
    }

    return u1t_result;
}

/*********************************************************************
 * @fn                - v_displayNextPage
 * @brief             - Step to the next detail page and wrap around.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayNextPage(void)
{
    if (u1g_page >= (uint8_t)DISPLAY_PAGE_LAST)
    {
        u1g_page = (uint8_t)DISPLAY_PAGE_FIRST_DETAIL;
    }
    else
    {
        u1g_page++;
    }

    if (pstg_lastResult != NULL)
    {
        v_displayShowSummary(pstg_lastResult);
    }
}

/*********************************************************************
 * @fn                - v_displayTempTask
 * @brief             - Redraw the screen when the reading has changed by a
 *                      tenth of a degree. On the setup screen this is free, but while
 *                      lifting it is only allowed when the arm is resting at
 *                      the bottom, because drawing takes about 92 ms and
 *                      would spoil the timing of a repetition.
 * @param[in]         - u4_timeMs  : current millisecond counter
 * @param[in]         - pst_result : latest measurements
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_displayTempTask(uint32_t u4_timeMs, const RepResult_t * pst_result)
{
    int16_t s2t_now = s2_thermalGetDeci();

    if (s2t_now == s2g_lastShownTemp)
    {
        /* nothing changed, the screen is left alone */
    }
    else if (u1g_view == (uint8_t)DISPLAY_VIEW_IDLE)
    {
        if ((u4_timeMs - u4g_lastTempDrawMs) >= DISPLAY_IDLE_REDRAW_MS)
        {
            u4g_lastTempDrawMs = u4_timeMs;
            v_displayShowIdle();
        }
    }
    else if (u1g_view == (uint8_t)DISPLAY_VIEW_LIVE)
    {
        if ((u2g_angleDeg < (uint16_t)DISPLAY_RESET_DEG)
            && ((u4_timeMs - u4g_lastTempDrawMs) >= DISPLAY_LIVE_REDRAW_MS))
        {
            u4g_lastTempDrawMs = u4_timeMs;
            v_displayShowLive(pst_result);
        }
    }
    else
    {
        /* the result pages do not show a live temperature */
    }
}

/*********************************************************************
 * @fn                - v_displayTask
 * @brief             - Called from the main loop. Only the digit and the
 *                      LEDs are refreshed here, the screen is not touched.
 * @param[in]         - u4_timeMs    : current millisecond counter
 * @param[in]         - u1_setActive : 1 = a set is running
 * @param[in]         - pst_result   : latest measurements
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayTask(uint32_t u4_timeMs, uint8_t u1_setActive,
                   const RepResult_t * pst_result)
{
    uint16_t u2t_remain = 0u;

    u4g_alarmTimeMs = u4_timeMs;

    if (u1_overheatIsActive() == 1u)
    {
        v_seg7WriteDigit((uint8_t)SEG7_BLANK);

        if (u1_overheatHasNewEvent() == 1u)
        {
            v_overheatClearNewEvent();
            v_displayShowOverheat();
        }
    }
    else if (u1_safetyIsLocked() == 1u)
    {
        u2t_remain = u2_safetyRestRemainSec(u4_timeMs);

        if (u2t_remain == 0u)
        {
            /* the rest is over, the digit stops flashing */
            v_seg7WriteDigit((uint8_t)SEG7_BLANK);
        }
        else if (((u4_timeMs / DISPLAY_ALARM_PERIOD_MS) % DISPLAY_BLINK_HALF)
                 == 0uL)
        {
            v_seg7WriteDigit(0u);
        }
        else
        {
            v_seg7WriteDigit((uint8_t)SEG7_BLANK);
        }

        if ((u2t_remain != u2g_lastRestSec)
            || ((u4_timeMs - u4g_lastLockDrawMs) >= DISPLAY_LOCK_REDRAW_MS))
        {
            u2g_lastRestSec = u2t_remain;
            u4g_lastLockDrawMs = u4_timeMs;
            v_displayShowLocked(u4_timeMs);
        }
    }
    else
    {
        v_displaySeg7Task(u4_timeMs, pst_result->u1_repCount);
        v_displayTempTask(u4_timeMs, pst_result);
    }

    v_displayLedTask(u1_setActive);
}
