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

#include "submenu.h"
#include "mainmenu.h"
#include "flashload.h"

/**
  * @brief  The application entry point.
  * @return Nothing.
  */
int main() {
	int card_capacity_mb, card_free_mb, direntries = 0;
	
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
	
	lcd_init();
	lcd_backlight_on();

	// Get SD card capacity
	
	lcd_print_centered("Detecting SD card...", 160, 116, 0xFFFF, 0x0000);
	lcd_update();

	gwloader_call(0x01);

	// Convert sectors to megabytes
	
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
	
	uint8_t *ptr = dir_buffer;
	
	while(*ptr) {
		if((*ptr) == 2) {
			directory_names[direntries] = ptr + 1;
			direntries++;
		}
		
		*ptr = 0;
		ptr++;
		
		while(*ptr >= 0x20) ptr++;
	}
	
	if(direntries == 0) {
		lcd_print_centered("There is no homebrew on the SD card.", 160, 108, 0xFFFF, 0x0000);
		lcd_print_centered("Please press the power button", 160, 116, 0xFFFF, 0x0000);
		lcd_print_centered("to turn off the device.", 160, 124, 0xFFFF, 0x0000);
		lcd_update();
		while(1) buttons_get();
	}
	
	// Draw the main menu
	
	initmenu(direntries, card_capacity_mb, card_free_mb);
	
	while(1) {
		int selection = mainmenu("G&W Homebrew Loader Menu");
		
		if(selection >= 0)
			start_flash_process(selection);
		else if(selection == -1)
			submenu();
	}
}
