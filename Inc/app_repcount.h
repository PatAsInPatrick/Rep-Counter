/*******************************************************************************
 * File Name    : app_repcount.h
 * Description  : Repetition counting algorithm. This module contains no
 *                register access at all, so the same code will work with the
 *                potentiometer today and with the MPU6050 sensor later.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef APP_REPCOUNT_H
#define APP_REPCOUNT_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Public enum ---------------------------------------------------------------*/
typedef enum
{
    REP_STATE_IDLE = 0,         /* the set has not been started               */
    REP_STATE_READY,            /* arm is down, waiting for the lift          */
    REP_STATE_CONCENTRIC,       /* lifting up                                 */
    REP_STATE_TOP,              /* top position reached                       */
    REP_STATE_ECCENTRIC         /* lowering down                              */
} RepState_t;

/* Public struct -------------------------------------------------------------*/
typedef struct
{
    uint8_t  u1_repCount;       /* valid repetitions in this set              */
    uint8_t  u1_partialCount;   /* repetitions rejected, range of motion low  */
    uint16_t u2_romDeg;         /* range of motion of the last repetition     */
    uint16_t u2_concentricMs;   /* time spent lifting                         */
    uint16_t u2_eccentricMs;    /* time spent lowering                        */
    uint16_t u2_velocityDps;    /* lifting speed in degrees per second        */
    uint8_t  u1_fatigueFlag;    /* 1 = speed dropped, the athlete is tired    */
    uint8_t  u1_dropPercent;    /* how much slower than the first repetition  */
    uint8_t  u1_validFlag;      /* 1 = the last repetition was counted        */
} RepResult_t;

typedef struct
{
    uint8_t  u1_validCount;     /* repetitions used to build the statistics   */
    uint8_t  u1_fatigueCount;   /* repetitions flagged as too slow            */
    uint16_t u2_romMin;
    uint16_t u2_romMax;
    uint32_t u4_romSum;
    uint16_t u2_upMin;          /* milliseconds                               */
    uint16_t u2_upMax;
    uint32_t u4_upSum;
    uint16_t u2_downMin;        /* milliseconds                               */
    uint16_t u2_downMax;
    uint32_t u4_downSum;
    uint16_t u2_speedMin;       /* degrees per second                         */
    uint16_t u2_speedMax;
    uint32_t u4_speedSum;
    uint8_t  u1_dropMax;        /* worst speed drop of the set, in percent    */
} RepStats_t;

/* Public function prototypes ------------------------------------------------*/
extern void v_repInit(void);
extern void v_repStartSet(void);
extern void v_repStopSet(void);
extern void v_repClearFatigue(void);
extern uint8_t u1_repIsSetActive(void);
extern RepState_t st_repGetState(void);
extern void v_repProcessSample(uint16_t u2_angleDeg, uint32_t u4_timeMs);
extern uint8_t u1_repIsResultReady(void);
extern void v_repClearResultReady(void);
extern const RepResult_t * pst_repGetResult(void);
extern const RepStats_t * pst_repGetStats(void);
extern uint16_t u2_repAverage(uint32_t u4_sum, uint8_t u1_count);

#endif /* APP_REPCOUNT_H */
