/*******************************************************************************
 * File Name    : drv_uart.c
 * Description  : USART2 at 115200 baud on PA2 / PA3 (ST-Link virtual COM).
 *                Every byte leaves through DMA1 Stream6, the CPU only starts
 *                the transfer and is told by interrupt when it has finished.
 * Date         : 2026-09-01
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include "drv_uart.h"

/* Private define ------------------------------------------------------------*/
#define UART_BRR_115200_AT_16MHZ    (0x8Bu)     /* mantissa 8, fraction 11    */
#define UART_DMA_CHANNEL_4          (0x4uL << DMA_SxCR_CHSEL_Pos)
#define UART_DIR_MEM_TO_PERIPH      (0x1uL << DMA_SxCR_DIR_Pos)
#define UART_DMA_IRQ_PRIORITY       (3u)

/* Private variables ---------------------------------------------------------*/
static volatile uint8_t u1g_uartBusy = 0u;

/* Public functions ----------------------------------------------------------*/

/*********************************************************************
 * @fn                - v_uartInit
 * @brief             - Configure USART2 and DMA1 Stream6 for transmission.
 *                      GPIO alternate function must be set first.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_uartInit(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    USART2->BRR = UART_BRR_115200_AT_16MHZ;
    USART2->CR3 |= USART_CR3_DMAT;
    USART2->CR1 |= USART_CR1_TE;
    USART2->CR1 |= USART_CR1_UE;

    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    while ((DMA1_Stream6->CR & DMA_SxCR_EN) != 0uL)
    {
        /* wait until the stream is really disabled before configuring it */
    }

    DMA1->HIFCR = DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6
                | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6;

    DMA1_Stream6->PAR = (uint32_t)(&(USART2->DR));
    DMA1_Stream6->CR = UART_DMA_CHANNEL_4
                     | UART_DIR_MEM_TO_PERIPH
                     | DMA_SxCR_MINC
                     | DMA_SxCR_TCIE;

    NVIC_SetPriority(DMA1_Stream6_IRQn, UART_DMA_IRQ_PRIORITY);
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}

/*********************************************************************
 * @fn                - u1_uartIsBusy
 * @brief             - Report whether a DMA transmission is still running.
 * @return            - uint8_t 1 = busy, 0 = free
 *//////////////////////////////////////////////////////////////////////
uint8_t u1_uartIsBusy(void)
{
    return u1g_uartBusy;
}

/*********************************************************************
 * @fn                - v_uartSend
 * @brief             - Start a DMA transmission. The buffer given by the
 *                      caller must stay valid until the transfer ends.
 * @param[in]         - pu1_data  : pointer to the bytes to send
 * @param[in]         - u2_length : number of bytes
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void v_uartSend(const uint8_t * pu1_data, uint16_t u2_length)
{
    if ((pu1_data != NULL) && (u2_length > 0u) && (u1g_uartBusy == 0u))
    {
        u1g_uartBusy = 1u;

        DMA1_Stream6->CR &= ~DMA_SxCR_EN;
        while ((DMA1_Stream6->CR & DMA_SxCR_EN) != 0uL)
        {
            /* the stream must be idle before a new transfer is programmed */
        }

        DMA1->HIFCR = DMA_HIFCR_CTCIF6;
        DMA1_Stream6->M0AR = (uint32_t)pu1_data;
        DMA1_Stream6->NDTR = (uint32_t)u2_length;
        DMA1_Stream6->CR |= DMA_SxCR_EN;
    }
}

/* Callback functions --------------------------------------------------------*/

/*********************************************************************
 * @fn                - DMA1_Stream6_IRQHandler
 * @brief             - Transmission finished, release the driver.
 * @return            - void
 *//////////////////////////////////////////////////////////////////////
void DMA1_Stream6_IRQHandler(void)
{
    if ((DMA1->HISR & DMA_HISR_TCIF6) != 0uL)
    {
        DMA1->HIFCR = DMA_HIFCR_CTCIF6;
        u1g_uartBusy = 0u;
    }
}
