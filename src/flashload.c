#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "stm32h7xx_hal.h"
#include "flashload.h"
#include "gwloader.h"
#include "mainmenu.h"
#include "buttons.h"
#include "flash.h"
#include "lcd.h"
#include "stm32.h"

/**
  * @brief  Wait for any processes to finish.
  * @return Nothing.
  */
void wait() {
	lcd_draw_window(200, 32);
	lcd_print_centered("Please wait...", 160, 116, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	lcd_update();

	mainmenu_finish();
}

/**
  * @brief  Dump the device.
  * @return Nothing.
  */
void start_dump_process() {
	wait();
	
	int i;
	
	// Dump internal flash
	
	gwloader_comm_buf[1] = 0;
	gwloader_comm_buf_word[3] = (uint32_t) intflash_name;
	gwloader_call(GWL_OPEN_WRITE);
	
	gwloader_comm_buf_word[2] = 0x2000;
	gwloader_comm_buf_word[3] = (uint32_t) data_buffer;

	lcd_draw_window(280, 64);
	lcd_print_centered("Dumping internal flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	lcd_print_centered("(131072 bytes, 16 8k sectors)", 160, 108, 0xFFFF, LCD_COLOR_GRAYSCALE(4));

	for(i = 0; i < 16; i++) {
		lcd_draw_progress_bar(i, 16, 60, 124, 200, 16);
		lcd_update();

		memcpy(data_buffer, (uint8_t *)(0x08000000 + i * 0x2000), 0x2000);
		gwloader_call(GWL_WRITE);
		
	}
	
	gwloader_call(GWL_CLOSE);

	// Dump external flash
	
	gwloader_comm_buf[1] = 0;
	gwloader_comm_buf_word[3] = (uint32_t) extflash_name;
	gwloader_call(GWL_OPEN_WRITE);
		
	gwloader_comm_buf_word[2] = 0x10000;
	gwloader_comm_buf_word[3] = (uint32_t) data_buffer;

	lcd_draw_window(280, 64);
	lcd_print_centered("Dumping external flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	lcd_print_centered("(1048576 bytes, 16 64k sectors)", 160, 108, 0xFFFF, LCD_COLOR_GRAYSCALE(4));

	for(i = 0; i < 16; i++) {
		lcd_draw_progress_bar(i, 16, 60, 124, 200, 16);
		lcd_update();

		OSPI_Read(&hospi1, i * 0x10000, data_buffer, 0x10000);
		gwloader_call(GWL_WRITE);
		
	}
	
	gwloader_call(GWL_CLOSE);

}
	
/**
  * @brief  Flash the selected homebrew onto the device.
  * @param  selection: Homebrew ID.
  * @return This function should never return.
  */
void start_flash_process(int selection) {
	lcd_fade();
	wait();
	
	// Enter the correct directory and open the internal flash file
	
	gwloader_comm_buf_word[3] = (uint32_t) directory_names[selection];
	gwloader_call(GWL_CHDIR);
	
	gwloader_comm_buf_word[3] = (uint32_t) intflash_name;
	
	if(gwloader_call_catcherr(GWL_OPEN_READ)) {
		gwloader_comm_buf_word[3] = (uint32_t) updir_name;
		gwloader_call(GWL_CHDIR);

		lcd_draw_window(240, 40);
		lcd_print_centered("This homebrew is corrupted.", 160, 112, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		lcd_print_centered("MAIN.BIN is missing.", 160, 120, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		lcd_update();
		
		while(!buttons_get());
		return;
	}
	
	char buffer[64];
	
	int size = gwloader_comm_buf_word[1];
	int sectors = size >> 13;
	if(size & 0x1FFF) sectors++;
	
	lcd_draw_window(280, 64);
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
	
	lcd_draw_window(280, 64);
	lcd_print_centered("Writing internal flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	sprintf(buffer, "(%d bytes, %d 8k sectors)", size, sectors);
	lcd_print_centered(buffer, 160, 108, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	
	gwloader_comm_buf_word[2] = 0x2000;
	gwloader_comm_buf_word[3] = (uint32_t) data_buffer;
	
	int i, j;
	
	for(i = 0; i < sectors; i++) {
		lcd_draw_progress_bar(i, sectors, 60, 124, 200, 16);
		lcd_update();
		
		gwloader_call(GWL_READ);
		
		for(j = 0; j < 0x2000; j += 16) {
			if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, 0x08000000 + i * 0x2000 + j, ((uint32_t)(data_buffer + j))) != HAL_OK) {
				Error_Handler();
			}
		}
	}
	
	lcd_draw_progress_bar(sectors, sectors, 60, 124, 200, 16);
	lcd_update();
	
	gwloader_call(GWL_CLOSE);
	HAL_FLASH_Lock();
	
	// Write the external flash, if required
	
	gwloader_comm_buf_word[3] = (uint32_t) extflash_name;
	
	if(!gwloader_call_catcherr(GWL_OPEN_READ)) {
		lcd_draw_window(280, 64);
		lcd_print_centered("Erasing external flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		lcd_print_centered("This can take a while.", 160, 108, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		lcd_update();
		
		OSPI_ChipErase(&hospi1);
		
		size = gwloader_comm_buf_word[1];
		sectors = size >> 16;
		if(size & 0xFFFF) sectors++;
		
		lcd_draw_window(280, 64);
		lcd_print_centered("Writing external flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		sprintf(buffer, "(%d bytes, %d 64k sectors)", size, sectors);
		lcd_print_centered(buffer, 160, 108, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
		
		gwloader_comm_buf_word[2] = 0x10000;
		gwloader_comm_buf_word[3] = (uint32_t) data_buffer;
		
		for(i = 0; i < sectors; i++) {
			lcd_draw_progress_bar(i, sectors, 60, 124, 200, 16);
			lcd_update();
			
			gwloader_call(GWL_READ);
			
//			OSPI_BlockErase(&hospi1, i * 0x10000);	// Doesn't work right :/
			OSPI_Program(&hospi1, i * 0x10000, data_buffer, 0x10000);
		}
		
		lcd_draw_progress_bar(sectors, sectors, 60, 124, 200, 16);
		lcd_update();
		
		gwloader_call(GWL_CLOSE);
	}
	
	lcd_draw_window(280, 64);
	lcd_print_centered("Rebooting...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	lcd_update();
	
	gwloader_call(GWL_RESET_HALT);
}
