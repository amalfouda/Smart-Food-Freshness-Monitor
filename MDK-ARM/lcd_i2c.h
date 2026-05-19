#ifndef LCD_I2C_H
#define LCD_I2C_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

/* -- I2C address ----------------------------------------------------------- */
#define LCD_I2C_ADDR    (0x27 << 1)   /* CubeMX HAL needs 8-bit shifted addr */

/* -- PCF8574 backpack bit positions ---------------------------------------- */
#define LCD_RS          0x01          /* Register Select: 0 = command, 1 = data */
#define LCD_RW          0x02          /* Read/Write: always 0 (write) for us    */
#define LCD_EN          0x04          /* Enable pulse                           */
#define LCD_BL          0x08          /* Backlight                              */

/* -- Common LCD commands --------------------------------------------------- */
#define LCD_CLEAR       0x01
#define LCD_HOME        0x02
#define LCD_ENTRY_MODE  0x06          /* Increment cursor, no display shift     */
#define LCD_DISPLAY_ON  0x0C          /* Display on, cursor off, blink off      */
#define LCD_FUNCTION_4B 0x28          /* 4-bit, 2 lines, 5x8 font              */

/* -- Public API ------------------------------------------------------------ */
void LCD_Init(I2C_HandleTypeDef *hi2c);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Print(const char *str);
void LCD_PrintChar(char c);
void LCD_Backlight(uint8_t on);

#endif /* LCD_I2C_H */