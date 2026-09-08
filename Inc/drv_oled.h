/*******************************************************************************
 * File Name    : drv_oled.h
 * Description  : SSD1306 128 x 64 OLED driver over I2C.
 *                Everything is drawn into a frame buffer in RAM first, then
 *                the whole buffer is pushed to the screen in one transfer.
 * Date         : 2026-09-01
 ******************************************************************************/
#ifndef DRV_OLED_H
#define DRV_OLED_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Public define -------------------------------------------------------------*/
#define OLED_WIDTH                  (128u)
#define OLED_HEIGHT                 (64u)
#define OLED_SCALE_SMALL            (1u)
#define OLED_SCALE_BIG              (3u)

/* Public function prototypes ------------------------------------------------*/
extern void v_oledInit(void);
extern void v_oledClear(void);
extern void v_oledDrawString(uint8_t u1_x, uint8_t u1_y, const char * pc_text,
                             uint8_t u1_scale);
extern void v_oledDrawNumber(uint8_t u1_x, uint8_t u1_y, uint16_t u2_value,
                             uint8_t u1_scale);
extern void v_oledDrawSeconds(uint8_t u1_x, uint8_t u1_y, uint16_t u2_milli,
                              uint8_t u1_scale);
extern void v_oledDrawBar(uint8_t u1_x, uint8_t u1_y, uint8_t u1_width,
                          uint8_t u1_height, uint8_t u1_percent);
extern void v_oledFlush(void);

#endif /* DRV_OLED_H */
