/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

#include <stdio.h>
#include <string.h>
#include "stm32h7xx_hal.h"
#include "stm32.h"
#include "gwloader.h"
#include "buttons.h"
#include "flash.h"
#include "lcd.h"
#include "common.h"

#include "mainmenu.h"


void draw_window(int w, int h) {
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

void draw_progress_bar(int step, int total, int x, int y, int w, int h) {
	int i, j, color;
	
	for(i = 0; i < w; i++) {
		color = (i >= (step * w / total)) ? 0x0000 : 0xFFFF;
		
		for(j = y; j < y + h; j++) {
			framebuffer[x + i + j * 320] = color;
		}
	}
}

void start_flash_process(int selection) {
	// Fade the screen
	
	int i, j, val, r, g, b;
	
	for(i = 0; i < 320 * 240; i++) {
		val = framebuffer[i];
		r = val >> 11;
		g = (val >> 5) & 0b111111;
		b = val & 0b11111;
		
		r >>= 3;
		g >>= 3;
		b >>= 3;
		framebuffer[i] = (r << 11) | (g << 5) | b;
	}
	
	// Wait for any processes to finish
	
	draw_window(200, 32);
	lcd_print_centered("Please wait...", 160, 116, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	lcd_update();

	mainmenu_finish();
	
	// Enter the correct directory and open the internal flash file
	
	gwloader_comm_buf_word[3] = (uint32_t) directory_names[selection];
	gwloader_call(0x0C);
	
	gwloader_comm_buf_word[3] = (uint32_t) intflash_name;
	
	if(gwloader_call_catcherr(0x02)) {
		gwloader_comm_buf_word[3] = (uint32_t) updir_name;
		gwloader_call(0x0C);

		draw_window(240, 40);
		lcd_print_centered("This homebrew is corrupted.", 160, 112, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		lcd_print_centered("MAIN.BIN is missing.", 160, 120, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		lcd_update();
		
		while(!buttons_get());
		return;
	}
	
	char buffer[64];
	
	int size = gwloader_comm_buf_word[1];
	int sectors = size / 0x2000;
	if(size % 0x2000) sectors++;
	
	draw_window(280, 64);
	lcd_print_centered("Erasing internal flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	lcd_update();
	
	// Erase the internal flash
	
	HAL_FLASH_Unlock();

	static FLASH_EraseInitTypeDef EraseInitStruct;
	
	EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
	EraseInitStruct.Banks         = FLASH_BANK_1;
	EraseInitStruct.Sector        = 0;
	EraseInitStruct.NbSectors     = sectors;

	if (HAL_FLASHEx_Erase(&EraseInitStruct, NULL) != HAL_OK) {
		Error_Handler();
	}

	// Write the internal flash
	
	draw_window(280, 64);
	lcd_print_centered("Writing internal flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	sprintf(buffer, "(%d bytes, %d 8k sectors)", size, sectors);
	lcd_print_centered(buffer, 160, 108, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	
	gwloader_comm_buf_word[2] = 0x2000;
	gwloader_comm_buf_word[3] = (uint32_t) data_buffer;
	
	for(i = 0; i < sectors; i++) {
		draw_progress_bar(i, sectors, 60, 124, 200, 16);
		lcd_update();
		
		gwloader_call(0x04);
		
		for(j = 0; j < 0x2000; j += 16) {
			if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, 0x08000000 + i * 0x2000 + j, ((uint32_t)(data_buffer + j))) != HAL_OK) {
				Error_Handler();
			}
		}
	}
	
	draw_progress_bar(sectors, sectors, 60, 124, 200, 16);
	lcd_update();
	
	gwloader_call(0x06);
	HAL_FLASH_Lock();
	
	// Write the external flash, if required
	
	gwloader_comm_buf_word[3] = (uint32_t) extflash_name;
	
	if(!gwloader_call_catcherr(0x02)) {
		draw_window(280, 64);
		lcd_print_centered("Erasing external flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		lcd_update();
		
		OSPI_Init(&hospi1, SPI_MODE, VENDOR_MX);
				
		OSPI_NOR_WriteEnable(&hospi1);
		OSPI_ChipErase(&hospi1);
		
		size = gwloader_comm_buf_word[1];
		sectors = size / 0x10000;
		if(size % 0x10000) sectors++;
		
		draw_window(280, 64);
		lcd_print_centered("Writing external flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		sprintf(buffer, "(%d bytes, %d 64k sectors)", size, sectors);
		lcd_print_centered(buffer, 160, 108, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		
		gwloader_comm_buf_word[2] = 0x10000;
		gwloader_comm_buf_word[3] = (uint32_t) data_buffer;
		
		for(i = 0; i < sectors; i++) {
			draw_progress_bar(i, sectors, 60, 124, 200, 16);
			lcd_update();
			
			gwloader_call(0x04);
			
//			OSPI_BlockErase(&hospi1, i * 0x10000);
			OSPI_Program(&hospi1, i * 0x10000, data_buffer, 0x10000);
		}
		
		draw_progress_bar(sectors, sectors, 60, 124, 200, 16);
		lcd_update();
		
		gwloader_call(0x06);
	}
	
	draw_window(280, 64);
	lcd_print_centered("Rebooting...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	lcd_update();
	
	gwloader_call(0x7F);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main() {
	int card_capacity_mb, card_free_mb, maxselection = 0;
	
	// Reset of all peripherals, Initializes the Flash interface and the Systick.

	HAL_Init();

	HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1_LOW);

	// Configure the system clock

	SystemClock_Config();

	// Initialize all configured peripherals

	MX_GPIO_Init();
	MX_LTDC_Init();
	MX_SPI2_Init();
	MX_OCTOSPI1_Init();

	// Initialize interrupts

	MX_NVIC_Init();

	// Initialize LCD
	
	lcd_init(&hspi2, &hltdc);
	lcd_backlight_on();

	// Get SD card capacity
	
	lcd_print_centered("Detecting SD card...", 160, 116, 0xFFFF, 0x0000);
	lcd_update();

	gwloader_call(0x01);

	uint64_t tmp;
	
	tmp = gwloader_comm_buf_word[2];
	tmp = tmp * 512 / 1000000;
	card_capacity_mb = tmp;

	tmp = gwloader_comm_buf_word[3];
	tmp = tmp * 512 / 1000000;
	card_free_mb = tmp;

	// Load the directory
	
	lcd_print_centered("Loading root directory...", 160, 116, 0xFFFF, 0x0000);
	lcd_update();

	gwloader_comm_buf_word[3] = (uint32_t) dir_buffer;
	gwloader_call(0x0D);

	// Parse the directory
	
	uint8_t c;
	uint8_t *ptr = dir_buffer;
	
	while((c = *ptr)) {
		if(c == 2) {
			directory_names[maxselection] = ptr + 1;
			maxselection++;
		}
		
		*ptr = 0;
		ptr++;
		
		while(*ptr >= 0x20) ptr++;
	}
	
	if(maxselection == 0) {
		lcd_print_centered("There is no homebrew on the SD card.", 160, 108, 0xFFFF, 0x0000);
		lcd_print_centered("Please press the power button", 160, 116, 0xFFFF, 0x0000);
		lcd_print_centered("to turn off the device.", 160, 124, 0xFFFF, 0x0000);
		lcd_update();
		while(1) buttons_get();
	}
	
	initmenu(maxselection, card_capacity_mb, card_free_mb);
	
	while (1) {
		int selection = mainmenu();
		
		if(selection >= 0) start_flash_process(selection);
	}
}
