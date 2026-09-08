/*******************************************************************************
 * File Name    : drv_gpio.h
 * Description  : GPIO driver - LED, button pin, analog pin, UART pin.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef DRV_GPIO_H
#define DRV_GPIO_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Public define -------------------------------------------------------------*/
/* Public function prototypes ------------------------------------------------*/
extern void v_gpioAnalogInit(void);
extern void v_gpioUartPinInit(void);

#endif /* DRV_GPIO_H */
