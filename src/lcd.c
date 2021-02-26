#include <string.h>
#include "lcd.h"
#include "main.h"
#include "font_basic.h"

uint16_t framebuffer[320 * 240];
uint16_t fb_internal[320 * 240] __attribute__((section (".lcd")));

void lcd_backlight_off() {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}
void lcd_backlight_on() {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
}

void lcd_putchar(unsigned char c, int x, int y, int fg, int bg) {
	int i, j;

	c -= ' ';

	for(j = 0; j < 8; j++) {
		for(i = 0; i < 8; i++) {
			framebuffer[x + i + (y + j) * 320] = (font8x8_basic[c][j] & (1 << i)) ? fg : bg;
		}
	}
}

void lcd_print(char *str, int x, int y, int fg, int bg) {
	while(*str) {
		lcd_putchar(*str++, x, y, fg, bg);
		x += 8;
	}
}

void lcd_print_centered(char *str, int x, int y, int fg, int bg) {
	lcd_print(str, x - strlen(str) * 4, y, fg, bg);
}

void lcd_print_rtl(char *str, int x, int y, int fg, int bg) {
	lcd_print(str, x - strlen(str) * 8, y, fg, bg);
}

void lcd_init(SPI_HandleTypeDef *spi, LTDC_HandleTypeDef *ltdc) {
	memset(framebuffer, 0, 320 * 240 * 2);
	memset(fb_internal, 0, 320 * 240 * 2);

	// Turn display *off* completely.
	lcd_backlight_off();

	// 3.3v power to display *SET* to disable supply.
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET);


	// TURN OFF CHIP SELECT
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	// TURN OFF PD8
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);

	// Turn off CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(100);

	lcd_backlight_on();


	// Wake
	// Enable 3.3v
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_Delay(1);
	// Enable 1.8V
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET);
	// also assert CS, not sure where to put this yet
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	HAL_Delay(7);



	// HAL_SPI_Transmit(spi, "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55", 10, 100);
	// Lets go, bootup sequence.
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_SET);
	HAL_Delay(2);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_SET);

	HAL_Delay(10);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_SPI_Transmit(spi, (uint8_t *)"\x08\x80", 2, 100);
	HAL_Delay(2);

	// CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	// HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_SPI_Transmit(spi, (uint8_t *)"\x6E\x80", 2, 100);
	HAL_Delay(2);
	// CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	// HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_SPI_Transmit(spi, (uint8_t *)"\x80\x80", 2, 100);

	HAL_Delay(2);
	// CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	// HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_SPI_Transmit(spi, (uint8_t *)"\x68\x00", 2, 100);
	HAL_Delay(2);
	// CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	// HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_SPI_Transmit(spi, (uint8_t *)"\xd0\x00", 2, 100);
	HAL_Delay(2);
	// CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	// HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_SPI_Transmit(spi, (uint8_t *)"\x1b\x00", 2, 100);

	HAL_Delay(2);
	// CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	// HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_SPI_Transmit(spi, (uint8_t *)"\xe0\x00", 2, 100);


	HAL_Delay(2);
	// CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	// HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_SPI_Transmit(spi, (uint8_t *)"\x6a\x80", 2, 100);

	HAL_Delay(2);
	// CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	// HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_SPI_Transmit(spi, (uint8_t *)"\x80\x00", 2, 100);
	HAL_Delay(2);
	// CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	// HAL_Delay(100);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_SPI_Transmit(spi, (uint8_t *)"\x14\x80", 2, 100);
	HAL_Delay(2);
	// CS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);


	HAL_LTDC_SetAddress(ltdc,(uint32_t) &fb_internal, 0);
}
/*
void lcd_update_imm(LTDC_HandleTypeDef *ltdc) {
	HAL_LTDC_Reload(ltdc, LTDC_RELOAD_VERTICAL_BLANKING);
	curr_fb ^= 1;
	framebuffer = framebuffers[curr_fb];
}

void lcd_update(LTDC_HandleTypeDef *ltdc) {
	lcd_update_imm(ltdc);
	HAL_Delay(20);
}
*/

void lcd_update() {
	memcpy(fb_internal, framebuffer, 320 * 240 * 2);
}
