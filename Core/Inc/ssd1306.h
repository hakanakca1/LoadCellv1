#ifndef __SSD1306_H__
#define __SSD1306_H__

#include "stm32l4xx_hal.h"
#include "i2c.h"

#define SSD1306_I2C_ADDR        (0x3C << 1)
#define SSD1306_WIDTH           128
#define SSD1306_HEIGHT          32

typedef enum { Black = 0, White = 1 } SSD1306_COLOR;

typedef struct {
    const uint8_t width;
    uint8_t height;
    const uint16_t *data;
} FontDef;

extern FontDef Font_7x10; // Font tanımını dışarı açıyoruz

void ssd1306_Init(void);
void ssd1306_Fill(SSD1306_COLOR color);
void ssd1306_UpdateScreen(void);
void ssd1306_SetCursor(uint8_t x, uint8_t y);
void ssd1306_WriteString(char* str, FontDef font, SSD1306_COLOR color);

#endif
