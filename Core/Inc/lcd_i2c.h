#ifndef LCD_I2C_H
#define LCD_I2C_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

#define LCD_I2C_ADDR    (0x27 << 1)

#define LCD_RS          0x01
#define LCD_RW          0x02
#define LCD_EN          0x04
#define LCD_BL          0x08

#define LCD_CLEAR       0x01
#define LCD_HOME        0x02
#define LCD_ENTRY_MODE  0x06
#define LCD_DISPLAY_ON  0x0C
#define LCD_FUNCTION_4B 0x28

void LCD_Init(I2C_HandleTypeDef *hi2c);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Print(const char *str);
void LCD_PrintChar(char c);
void LCD_Backlight(uint8_t on);

#endif