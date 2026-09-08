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

/* Private define ------------------------------------------------------------*/
#define DISPLAY_SEG7_SLOT_MS        (700uL) /* time one digit stays visible   */
#define DISPLAY_SEG7_GAP_MS         (200uL) /* blank gap between two digits   */
#define DISPLAY_SEG7_CYCLE_MS       (DISPLAY_SEG7_SLOT_MS + DISPLAY_SEG7_GAP_MS)
#define DISPLAY_SEG7_TOTAL_MS       (DISPLAY_SEG7_CYCLE_MS * 2uL)

#define DISPLAY_DECIMAL_BASE        (10u)
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
#define DISPLAY_PAGE_LAST           (5u)
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
#define DISPLAY_VALUE_X             (72u)
#define DISPLAY_BIG_X               (46u)
#define DISPLAY_BIG_Y               (18u)

/* Private variables ---------------------------------------------------------*/
static uint16_t u2g_angleDeg = 0u;
static uint8_t  u1g_ladderMask = 0u;
static uint8_t  u1g_page = 0u;
static const RepResult_t * pstg_lastResult = NULL;

/* Private function prototypes -----------------------------------------------*/
static void v_displaySeg7Task(uint32_t u4_timeMs, uint8_t u1_count);
static void v_displayLedTask(uint8_t u1_setActive);
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

    if (u1_setActive == 0u)
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
 * @fn                - v_displayShowIdle
 * @brief             - Screen shown before the first set starts.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayShowIdle(void)
{
    v_oledClear();
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_TITLE_Y,
                     "YOK HAI SUD", 1u);
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_LINE2_Y,
                     "FULL RANGE COUNT", 1u);
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_STATUS_Y,
                     "PRESS D2 TO START", 1u);
    v_oledFlush();
}

/*********************************************************************
 * @fn                - v_displayShowRunning
 * @brief             - Fixed screen kept during the whole set. The live
 *                      feedback comes from the LEDs and the 7-Segment,
 *                      so the screen is not touched again until the end.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_displayShowRunning(void)
{
    v_oledClear();
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_TITLE_Y,
                     "SET RUNNING", 1u);
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_LINE1_Y,
                     "COUNT ON 7SEG", 1u);
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_LINE2_Y,
                     "HEIGHT ON 3 LEDS", 1u);
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, 38u, "LIFT NOW", 2u);
    v_oledDrawString((uint8_t)DISPLAY_MARGIN_X, (uint8_t)DISPLAY_STATUS_Y,
                     "PRESS D2 TO END", 1u);
    v_oledFlush();
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
                     "MIN", 1u);
    v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X, (uint8_t)DISPLAY_STAT_ROW2_Y,
                     "AVG", 1u);
    v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X, (uint8_t)DISPLAY_STAT_ROW3_Y,
                     "MAX", 1u);

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

        v_oledClear();

        switch (u1g_page)
        {
            case DISPLAY_PAGE_ROM:
                v_displayDrawStatPage("RANGE OF MOTION", "DEG",
                                      pst_stats->u2_romMin,
                                      u2_repAverage(pst_stats->u4_romSum,
                                                    pst_stats->u1_validCount),
                                      pst_stats->u2_romMax, 0u);
                break;

            case DISPLAY_PAGE_UP:
                v_displayDrawStatPage("LIFT UP TIME", "S",
                                      pst_stats->u2_upMin,
                                      u2_repAverage(pst_stats->u4_upSum,
                                                    pst_stats->u1_validCount),
                                      pst_stats->u2_upMax, 1u);
                break;

            case DISPLAY_PAGE_DOWN:
                v_displayDrawStatPage("LOWER DOWN TIME", "S",
                                      pst_stats->u2_downMin,
                                      u2_repAverage(pst_stats->u4_downSum,
                                                    pst_stats->u1_validCount),
                                      pst_stats->u2_downMax, 1u);
                break;

            case DISPLAY_PAGE_SPEED:
                v_displayDrawStatPage("LIFT SPEED", "D/S",
                                      pst_stats->u2_speedMin,
                                      u2_repAverage(pst_stats->u4_speedSum,
                                                    pst_stats->u1_validCount),
                                      pst_stats->u2_speedMax, 0u);
                break;

            case DISPLAY_PAGE_DROP:
                v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                                 (uint8_t)DISPLAY_TITLE_Y, "SPEED DROP", 1u);
                v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X,
                                 (uint8_t)DISPLAY_STAT_ROW1_Y, "LAST", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                                 (uint8_t)DISPLAY_STAT_ROW1_Y,
                                 (uint16_t)pst_result->u1_dropPercent, 1u);
                v_oledDrawString((uint8_t)DISPLAY_UNIT_X,
                                 (uint8_t)DISPLAY_STAT_ROW1_Y, "PCT", 1u);

                v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X,
                                 (uint8_t)DISPLAY_STAT_ROW2_Y, "WORST", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                                 (uint8_t)DISPLAY_STAT_ROW2_Y,
                                 (uint16_t)pst_stats->u1_dropMax, 1u);
                v_oledDrawString((uint8_t)DISPLAY_UNIT_X,
                                 (uint8_t)DISPLAY_STAT_ROW2_Y, "PCT", 1u);

                v_oledDrawString((uint8_t)DISPLAY_STAT_LABEL_X,
                                 (uint8_t)DISPLAY_STAT_ROW3_Y, "TIRED", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_STAT_VALUE_X,
                                 (uint8_t)DISPLAY_STAT_ROW3_Y,
                                 (uint16_t)pst_stats->u1_fatigueCount, 1u);
                v_oledDrawString((uint8_t)DISPLAY_UNIT_X,
                                 (uint8_t)DISPLAY_STAT_ROW3_Y, "REPS", 1u);
                break;

            case DISPLAY_PAGE_OVERVIEW:
            default:
                v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                                 (uint8_t)DISPLAY_TITLE_Y, "SET RESULT", 1u);

                v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                                 (uint8_t)DISPLAY_ROW_REPS_Y, "REPS", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_VALUE_X,
                                 (uint8_t)DISPLAY_ROW_REPS_Y,
                                 (uint16_t)pst_result->u1_repCount, 1u);

                v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                                 (uint8_t)DISPLAY_ROW_PARTIAL_Y, "PARTIAL", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_VALUE_X,
                                 (uint8_t)DISPLAY_ROW_PARTIAL_Y,
                                 (uint16_t)pst_result->u1_partialCount, 1u);

                v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                                 (uint8_t)DISPLAY_ROW_FATIGUE_Y, "FATIGUE", 1u);
                v_oledDrawNumber((uint8_t)DISPLAY_VALUE_X,
                                 (uint8_t)DISPLAY_ROW_FATIGUE_Y,
                                 (uint16_t)pst_stats->u1_fatigueCount, 1u);
                break;
        }

        if (u1g_page == (uint8_t)DISPLAY_PAGE_OVERVIEW)
        {
            v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                             (uint8_t)DISPLAY_STATUS_Y, "D4 DETAIL  D2 NEW", 1u);
        }
        else
        {
            v_oledDrawString((uint8_t)DISPLAY_MARGIN_X,
                             (uint8_t)DISPLAY_STATUS_Y, "D4 NEXT  D3 BACK", 1u);
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
    v_displaySeg7Task(u4_timeMs, pst_result->u1_repCount);
    v_displayLedTask(u1_setActive);
}
