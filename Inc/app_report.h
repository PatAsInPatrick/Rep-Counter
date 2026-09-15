/*******************************************************************************
 * File Name    : app_report.h
 * Description  : Builds the text lines sent to the PC terminal and keeps them
 *                in a small queue, so that the main loop never has to wait.
 *                Formatting lives here so that the UART driver stays generic.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef APP_REPORT_H
#define APP_REPORT_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "app_repcount.h"

/* Public function prototypes ------------------------------------------------*/
extern void v_reportInit(void);
extern void v_reportBanner(void);
extern void v_reportSetStarted(void);
extern void v_reportRepetition(const RepResult_t * pst_result);
extern void v_reportSetSummary(const RepResult_t * pst_result);
extern void v_reportSafetyLocked(void);
extern void v_reportSafetyCleared(void);
extern void v_reportBlocked(uint8_t u1_blockedCount);
extern void v_reportOverheat(uint8_t u1_active, int16_t s2_temperatureDeci);
extern void v_reportPump(void);

#endif /* APP_REPORT_H */
