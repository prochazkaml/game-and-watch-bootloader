#include "stm32h7xx_hal.h"

extern uint16_t framebuffer[320 * 240]  __attribute__((section (".lcd")));

#define GFX_MAX_WIDTH 320

#define LCD_COLOR_GRAYSCALE(level) (((level) << 11) | ((level) << 6) | (level))

void lcd_init(SPI_HandleTypeDef *spi, LTDC_HandleTypeDef *ltdc);
void lcd_update();
void lcd_backlight_on();
void lcd_backlight_off();

void lcd_putchar(unsigned char c, int x, int y, int fg, int bg);
void lcd_print(char *str, int x, int y, int fg, int bg);
void lcd_print_centered(char *str, int x, int y, int fg, int bg);
void lcd_print_rtl(char *str, int x, int y, int fg, int bg);
