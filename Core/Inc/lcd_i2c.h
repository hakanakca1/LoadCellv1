#ifndef INC_LCD_I2C_H_
#define INC_LCD_I2C_H_

#include "stm32l4xx_hal.h"

// buradaki ismin tamamen küçük harf olduğuna dikkat et
#define lcd_addr 0x4e

void lcd_init(void);
void lcd_send_cmd(char cmd);
void lcd_send_data(char data);
void lcd_send_string(char *str);
void lcd_put_cur(int row, int col);
void lcd_clear(void);
void lcd_delay_us(uint16_t us);

#endif
