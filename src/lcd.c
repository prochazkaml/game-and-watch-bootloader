#include <string.h>
#include <stdint.h>

#include "lcd.h"
#include "stm32.h"
#include "font_basic.h"

uint16_t framebuffer[320 * 240] __attribute__((section (".lcd")));
uint16_t fb_internal[320 * 240] __attribute__((section (".lcd")));

extern DAC_HandleTypeDef hdac1;
extern DAC_HandleTypeDef hdac2;

uint32_t active_framebuffer;

/**
  * @brief  Turn off the backlight.
  * @return Nothing.
  */
void lcd_backlight_off() {
	HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
	HAL_DAC_Stop(&hdac1, DAC_CHANNEL_2);
	HAL_DAC_Stop(&hdac2, DAC_CHANNEL_1);
}

/**
  * @brief  Turn on the backlight.
  * @param  brightness: LCD brightness (128-255 recommended).
  * @return Nothing.
  */
void lcd_backlight_on(uint8_t brightness) {
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_8B_R, brightness);
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_8B_R, brightness);
	HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_8B_R, brightness);

	HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
	HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);
	HAL_DAC_Start(&hdac2, DAC_CHANNEL_1);
}

/**
  * @brief  Fade the working area (320x208+0+16) of the screen.
  * @return Nothing.
  */
void lcd_fade() {
	int i, val, r, g, b;
	
	for(i = 16 * 320; i < 320 * 224; i++) {
		val = framebuffer[i];
		r = val >> 11;
		g = (val >> 5) & 0b111111;
		b = val & 0b11111;
		
		r >>= 2;
		g >>= 2;
		b >>= 2;
		framebuffer[i] = (r << 11) | (g << 5) | b;
	}
}

/**
  * @brief  Draw a window in the middle of the screen.
  * @param  w: Width of the window.
  * @param  h: Height of the window.
  * @return Nothing.
  */
void lcd_draw_window(int w, int h) {
	int x, y;

	w /= 2;
	h /= 2;
	
	for(y = 120 - h; y < 120 + h; y++) {
		for(x = 160 - w; x < 160 + w; x++) {
			framebuffer[x + y * 320] = LCD_COLOR_GRAYSCALE(4);
		}
	}
	
	for(x = 159 - w; x < 160 + w; x++) {
		framebuffer[x + (119 - h) * 320] = 0xFFFF;
		framebuffer[x + (120 + h) * 320] = 0xFFFF;
	}

	for(y = 119 - h; y < 120 + h; y++) {
		framebuffer[159 - w + y * 320] = 0xFFFF;
		framebuffer[160 + w + y * 320] = 0xFFFF;
	}

	
}

/**
  * @brief  Draw a progress bar the screen.
  * @param  step: Current step.
  * @param  total: Total number of steps.
  * @param  x: X position of the progress bar.
  * @param  y: X position of the progress bar.
  * @param  w: Width of the progress bar.
  * @param  h: Height of the progress bar.
  * @return Nothing.
  */
void lcd_draw_progress_bar(int step, int total, int x, int y, int w, int h) {
	int i, j, color;
	
	for(i = 0; i < w; i++) {
		color = (i >= (step * w / total)) ? 0x0000 : 0xFFFF;
		
		for(j = y; j < y + h; j++) {
			framebuffer[x + i + j * 320] = color;
		}
	}
}

/**
  * @brief  Turn off the backlight.
  * @return Nothing.
  
void lcd_backlight_off() {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}*/

/**
  * @brief  Turn on the backlight.
  * @return Nothing.
  
void lcd_backlight_on() {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
}*/

/**
  * @brief  Draw a 64x48 16bpp raw bitmap on the screen.
  * @param  off_x: X position of the bitmap.
  * @param  off_y: X position of the bitmap.
  * @return Nothing.
  */
void lcd_draw_icon(uint16_t *bmp, int off_x, int off_y) {
	int x, y, ptr = 0;

	for(y = 0; y < 48; y++) {
		for(x = 0; x < 64; x++) {
			framebuffer[off_x + x + (off_y + 47 - y) * 320] = bmp[ptr++];
		}
	}
}

/**
  * @brief  Put a character on the screen.
  * @param  c: ASCII code of the character (range 0x20-0x7E).
  * @param  x: X position of the character.
  * @param  y: X position of the character.
  * @param  fg: Character color.
  * @param  bg: Background color.
  * @return Nothing.
  */
void lcd_putchar(unsigned char c, int x, int y, int fg, int bg) {
	int i, j;

	if(c < ' ' || c > '~') return;

	c -= ' ';

	for(j = 0; j < 8; j++) {
		for(i = 0; i < 8; i++) {
			framebuffer[x + i + (y + j) * 320] = (font8x8_basic[c][j] & (1 << i)) ? fg : bg;
		}
	}
}

/**
  * @brief  Put a string on the screen (left align).
  * @param  str: Pointer to a null-terminated ASCII string (range 0x20-0x7E).
  * @param  x: X position of the string.
  * @param  y: X position of the string.
  * @param  fg: Character color.
  * @param  bg: Background color.
  * @return Nothing.
  */
void lcd_print(char *str, int x, int y, int fg, int bg) {
	int og_x = x;

	while(*str) {
		lcd_putchar(*str++, x, y, fg, bg);
		x += 8;

		if(*str == '\n') {
			x = og_x;
			y += 8;
		}
	}
}

/**
  * @brief  Put a string on the screen (center align).
  * @param  str: Pointer to a null-terminated ASCII string (range 0x20-0x7E).
  * @param  x: X position of the string.
  * @param  y: X position of the string.
  * @param  fg: Character color.
  * @param  bg: Background color.
  * @return Nothing.
  */
void lcd_print_centered(char *str, int x, int y, int fg, int bg) {
	lcd_print(str, x - strlen(str) * 4, y, fg, bg);
}

/**
  * @brief  Put a string on the screen (right align).
  * @param  str: Pointer to a null-terminated ASCII string (range 0x20-0x7E).
  * @param  x: X position of the string.
  * @param  y: X position of the string.
  * @param  fg: Character color.
  * @param  bg: Background color.
  * @return Nothing.
  */
void lcd_print_rtl(char *str, int x, int y, int fg, int bg) {
	lcd_print(str, x - strlen(str) * 8, y, fg, bg);
}

/**
  * @brief  Initialize the LCD.
  * @return Nothing.
  */
void lcd_init() {
	int i;

	memset(framebuffer, 0, 320 * 240 * 2);
	memset(fb_internal, 0, 320 * 240 * 2);

	// 3.3v power to display *SET* to disable supply.
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET);


	// TURN OFF CHIP SELECT
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
	// TURN OFF PD8
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
	HAL_Delay(100);

	// Wake
	// Enable 3.3v
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_Delay(1);
	// Enable 1.8V
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET);
	// also assert CS, not sure where to put this yet
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	HAL_Delay(7);

	// Lets go, bootup sequence.
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_SET);
	HAL_Delay(2);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
	HAL_Delay(2);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_SET);

	HAL_Delay(10);

	uint8_t bootup_sequence[20] = {
		0x08, 0x80, 0x6E, 0x80, 0x80, 0x80, 0x68, 0x00,
		0xD0, 0x00, 0x1B, 0x00, 0xE0, 0x00, 0x6A, 0x80,
		0x80, 0x00, 0x14, 0x80
	};
	
	for(i = 0; i < 20; i += 2) {
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
		HAL_Delay(2);
		
		HAL_SPI_Transmit(&hspi2, bootup_sequence + i, 2, 100);
		HAL_Delay(2);		

		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	}
	
	HAL_LTDC_SetAddress(&hltdc, (uint32_t) &fb_internal, 0);

	// Prevents the screen from flashing on bootup.
	HAL_Delay(100);
}

/**
  * @brief  Updates the LCD.
  * @return Nothing.
  */
void lcd_update() {
	memcpy(fb_internal, framebuffer, 320 * 240 * 2);
}

/**
  * @brief  Turns off the LCD.
  * @return Nothing.
  */
void lcd_deinit(SPI_HandleTypeDef *spi) {
	// Chip select low.
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);

	// 3.3v power to display *SET* to disable supply.
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);

	// Disable 1.8v.
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET);

	// Pull reset line(?) low. (Flakey without this)
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
}