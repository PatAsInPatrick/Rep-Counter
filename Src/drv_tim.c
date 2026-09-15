/*******************************************************************************
 * File Name    : drv_tim.c
 * Description  : TIM2 generates a 100 Hz TRGO used as the ADC trigger.
 *                TIM3 generates a 1 ms interrupt used as the system time base.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "drv_tim.h"

/* Private define ------------------------------------------------------------*/
/* System clock after reset = HSI = 16 MHz, APB1 timer clock = 16 MHz         */
#define TIM_TRIGGER_PSC             (160u - 1u)     /* 16 MHz / 160  = 100 kHz */
#define TIM_TRIGGER_ARR             (1000u - 1u)    /* 100 kHz / 1000 = 100 Hz */
#define TIM_TICK_PSC                (16u - 1u)      /* 16 MHz / 16   = 1 MHz   */
#define TIM_TICK_ARR                (1000u - 1u)    /* 1 MHz / 1000  = 1 kHz   */

#define TIM_MMS_UPDATE              (0x2uL << TIM_CR2_MMS_Pos)

#define TIM_TICK_IRQ_PRIORITY       (1u)

/* Private variables ---------------------------------------------------------*/
static volatile uint32_t u4g_tickMs = 0u;

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_timTriggerInit
 * @brief             - TIM2 update event is exported as TRGO at 100 Hz.
 *                      The ADC uses this signal, so the CPU never has to
 *                      poll or start a conversion by software.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_timTriggerInit(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->PSC = TIM_TRIGGER_PSC;
    TIM2->ARR = TIM_TRIGGER_ARR;

    TIM2->CR2 &= ~TIM_CR2_MMS;
    TIM2->CR2 |= TIM_MMS_UPDATE;

    TIM2->EGR |= TIM_EGR_UG;
    TIM2->CR1 |= TIM_CR1_CEN;
}

/*********************************************************************
 * @fn                - v_timTickInit
 * @brief             - TIM3 raises an update interrupt every 1 ms.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_timTickInit(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    TIM3->PSC = TIM_TICK_PSC;
    TIM3->ARR = TIM_TICK_ARR;
    TIM3->EGR |= TIM_EGR_UG;
    TIM3->SR = 0u;
    TIM3->DIER |= TIM_DIER_UIE;

    NVIC_SetPriority(TIM3_IRQn, TIM_TICK_IRQ_PRIORITY);
    NVIC_EnableIRQ(TIM3_IRQn);

    TIM3->CR1 |= TIM_CR1_CEN;
}

/*********************************************************************
 * @fn                - u4_timGetTickMs
 * @brief             - Return the number of milliseconds since start-up.
 * @return            - uint32_t millisecond counter
 *//////////////////////////////////////////////////////////////////////
uint32_t u4_timGetTickMs(void)
{
    return u4g_tickMs;
}

/* Callback functions --------------------------------------------------------*/

/*********************************************************************
 * @fn                - TIM3_IRQHandler
 * @brief             - 1 ms interrupt service routine.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void TIM3_IRQHandler(void)
{
    if ((TIM3->SR & TIM_SR_UIF) != 0uL)
    {
        TIM3->SR &= ~TIM_SR_UIF;
        u4g_tickMs++;
    }
}
