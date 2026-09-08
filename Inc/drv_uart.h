/*******************************************************************************
 * File Name    : drv_uart.h
 * Description  : USART2 driver, transmit through DMA1 Stream6 (no polling).
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef DRV_UART_H
#define DRV_UART_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Public function prototypes ------------------------------------------------*/
extern void v_uartInit(void);
extern uint8_t u1_uartIsBusy(void);
extern void v_uartSend(const uint8_t * pu1_data, uint16_t u2_length);

#endif /* DRV_UART_H */
