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
#include "buttons.h"
#include "flash.h"
#include "lcd.h"
#include "fslib.h"

//#include "mainmenu.h"

/**
  * @brief  The application entry point.
  * @return Nothing.
  */
int main() {
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

	// Inintialize flash

	OSPI_Init(&hospi1, SPI_MODE, VENDOR_MX);
	OSPI_NOR_WriteEnable(&hospi1);
	OSPI_EnableMemoryMappedMode(&hospi1);

	if(fsmount((uint8_t*)0x90000000)) {
		lcd_print_centered("Error! File system is corrupted!", 160, 116, 0xFFFF, 0x0000);
		lcd_update();

		while(1);
	}

	while(1) {
		int selection = mainmenu("G&W Homebrew Loader");
		
/*		if(selection >= 0)
			start_flash_process(selection);
		else if(selection == -1)
			submenu();*/
	}
}
