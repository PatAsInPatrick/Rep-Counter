/*******************************************************************************
 * File Name    : drv_exti.c
 * Description  : Four switches, four EXTI lines, four interrupt vectors.
 *                Every line is cleared with "=" so that the neighbours inside
 *                the same vector are never disturbed.
 *                Software debounce uses the 1 ms tick of drv_tim.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "drv_exti.h"
#include "drv_tim.h"

/* Private define ------------------------------------------------------------*/
#define EXTI_START_STOP_LINE        (10u)   /* PA10 */
#define EXTI_OVERVIEW_LINE          (3u)    /* PB3  */
#define EXTI_PAGE_LINE              (5u)    /* PB5  */
#define EXTI_CLEAR_LINE             (4u)    /* PB4  */

#define EXTI_PORT_A_CODE            (0x0uL)
#define EXTI_PORT_B_CODE            (0x1uL)
#define EXTI_CR_FIELD_MASK          (0xFuL)
#define EXTI_CR_LINE_PER_REGISTER   (4u)
#define EXTI_CR_BIT_PER_LINE        (4u)

#define EXTI_MODE_MASK              (0x3uL)
#define EXTI_MODE_INPUT             (0x0uL)
#define EXTI_PUPD_PULLUP            (0x1uL)
#define EXTI_MODE_BIT_PER_PIN       (2u)

#define EXTI_DEBOUNCE_MS            (200uL)
#define EXTI_IRQ_PRIORITY           (1u)
#define EXTI_SWITCH_COUNT           (4u)

/* Private variables ---------------------------------------------------------*/
static const uint8_t u1g_extiLine[EXTI_SWITCH_COUNT] =
{
    EXTI_START_STOP_LINE, EXTI_OVERVIEW_LINE, EXTI_PAGE_LINE, EXTI_CLEAR_LINE
};

static volatile uint8_t  u1g_eventFlags = 0u;
static volatile uint32_t u4g_lastEventMs[EXTI_SWITCH_COUNT] = {0uL};

/* Private function prototypes -----------------------------------------------*/
static void v_extiConfigPin(GPIO_TypeDef * pst_port, uint8_t u1_pin);
static void v_extiSelectSource(uint8_t u1_line, uint32_t u4_portCode);
static void v_extiEnableLine(uint8_t u1_line);
static void v_extiHandleLine(uint8_t u1_index);

/* Private functions ---------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_extiConfigPin
 * @brief             - Configure one switch pin as input with pull-up.
 *                      The mode field is written explicitly because some
 *                      pins do not start as plain inputs after reset.
 * @param[in]         - pst_port : GPIO port base address
 * @param[in]         - u1_pin   : pin number 0..15
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_extiConfigPin(GPIO_TypeDef * pst_port, uint8_t u1_pin)
{
    uint32_t u4t_shift = (uint32_t)u1_pin * EXTI_MODE_BIT_PER_PIN;

    pst_port->MODER &= ~(EXTI_MODE_MASK << u4t_shift);
    pst_port->MODER |= (EXTI_MODE_INPUT << u4t_shift);
    pst_port->PUPDR &= ~(EXTI_MODE_MASK << u4t_shift);
    pst_port->PUPDR |= (EXTI_PUPD_PULLUP << u4t_shift);
}

/*********************************************************************
 * @fn                - v_extiSelectSource
 * @brief             - Tell SYSCFG which port drives one EXTI line.
 * @param[in]         - u1_line     : EXTI line number
 * @param[in]         - u4_portCode : 0 = port A, 1 = port B
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_extiSelectSource(uint8_t u1_line, uint32_t u4_portCode)
{
    uint8_t  u1t_register = (uint8_t)(u1_line / EXTI_CR_LINE_PER_REGISTER);
    uint32_t u4t_shift = (uint32_t)(u1_line % EXTI_CR_LINE_PER_REGISTER)
                       * EXTI_CR_BIT_PER_LINE;

    SYSCFG->EXTICR[u1t_register] &= ~(EXTI_CR_FIELD_MASK << u4t_shift);
    SYSCFG->EXTICR[u1t_register] |= (u4_portCode << u4t_shift);
}

/*********************************************************************
 * @fn                - v_extiEnableLine
 * @brief             - Enable the falling edge of one EXTI line.
 * @param[in]         - u1_line : EXTI line number
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_extiEnableLine(uint8_t u1_line)
{
    EXTI->FTSR |= (1uL << u1_line);
    EXTI->RTSR &= ~(1uL << u1_line);
    EXTI->PR = (1uL << u1_line);
    EXTI->IMR |= (1uL << u1_line);
}

/*********************************************************************
 * @fn                - v_extiHandleLine
 * @brief             - Common body of the four interrupt handlers.
 * @param[in]         - u1_index : 0 = start/stop, 1 = overview,
 *                                 2 = next page, 3 = clear fatigue
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
static void v_extiHandleLine(uint8_t u1_index)
{
    uint32_t u4t_now = 0uL;
    uint8_t  u1t_line = u1g_extiLine[u1_index];

    if ((EXTI->PR & (1uL << u1t_line)) != 0uL)
    {
        EXTI->PR = (1uL << u1t_line);

        u4t_now = u4_timGetTickMs();
        if ((u4t_now - u4g_lastEventMs[u1_index]) > EXTI_DEBOUNCE_MS)
        {
            u4g_lastEventMs[u1_index] = u4t_now;
            u1g_eventFlags |= (uint8_t)(1u << u1_index);
        }
    }
}

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_extiButtonInit
 * @brief             - Configure the four switches and their interrupts.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_extiButtonInit(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    v_extiConfigPin(GPIOA, (uint8_t)EXTI_START_STOP_LINE);
    v_extiConfigPin(GPIOB, (uint8_t)EXTI_OVERVIEW_LINE);
    v_extiConfigPin(GPIOB, (uint8_t)EXTI_PAGE_LINE);
    v_extiConfigPin(GPIOB, (uint8_t)EXTI_CLEAR_LINE);

    v_extiSelectSource((uint8_t)EXTI_START_STOP_LINE, EXTI_PORT_A_CODE);
    v_extiSelectSource((uint8_t)EXTI_OVERVIEW_LINE, EXTI_PORT_B_CODE);
    v_extiSelectSource((uint8_t)EXTI_PAGE_LINE, EXTI_PORT_B_CODE);
    v_extiSelectSource((uint8_t)EXTI_CLEAR_LINE, EXTI_PORT_B_CODE);

    v_extiEnableLine((uint8_t)EXTI_START_STOP_LINE);
    v_extiEnableLine((uint8_t)EXTI_OVERVIEW_LINE);
    v_extiEnableLine((uint8_t)EXTI_PAGE_LINE);
    v_extiEnableLine((uint8_t)EXTI_CLEAR_LINE);

    NVIC_SetPriority(EXTI15_10_IRQn, EXTI_IRQ_PRIORITY);
    NVIC_SetPriority(EXTI3_IRQn, EXTI_IRQ_PRIORITY);
    NVIC_SetPriority(EXTI9_5_IRQn, EXTI_IRQ_PRIORITY);
    NVIC_SetPriority(EXTI4_IRQn, EXTI_IRQ_PRIORITY);

    NVIC_EnableIRQ(EXTI15_10_IRQn);
    NVIC_EnableIRQ(EXTI3_IRQn);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
    NVIC_EnableIRQ(EXTI4_IRQn);
}

/*********************************************************************
 * @fn                - u1_extiGetEvents
 * @brief             - Return the pending button events as a bit mask.
 * @return            - uint8_t combination of EXTI_EVENT_xxx
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_extiGetEvents(void)
{
    return u1g_eventFlags;
}

/*********************************************************************
 * @fn                - v_extiClearEvent
 * @brief             - Acknowledge one or more button events.
 * @param[in]         - u1_mask : events to clear
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_extiClearEvent(uint8_t u1_mask)
{
    u1g_eventFlags &= (uint8_t)(~u1_mask);
}

/* Callback functions --------------------------------------------------------*/

/*********************************************************************
 * @fn                - EXTI15_10_IRQHandler
 * @brief             - Interrupt of the start and stop switch on PA10.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void EXTI15_10_IRQHandler(void)
{
    v_extiHandleLine(0u);
}

/*********************************************************************
 * @fn                - EXTI3_IRQHandler
 * @brief             - Interrupt of the overview switch on PB3.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void EXTI3_IRQHandler(void)
{
    v_extiHandleLine(1u);
}

/*********************************************************************
 * @fn                - EXTI9_5_IRQHandler
 * @brief             - Interrupt of the next page switch on PB5.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void EXTI9_5_IRQHandler(void)
{
    v_extiHandleLine(2u);
}

/*********************************************************************
 * @fn                - EXTI4_IRQHandler
 * @brief             - Interrupt of the fatigue baseline switch on PB4.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void EXTI4_IRQHandler(void)
{
    v_extiHandleLine(3u);
}
