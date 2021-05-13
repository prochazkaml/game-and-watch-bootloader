#include "stm32h7xx_hal.h"

extern uint16_t framebuffer[320 * 240]  __attribute__((section (".lcd")));

#define LCD_COLOR_GRAYSCALE(level) (((level) << 11) | ((level) << 6) | (level))

void lcd_init();
void lcd_update();
void lcd_backlight_on(uint8_t brightness);
void lcd_backlight_off();
void lcd_backlight_level(uint8_t level);

void lcd_fade();
void lcd_draw_window(int w, int h);
void lcd_draw_progress_bar(int step, int total, int x, int y, int w, int h);
void lcd_putchar(unsigned char c, int x, int y, int fg, int bg);
void lcd_print(char *str, int x, int y, int fg, int bg);
void lcd_print_centered(char *str, int x, int y, int fg, int bg);
void lcd_print_rtl(char *str, int x, int y, int fg, int bg);
void lcd_draw_icon(uint16_t *bmp, int off_x, int off_y);
void lcd_deinit(SPI_HandleTypeDef *spi);
