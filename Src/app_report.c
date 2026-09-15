/*******************************************************************************
 * File Name    : app_report.c
 * Description  : Text output. Numbers are converted by hand because sprintf
 *                needs a large heap and makes the program crash on this MCU.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "app_report.h"
#include "drv_uart.h"
#include "app_energy.h"
#include "app_thermal.h"
#include "app_safety.h"

/* Private define ------------------------------------------------------------*/
#define REPORT_LINE_LENGTH          (72u)
#define REPORT_QUEUE_LENGTH         (4u)
#define REPORT_DECIMAL_BASE         (10u)
#define REPORT_ASCII_ZERO           (48u)   /* '0' */
#define REPORT_DIGIT_MAX            (5u)    /* 65535 has five digits          */

/* Private variables ---------------------------------------------------------*/
static uint8_t  u1g_queue[REPORT_QUEUE_LENGTH][REPORT_LINE_LENGTH];
static uint16_t u2g_queueLength[REPORT_QUEUE_LENGTH];
static uint8_t  u1g_queueHead = 0u;
static uint8_t  u1g_queueTail = 0u;
static uint8_t  u1g_queueCount = 0u;

/* Private function prototypes -----------------------------------------------*/
static uint16_t u2_reportAddText(uint8_t * pu1_line, uint16_t u2_index,
                                 const char * pc_text);
static uint16_t u2_reportAddNumber(uint8_t * pu1_line, uint16_t u2_index,
                                   uint16_t u2_value);
static void v_reportPush(const uint8_t * pu1_line, uint16_t u2_length);

/* Private functions ---------------------------------------------------------*/

/*********************************************************************
 * @fn                - u2_reportAddText
 * @brief             - Copy a constant string into the line buffer.
 * @param[in]         - pu1_line : destination buffer
 * @param[in]         - u2_index : current write position
 * @param[in]         - pc_text  : text to append
 * @return            - uint16_t new write position
 *//////////////////////////////////////////////////////////////////////
static uint16_t u2_reportAddText(uint8_t * pu1_line, uint16_t u2_index,
                                 const char * pc_text)
{
    uint16_t u2t_index = u2_index;
    uint16_t u2t_source = 0u;

    while ((pc_text[u2t_source] != '\0') && (u2t_index < (REPORT_LINE_LENGTH - 1u)))
    {
        pu1_line[u2t_index] = (uint8_t)pc_text[u2t_source];
        u2t_index++;
        u2t_source++;
    }

    return u2t_index;
}

/*********************************************************************
 * @fn                - u2_reportAddNumber
 * @brief             - Convert an unsigned number into decimal characters.
 * @param[in]         - pu1_line : destination buffer
 * @param[in]         - u2_index : current write position
 * @param[in]         - u2_value : value to print
 * @return            - uint16_t new write position
 *//////////////////////////////////////////////////////////////////////
static uint16_t u2_reportAddNumber(uint8_t * pu1_line, uint16_t u2_index,
                                   uint16_t u2_value)
{
    uint8_t  u1t_digit[REPORT_DIGIT_MAX];
    uint8_t  u1t_count = 0u;
    uint16_t u2t_work = u2_value;
    uint16_t u2t_index = u2_index;

    do
    {
        u1t_digit[u1t_count] = (uint8_t)(u2t_work % REPORT_DECIMAL_BASE);
        u2t_work = u2t_work / REPORT_DECIMAL_BASE;
        u1t_count++;
    }
    while ((u2t_work > 0u) && (u1t_count < (uint8_t)REPORT_DIGIT_MAX));

    while (u1t_count > 0u)
    {
        u1t_count--;
        if (u2t_index < (REPORT_LINE_LENGTH - 1u))
        {
            pu1_line[u2t_index] = u1t_digit[u1t_count] + (uint8_t)REPORT_ASCII_ZERO;
            u2t_index++;
        }
    }

    return u2t_index;
}

/*********************************************************************
 * @fn                - v_reportPush
 * @brief             - Put one finished line into the transmit queue.
 * @param[in]         - pu1_line  : line content
 * @param[in]         - u2_length : number of bytes
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_reportPush(const uint8_t * pu1_line, uint16_t u2_length)
{
    uint16_t u2t_index = 0u;

    if (u1g_queueCount < (uint8_t)REPORT_QUEUE_LENGTH)
    {
        for (u2t_index = 0u; u2t_index < u2_length; u2t_index++)
        {
            u1g_queue[u1g_queueHead][u2t_index] = pu1_line[u2t_index];
        }

        u2g_queueLength[u1g_queueHead] = u2_length;
        u1g_queueHead++;
        if (u1g_queueHead >= (uint8_t)REPORT_QUEUE_LENGTH)
        {
            u1g_queueHead = 0u;
        }
        u1g_queueCount++;
    }
}

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_reportInit
 * @brief             - Empty the transmit queue.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_reportInit(void)
{
    u1g_queueHead = 0u;
    u1g_queueTail = 0u;
    u1g_queueCount = 0u;
}

/*********************************************************************
 * @fn                - v_reportBanner
 * @brief             - Print the start-up message.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_reportBanner(void)
{
    uint8_t  u1t_line[REPORT_LINE_LENGTH];
    uint16_t u2t_index = 0u;

    u2t_index = u2_reportAddText(u1t_line, u2t_index,
                                 "SMART REP COUNTER - press button to start\r\n");
    v_reportPush(u1t_line, u2t_index);
}

/*********************************************************************
 * @fn                - v_reportSetStarted
 * @brief             - Print the header of a new set.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_reportSetStarted(void)
{
    uint8_t  u1t_line[REPORT_LINE_LENGTH];
    uint16_t u2t_index = 0u;

    u2t_index = u2_reportAddText(u1t_line, u2t_index,
                                 "SET START,rep,rom,up_ms,down_ms,speed,fatigue\r\n");
    v_reportPush(u1t_line, u2t_index);
}

/*********************************************************************
 * @fn                - v_reportRepetition
 * @brief             - Print one line for the repetition that just ended.
 * @param[in]         - pst_result : measurement of the repetition
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_reportRepetition(const RepResult_t * pst_result)
{
    uint8_t  u1t_line[REPORT_LINE_LENGTH];
    uint16_t u2t_index = 0u;

    if (pst_result->u1_validFlag == 1u)
    {
        u2t_index = u2_reportAddText(u1t_line, u2t_index, "REP,");
        u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                       (uint16_t)pst_result->u1_repCount);
    }
    else
    {
        u2t_index = u2_reportAddText(u1t_line, u2t_index, "PARTIAL,");
        u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                       (uint16_t)pst_result->u1_partialCount);
    }

    u2t_index = u2_reportAddText(u1t_line, u2t_index, ",rom=");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index, pst_result->u2_romDeg);
    u2t_index = u2_reportAddText(u1t_line, u2t_index, ",up=");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index, pst_result->u2_concentricMs);
    u2t_index = u2_reportAddText(u1t_line, u2t_index, ",down=");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index, pst_result->u2_eccentricMs);
    u2t_index = u2_reportAddText(u1t_line, u2t_index, ",speed=");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index, pst_result->u2_velocityDps);

    if (pst_result->u1_fatigueFlag == 1u)
    {
        u2t_index = u2_reportAddText(u1t_line, u2t_index, ",FATIGUE");
    }

    u2t_index = u2_reportAddText(u1t_line, u2t_index, "\r\n");
    v_reportPush(u1t_line, u2t_index);
}

/*********************************************************************
 * @fn                - v_reportSetSummary
 * @brief             - Print the summary when the set is stopped.
 * @param[in]         - pst_result : totals of the set
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_reportSetSummary(const RepResult_t * pst_result)
{
    uint8_t  u1t_line[REPORT_LINE_LENGTH];
    uint16_t u2t_index = 0u;

    u2t_index = u2_reportAddText(u1t_line, u2t_index, "SET END,total=");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                   (uint16_t)pst_result->u1_repCount);
    u2t_index = u2_reportAddText(u1t_line, u2t_index, ",partial=");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                   (uint16_t)pst_result->u1_partialCount);
    u2t_index = u2_reportAddText(u1t_line, u2t_index, ",fatigue=");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                   (uint16_t)pst_repGetStats()->u1_fatigueCount);
    u2t_index = u2_reportAddText(u1t_line, u2t_index, ",kcal=");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                   (uint16_t)(u2_energyGetKcalX100() / 100u));
    u2t_index = u2_reportAddText(u1t_line, u2t_index, ".");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                   (uint16_t)(u2_energyGetKcalX100() % 100u));
    u2t_index = u2_reportAddText(u1t_line, u2t_index, ",load=");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                   (uint16_t)(u2_energyGetWeightHg() / 10u));
    u2t_index = u2_reportAddText(u1t_line, u2t_index, "kg,temp=");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                   (uint16_t)(s2_thermalGetDeci() / 10));
    u2t_index = u2_reportAddText(u1t_line, u2t_index, "C\r\n\r\n");
    v_reportPush(u1t_line, u2t_index);
}

/*********************************************************************
 * @fn                - v_reportSafetyLocked
 * @brief             - Announce that the counter has been locked for safety.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_reportSafetyLocked(void)
{
    uint8_t  u1t_line[REPORT_LINE_LENGTH];
    uint16_t u2t_index = 0u;

    u2t_index = u2_reportAddText(u1t_line, u2t_index,
                                 "ALERT,OVERTRAIN,counter locked,rest ");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                   (uint16_t)SAFETY_REST_SECONDS);
    u2t_index = u2_reportAddText(u1t_line, u2t_index, " s\r\n");
    v_reportPush(u1t_line, u2t_index);
}

/*********************************************************************
 * @fn                - v_reportSafetyCleared
 * @brief             - Announce that the user acknowledged the alarm.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_reportSafetyCleared(void)
{
    uint8_t  u1t_line[REPORT_LINE_LENGTH];
    uint16_t u2t_index = 0u;

    u2t_index = u2_reportAddText(u1t_line, u2t_index,
                                 "ALERT,CLEARED,counting resumed\r\n");
    v_reportPush(u1t_line, u2t_index);
}

/*********************************************************************
 * @fn                - v_reportBlocked
 * @brief             - Report a repetition that was refused while locked.
 * @param[in]         - u1_blockedCount : how many have been refused so far
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_reportBlocked(uint8_t u1_blockedCount)
{
    uint8_t  u1t_line[REPORT_LINE_LENGTH];
    uint16_t u2t_index = 0u;

    u2t_index = u2_reportAddText(u1t_line, u2t_index, "BLOCKED,");
    u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                   (uint16_t)u1_blockedCount);
    u2t_index = u2_reportAddText(u1t_line, u2t_index, ",rest first\r\n");
    v_reportPush(u1t_line, u2t_index);
}

/*********************************************************************
 * @fn                - v_reportOverheat
 * @brief             - Announce that the hardware watchdog stopped or
 *                      released the training.
 * @param[in]         - u1_active           : 1 = alarm on, 0 = released
 * @param[in]         - s2_temperatureDeci  : temperature in tenths
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_reportOverheat(uint8_t u1_active, int16_t s2_temperatureDeci)
{
    uint8_t  u1t_line[REPORT_LINE_LENGTH];
    uint16_t u2t_index = 0u;

    if (u1_active == 1u)
    {
        u2t_index = u2_reportAddText(u1t_line, u2t_index,
                                     "ALERT,OVERHEAT,adc watchdog,temp=");
    }
    else
    {
        u2t_index = u2_reportAddText(u1t_line, u2t_index,
                                     "ALERT,COOLED,watchdog armed,temp=");
    }

    u2t_index = u2_reportAddNumber(u1t_line, u2t_index,
                                   (uint16_t)(s2_temperatureDeci / 10));
    u2t_index = u2_reportAddText(u1t_line, u2t_index, "C\r\n");
    v_reportPush(u1t_line, u2t_index);
}

/*********************************************************************
 * @fn                - v_reportPump
 * @brief             - Hand the next queued line to the UART driver when the
 *                      previous DMA transfer has finished. Never blocks.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_reportPump(void)
{
    if ((u1g_queueCount > 0u) && (u1_uartIsBusy() == 0u))
    {
        v_uartSend(&u1g_queue[u1g_queueTail][0], u2g_queueLength[u1g_queueTail]);

        u1g_queueTail++;
        if (u1g_queueTail >= (uint8_t)REPORT_QUEUE_LENGTH)
        {
            u1g_queueTail = 0u;
        }
        u1g_queueCount--;
    }
}
