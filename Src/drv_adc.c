/*******************************************************************************
 * File Name    : drv_adc.c
 * Description  : ADC1 channel 4 (PA4 potentiometer).
 *                TIM2 TRGO starts every conversion, DMA2 Stream0 moves the
 *                result into a circular buffer, the DMA transfer complete
 *                interrupt averages the buffer and raises a flag.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "drv_adc.h"

/* Private define ------------------------------------------------------------*/
/* --- CHANGE HERE when the potentiometer is on PB1 : channel 9 ------------- */
#define ADC_CHANNEL_NUMBER          (4u)
#define ADC_BUFFER_LENGTH           (4u)
#define ADC_BUFFER_SHIFT            (2u)    /* divide by 4 using a shift      */

#define ADC_SMP_480_CYCLES          (0x7uL)
#define ADC_SMPR2_SHIFT_CH4         (12u)   /* SMP4 field position            */

#define ADC_EXTSEL_TIM2_TRGO        (0x6uL << ADC_CR2_EXTSEL_Pos)
#define ADC_EXTEN_RISING            (0x1uL << ADC_CR2_EXTEN_Pos)

#define DMA_CHANNEL_0               (0x0uL << DMA_SxCR_CHSEL_Pos)
#define DMA_SIZE_HALFWORD           (0x1uL)

#define ADC_DMA_IRQ_PRIORITY        (2u)

/* Private variables ---------------------------------------------------------*/
static volatile uint16_t u2g_adcBuffer[ADC_BUFFER_LENGTH] = {0u};
static volatile uint16_t u2g_adcFiltered = 0u;
static volatile uint8_t  u1g_adcSampleReady = 0u;

/* Public functions ----------------------------------------------------------*/

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

    ADC1->SQR1 &= ~ADC_SQR1_L;                      /* one conversion only   */
    ADC1->SQR3 = (uint32_t)ADC_CHANNEL_NUMBER;

    ADC1->CR1 &= ~ADC_CR1_SCAN;

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
 * @fn                - u2_adcGetFiltered
 * @brief             - Return the average of the last four conversions.
 * @return            - uint16_t raw ADC count 0..4095
 *//////////////////////////////////////////////////////////////////////
uint16_t u2_adcGetFiltered(void)
{
    return u2g_adcFiltered;
}

/* Callback functions --------------------------------------------------------*/

/*********************************************************************
 * @fn                - DMA2_Stream0_IRQHandler
 * @brief             - Transfer complete : average the buffer, raise flag.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void DMA2_Stream0_IRQHandler(void)
{
    uint32_t u4t_sum = 0uL;
    uint8_t  u1t_index = 0u;

    if ((DMA2->LISR & DMA_LISR_TCIF0) != 0uL)
    {
        DMA2->LIFCR = DMA_LIFCR_CTCIF0;

        for (u1t_index = 0u; u1t_index < (uint8_t)ADC_BUFFER_LENGTH; u1t_index++)
        {
            u4t_sum += (uint32_t)u2g_adcBuffer[u1t_index];
        }

        u2g_adcFiltered = (uint16_t)(u4t_sum >> ADC_BUFFER_SHIFT);
        u1g_adcSampleReady = 1u;
    }
}
