/*******************************************************************************
 * File Name    : app_safety.h
 * Description  : Safety supervisor. It watches what happens after the system
 *                has warned about fatigue, and locks the counter when the
 *                user keeps lifting without acknowledging the warning.
 * Date         : 2026-09-08
 ******************************************************************************/
#ifndef APP_SAFETY_H
#define APP_SAFETY_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Public define -------------------------------------------------------------*/
#define SAFETY_REST_SECONDS         (30u)   /* forced rest before unlocking   */

/* Public enum ---------------------------------------------------------------*/
typedef enum
{
    SAFETY_STATE_SAFE = 0,      /* nothing wrong                              */
    SAFETY_STATE_WARN,          /* fatigue seen once, user was warned         */
    SAFETY_STATE_LOCKED         /* user ignored the warning, counter stopped  */
} SafetyState_t;

/* Public function prototypes ------------------------------------------------*/
extern void v_safetyInit(void);
extern void v_safetyNotifyRepetition(uint8_t u1_fatigueFlag, uint32_t u4_timeMs);
extern uint8_t u1_safetyAcknowledge(uint32_t u4_timeMs);
extern SafetyState_t st_safetyGetState(void);
extern uint8_t u1_safetyIsLocked(void);
extern uint16_t u2_safetyRestRemainSec(uint32_t u4_timeMs);
extern uint8_t u1_safetyGetBlockedCount(void);
extern void v_safetyNotifyViolation(uint32_t u4_timeMs);

#endif /* APP_SAFETY_H */
