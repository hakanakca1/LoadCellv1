#include "lcd_i2c.h"

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim4;

// hassas mikrosaniye gecikmesi
void lcd_delay_us(uint16_t us)
{
	__HAL_TIM_SET_COUNTER(&htim4, 0);       // __hal_tim_set_counter değil
	    HAL_TIM_Base_Start(&htim4);             // hal_tim_base_start değil
	    while (__HAL_TIM_GET_COUNTER(&htim4) < us); // __hal_tim_get_counter değil
	    HAL_TIM_Base_Stop(&htim4);
}

void lcd_send_cmd(char cmd) {
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (cmd & 0xf0);
    data_l = ((cmd << 4) & 0xf0);

    data_t[0] = data_u | 0x0c;  // en=1, rs=0
    data_t[1] = data_u | 0x08;  // en=0, rs=0
    data_t[2] = data_l | 0x0c;  // en=1, rs=0
    data_t[3] = data_l | 0x08;  // en=0, rs=0

    HAL_I2C_Master_Transmit(&hi2c1, lcd_addr, (uint8_t *)data_t, 4, 100);

    // komut tipine göre hassas bekleme
    if (cmd == 0x01 || cmd == 0x02) {
        lcd_delay_us(2000);
    } else {
        lcd_delay_us(50);
    }
}

void lcd_send_data(char data) {
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (data & 0xf0);
    data_l = ((data << 4) & 0xf0);

    data_t[0] = data_u | 0x0d;  // en=1, rs=1
    data_t[1] = data_u | 0x09;  // en=0, rs=1
    data_t[2] = data_l | 0x0d;  // en=1, rs=1
    data_t[3] = data_l | 0x09;  // en=0, rs=1

    HAL_I2C_Master_Transmit(&hi2c1, lcd_addr, (uint8_t *)data_t, 4, 100);
    lcd_delay_us(50); // her karakterden sonra kısa bekleme şart
}

void lcd_init(void) {
	HAL_Delay(100);
    lcd_send_cmd(0x30);
    HAL_Delay(10);
    lcd_send_cmd(0x30);
    HAL_Delay(10);
    lcd_send_cmd(0x30);
    HAL_Delay(20);
    lcd_send_cmd(0x20);
    HAL_Delay(20);

    lcd_send_cmd(0x28);
    lcd_send_cmd(0x08);
    lcd_send_cmd(0x01);
    lcd_send_cmd(0x06);
    lcd_send_cmd(0x0c);
}

void lcd_send_string(char *str) {
    while (*str) lcd_send_data(*str++);
}

void lcd_put_cur(int row, int col) {
    switch (row) {
        case 0: col |= 0x80; break;
        case 1: col |= 0xc0; break;
    }
    lcd_send_cmd(col);
}

void lcd_clear(void) {
    lcd_send_cmd(0x01);
}
