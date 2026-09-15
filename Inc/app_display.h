/*******************************************************************************
 * File Name    : app_display.h
 * Description  : Decides what appears on the 7-Segment, on the four LEDs and
 *                on the OLED. This module holds no register access.
 *                The LEDs act as a lifting height ladder, the OLED is only
 *                redrawn when a screen really changes.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "app_repcount.h"

/* Public function prototypes ------------------------------------------------*/
extern void v_displayInit(void);
extern void v_displaySetAngle(uint16_t u2_angleDeg);
extern void v_displayShowIdle(void);
extern void v_displayShowLocked(uint32_t u4_timeMs);
extern void v_displayShowOverheat(void);
extern void v_displayShowRunning(const RepResult_t * pst_result);
extern void v_displayShowLive(const RepResult_t * pst_result);
extern void v_displayBack(void);
extern uint8_t u1_displayIsResultView(void);
extern void v_displayShowSummary(const RepResult_t * pst_result);
extern void v_displayShowOverview(void);
extern void v_displayNextPage(void);
extern void v_displayTask(uint32_t u4_timeMs, uint8_t u1_setActive,
                          const RepResult_t * pst_result);

#endif /* APP_DISPLAY_H */
