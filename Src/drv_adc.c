/*******************************************************************************
 * File Name    : drv_adc.c
 * Description  : ADC1 converts two channels in one sequence.
 *                  first  : channel 4  = PA4 potentiometer, the arm angle
 *                  second : channel 0  = PA0 thermistor, the room temperature
 *                TIM2 TRGO starts every sequence, DMA2 Stream0 moves the
 *                results into a circular buffer, and the transfer complete
 *                interrupt averages four sequences of each channel.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "drv_adc.h"

/* Private define ------------------------------------------------------------*/
/* --- CHANGE HERE when the potentiometer is on PB1 : channel 9 ------------- */
#define ADC_CHANNEL_ANGLE           (4u)    /* PA4 */
#define ADC_CHANNEL_THERMAL         (0u)    /* PA0 */
#define ADC_SEQUENCE_LENGTH         (2u)
#define ADC_AVERAGE_COUNT           (4u)
#define ADC_BUFFER_LENGTH           (ADC_SEQUENCE_LENGTH * ADC_AVERAGE_COUNT)
#define ADC_BUFFER_SHIFT            (2u)    /* divide by four using a shift   */

#define ADC_SMP_480_CYCLES          (0x7uL)
#define ADC_SMPR2_SHIFT_CH4         (12u)   /* SMP4 field position            */
#define ADC_SMPR2_SHIFT_CH0         (0u)    /* SMP0 field position            */
#define ADC_SQR3_SHIFT_SQ2          (5u)
#define ADC_SQR1_SHIFT_L            (20u)

#define ADC_EXTSEL_TIM2_TRGO        (0x6uL << ADC_CR2_EXTSEL_Pos)
#define ADC_EXTEN_RISING            (0x1uL << ADC_CR2_EXTEN_Pos)

#define DMA_CHANNEL_0               (0x0uL << DMA_SxCR_CHSEL_Pos)
#define DMA_SIZE_HALFWORD           (0x1uL)

#define ADC_DMA_IRQ_PRIORITY        (3u)
#define ADC_WATCHDOG_IRQ_PRIORITY   (0u)    /* highest, this is the brake     */
#define ADC_WATCHDOG_HIGH_LIMIT     (4095u) /* only the low side is guarded   */
#define ADC_CR1_AWDCH_THERMAL       ((uint32_t)ADC_CHANNEL_THERMAL)

/* Private variables ---------------------------------------------------------*/
static volatile uint16_t u2g_adcBuffer[ADC_BUFFER_LENGTH] = {0u};
static volatile uint16_t u2g_adcAngle = 0u;
static volatile uint16_t u2g_adcThermal = 0u;
static volatile uint8_t  u1g_adcSampleReady = 0u;

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_adcWatchdogInit
 * @brief             - Set up the analog watchdog on the thermistor channel.
 *                      The thermistor resistance falls when it gets warmer,
 *                      so a hot room produces a low count. The alarm is
 *                      therefore placed on the lower limit.
 *                      Call this after v_adcInit.
 * @param[in]         - u2_lowLimit : count below which the alarm fires
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_adcWatchdogInit(uint16_t u2_lowLimit)
{
    ADC1->LTR = (uint32_t)u2_lowLimit;
    ADC1->HTR = (uint32_t)ADC_WATCHDOG_HIGH_LIMIT;

    /* guard one single channel, not the whole sequence */
    ADC1->CR1 &= ~ADC_CR1_AWDCH;
    ADC1->CR1 |= ADC_CR1_AWDCH_THERMAL;
    ADC1->CR1 |= ADC_CR1_AWDSGL;
    ADC1->CR1 |= ADC_CR1_AWDEN;

    NVIC_SetPriority(ADC_IRQn, ADC_WATCHDOG_IRQ_PRIORITY);
    NVIC_EnableIRQ(ADC_IRQn);

    v_adcWatchdogArm();
}

/*********************************************************************
 * @fn                - v_adcWatchdogArm
 * @brief             - Allow the watchdog to interrupt again. The handler
 *                      switches the interrupt off after it has fired, so
 *                      that a hot room cannot flood the processor, and the
 *                      application arms it again once the room has cooled.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_adcWatchdogArm(void)
{
    ADC1->SR &= ~ADC_SR_AWD;
    ADC1->CR1 |= ADC_CR1_AWDIE;
}

/*********************************************************************
 * @fn                - v_adcInit
 * @brief             - Configure DMA2 Stream0 and ADC1 for triggered
 *                      conversion. GPIO analog mode must be set first.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_adcInit(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* --- DMA2 Stream0 Channel0 = ADC1, peripheral to memory, circular ---- */
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    while ((DMA2_Stream0->CR & DMA_SxCR_EN) != 0uL)
    {
        /* wait until the stream is really disabled before configuring it */
    }

    DMA2->LIFCR = DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0
                | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0;

    DMA2_Stream0->PAR = (uint32_t)(&(ADC1->DR));
    DMA2_Stream0->M0AR = (uint32_t)(&u2g_adcBuffer[0]);
    DMA2_Stream0->NDTR = (uint32_t)ADC_BUFFER_LENGTH;

    DMA2_Stream0->CR = DMA_CHANNEL_0
                     | (DMA_SIZE_HALFWORD << DMA_SxCR_MSIZE_Pos)
                     | (DMA_SIZE_HALFWORD << DMA_SxCR_PSIZE_Pos)
                     | DMA_SxCR_MINC
                     | DMA_SxCR_CIRC
                     | DMA_SxCR_TCIE;

    NVIC_SetPriority(DMA2_Stream0_IRQn, ADC_DMA_IRQ_PRIORITY);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    DMA2_Stream0->CR |= DMA_SxCR_EN;

    /* --- ADC1 : one channel, triggered by TIM2 TRGO ---------------------- */
    ADC1->SMPR2 &= ~(0x7uL << ADC_SMPR2_SHIFT_CH4);
    ADC1->SMPR2 |= (ADC_SMP_480_CYCLES << ADC_SMPR2_SHIFT_CH4);
    ADC1->SMPR2 &= ~(0x7uL << ADC_SMPR2_SHIFT_CH0);
    ADC1->SMPR2 |= (ADC_SMP_480_CYCLES << ADC_SMPR2_SHIFT_CH0);

    /* two conversions per trigger, the angle first and the thermistor next */
    ADC1->SQR1 &= ~ADC_SQR1_L;
    ADC1->SQR1 |= (((uint32_t)ADC_SEQUENCE_LENGTH - 1uL) << ADC_SQR1_SHIFT_L);
    ADC1->SQR3 = (uint32_t)ADC_CHANNEL_ANGLE
               | ((uint32_t)ADC_CHANNEL_THERMAL << ADC_SQR3_SHIFT_SQ2);

    ADC1->CR1 |= ADC_CR1_SCAN;

    ADC1->CR2 &= ~ADC_CR2_EXTSEL;
    ADC1->CR2 |= ADC_EXTSEL_TIM2_TRGO;
    ADC1->CR2 &= ~ADC_CR2_EXTEN;
    ADC1->CR2 |= ADC_EXTEN_RISING;
    ADC1->CR2 |= ADC_CR2_DMA;
    ADC1->CR2 |= ADC_CR2_DDS;
    ADC1->CR2 |= ADC_CR2_ADON;
}

/*********************************************************************
 * @fn                - u1_adcIsSampleReady
 * @brief             - Tell the application that a filtered value is ready.
 * @return            - uint8_t 1 = ready, 0 = not ready
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_adcIsSampleReady(void)
{
    return u1g_adcSampleReady;
}

/*********************************************************************
 * @fn                - v_adcClearSampleReady
 * @brief             - Acknowledge the ready flag.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_adcClearSampleReady(void)
{
    u1g_adcSampleReady = 0u;
}

/*********************************************************************
 * @fn                - u2_adcGetAngleRaw
 * @brief             - Average of the last four readings of the angle.
 * @return            - uint16_t raw ADC count 0..4095
 *//////////////////////////////////////////////////////////////////////
uint16_t u2_adcGetAngleRaw(void)
{
    return u2g_adcAngle;
}

/*********************************************************************
 * @fn                - u2_adcGetThermalRaw
 * @brief             - Average of the last four readings of the thermistor.
 * @return            - uint16_t raw ADC count 0..4095
 *//////////////////////////////////////////////////////////////////////
uint16_t u2_adcGetThermalRaw(void)
{
    return u2g_adcThermal;
}

/* Callback functions --------------------------------------------------------*/

/*********************************************************************
 * @fn                - ADC_IRQHandler
 * @brief             - Emergency path. The comparison was done by the ADC
 *                      itself, so reaching this point already means the
 *                      temperature is out of range. The handler switches its
 *                      own interrupt off and hands over to the application
 *                      hook, which shuts the training down immediately.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void ADC_IRQHandler(void)
{
    if ((ADC1->SR & ADC_SR_AWD) != 0uL)
    {
        ADC1->SR &= ~ADC_SR_AWD;
        ADC1->CR1 &= ~ADC_CR1_AWDIE;

        v_adcWatchdogHook();
    }
}

/*********************************************************************
 * @fn                - DMA2_Stream0_IRQHandler
 * @brief             - Transfer complete : average the buffer, raise flag.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void DMA2_Stream0_IRQHandler(void)
{
    uint32_t u4t_angleSum = 0uL;
    uint32_t u4t_thermalSum = 0uL;
    uint8_t  u1t_index = 0u;

    if ((DMA2->LISR & DMA_LISR_TCIF0) != 0uL)
    {
        DMA2->LIFCR = DMA_LIFCR_CTCIF0;

        for (u1t_index = 0u; u1t_index < (uint8_t)ADC_BUFFER_LENGTH;
             u1t_index = (uint8_t)(u1t_index + ADC_SEQUENCE_LENGTH))
        {
            u4t_angleSum += (uint32_t)u2g_adcBuffer[u1t_index];
            u4t_thermalSum += (uint32_t)u2g_adcBuffer[u1t_index + 1u];
        }

        u2g_adcAngle = (uint16_t)(u4t_angleSum >> ADC_BUFFER_SHIFT);
        u2g_adcThermal = (uint16_t)(u4t_thermalSum >> ADC_BUFFER_SHIFT);
        u1g_adcSampleReady = 1u;
    }
}
