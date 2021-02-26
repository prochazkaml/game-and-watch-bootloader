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
#include "main.h"
#include "stm32.h"
#include "gwloader.h"
#include "buttons.h"
#include "flash.h"
#include "lcd.h"
#include "default.h"
#include "hourglass.h"

typedef struct {
	int id;
	int id_pending;
	int load_status;
	char name[32];
	char author[32];
	char version[32];
	uint16_t bitmap[64 * 48];
} HomebrewEntry;

HomebrewEntry cache[3];

uint8_t *directory_names[1024];

uint8_t dir_buffer[16384] __attribute__((section (".databuf")));
uint8_t data_buffer[64 * 1024] __attribute__((section (".databuf")));

const char updir_name[] = "..";
const char intflash_name[] = "MAIN.BIN";
const char extflash_name[] = "EXTFLASH.BIN";
const char icon_name[] = "ICON.BMP";
const char manifest_name[] = "MANIFEST.TXT";

int selection = 0, maxselection = 0, scroll = 0;
int card_capacity_mb, card_free_mb;

void draw_border(int i, int color) {
	int j;

	for(j = 8; j < 320 - 8; j++) {
		framebuffer[320 * 21 + i * 320 * 72 + j] = color;
		framebuffer[320 * (21 + 53) + i * 320 * 72 + j] = color;
	}

	for(j = 1; j < 53; j++) {
		framebuffer[320 * 21 + i * 320 * 72 + j * 320 + 7] = color;
		framebuffer[320 * 21 + i * 320 * 72 + j * 320 + (320 - 7)] = color;
	}
}

void draw_bitmap(uint16_t *bmp, int off_x, int off_y) {
	int x, y, ptr = 0;

	for(y = 0; y < 48; y++) {
		for(x = 0; x < 64; x++) {
			framebuffer[off_x + x + (off_y + 47 - y) * 320] = bmp[ptr++];
		}
	}
}

void decode_bmp(unsigned char *bmp, int id) {
	id %= 3;
	
	unsigned char *imgdata = bmp + hourglass_bmp[0x0A];
	int i;
	
	for(i = 0; i < 64 * 48; i++) {
		cache[id].bitmap[i] = imgdata[i * 2] | (imgdata[i * 2 + 1] << 8);
	}
}

void draw_footer() {
	int j;
	char buffer[32];
	
	// Draw the footer
	for(j = 224 * 320; j < 240 * 320; j++) framebuffer[j] = LCD_COLOR_GRAYSCALE(4);
	sprintf(buffer, "%d/%d", selection + 1, maxselection);
	lcd_print_rtl(buffer, 312, 228, 0xFFFF, LCD_COLOR_GRAYSCALE(4));

	sprintf(buffer, "%d.%02d GB total, %d.%02d GB free",
		card_capacity_mb / 1000, card_capacity_mb % 1000 / 10,
		card_free_mb / 1000, card_free_mb % 1000 / 10);
	lcd_print(buffer, 8, 228, 0xFFFF, LCD_COLOR_GRAYSCALE(4));

	// Clear the screen
	for(j = 16 * 320; j < 224 * 320; j++) framebuffer[j] = 0x0000;	
}

void update_screen() {
	int i, j;

	draw_footer();
	
	// Draw the boxes, text and icons
	for(i = 0; i < ((maxselection < 3) ? maxselection : 3); i++) {
		draw_border(i, (i == (selection - scroll)) ? 0xFFFF : LCD_COLOR_GRAYSCALE(4));
		
		j = (i + scroll) % 3;
		draw_bitmap(cache[j].bitmap, 10, 24 + i * 72);
		
		lcd_print(cache[j].name, 80, 28 + i * 72, 0xFFFF, 0x0000);
		lcd_print(cache[j].author, 80, 44 + i * 72, LCD_COLOR_GRAYSCALE(24), 0x0000);
		lcd_print(cache[j].version, 80, 60 + i * 72, LCD_COLOR_GRAYSCALE(24), 0x0000);
	}

	lcd_update();
}

void init_hb_info(int id) {
	int i = id % 3;
	
	if(id >= maxselection || (id == cache[i].id && cache[i].id_pending < 0)) {
		return;
	}
	
	cache[i].id_pending = id;

	cache[i].name[0] = 0;
	cache[i].author[0] = 0;
	cache[i].version[0] = 0;
	
	decode_bmp(hourglass_bmp, i);
}

int update_hb_info_ctr = 0;

void update_hb_info() {
	int i = update_hb_info_ctr;
	
	if(cache[i].id_pending >= 0 && cache[i].load_status == 0) {
		cache[i].load_status = 1;
		cache[i].id = cache[i].id_pending;
		cache[i].id_pending = -1;
	}
	
	if(cache[i].load_status) {
		switch(cache[i].load_status) {
			case 1:		// Enter the directory
				gwloader_comm_buf_word[3] = (uint32_t) directory_names[cache[i].id];
				gwloader_call_nonblock(0x0C);
				cache[i].load_status++;
				break;
				
			case 3:		// Open then manifest file for reading
				gwloader_comm_buf_word[3] = (uint32_t) manifest_name;
				gwloader_call_nonblock(0x02);
				cache[i].load_status++;
				break;
				
			case 5:		// Read the manifest file
				gwloader_comm_buf_word[2] = sizeof(data_buffer);
				gwloader_comm_buf_word[3] = (uint32_t) data_buffer;
				gwloader_call_nonblock(0x04);
				cache[i].load_status++;
				break;

			case 7:		// Parse the manifest file and close it
				// Zero-terminate the file
				
				if(cache[i].id_pending < 0) {
					*(data_buffer + gwloader_comm_buf_word[1]) = 0;
					
					// Use default values first
					
					sprintf(cache[i].name, "Unnamed homebrew");
					sprintf(cache[i].author, "Unknown author");
					sprintf(cache[i].version, "1.0");
					
					// Parse each line
					
					char *manifest = strtok((char *) data_buffer, "\n");
					
					while(manifest != NULL) {
						if(!memcmp("Name=", manifest, 5))
							strncpy(cache[i].name, manifest + 5, 32);

						if(!memcmp("Author=", manifest, 7))
							strncpy(cache[i].author, manifest + 7, 32);
							
						if(!memcmp("Version=", manifest, 8))
							strncpy(cache[i].version, manifest + 8, 32);
							
//						} else if(!memcmp("UsesFileAccess=", manifest, 15)) {

						manifest = strtok(NULL, "\n");
					}
				}
				
				gwloader_call_nonblock(0x06);
				cache[i].load_status++;
				break;
				
			case 9:		// Open the homebrew icon
				gwloader_comm_buf_word[3] = (uint32_t) icon_name;
				gwloader_call_nonblock(0x02);
				cache[i].load_status++;
				break;
				
			case 11:	// Read the icon
				gwloader_comm_buf_word[2] = sizeof(data_buffer);
				gwloader_comm_buf_word[3] = (uint32_t) data_buffer;
				gwloader_call_nonblock(0x04);
				cache[i].load_status++;
				break;

			case 13:	// Parse the icon and close it
				if(cache[i].id_pending < 0) {
					decode_bmp(data_buffer, i);
				}
				
				gwloader_call_nonblock(0x06);
				cache[i].load_status++;
				break;
				
			case 15:		// Exit the directory
				gwloader_comm_buf_word[3] = (uint32_t) updir_name;
				gwloader_call_nonblock(0x0C);
				cache[i].load_status++;
				break;
				
			case 17:		// End the process
				cache[i].load_status = 0;
				
				update_hb_info_ctr++;
				update_hb_info_ctr %= 3;
				break;
				
			case 2: case 6: case 8: case 12: case 14: case 16: 	// Check if the command is done
				if(gwloader_call_isdone()) cache[i].load_status++;
				break;
				
			case 4:		// Special handler if the manifest file doesn't exist
				if(gwloader_comm_buf[0] == 0x81) {
					gwloader_comm_buf[0] = 0;
					
					if(cache[i].id_pending < 0) {
						sprintf(cache[i].name, "Corrupted homebrew");
						sprintf(cache[i].author, "The manifest file is missing.");
						sprintf(cache[i].version, "???");
						
						decode_bmp(default_bmp, i);
					}
					
					cache[i].load_status = 15;
				} else if(gwloader_call_isdone_ingore_missing()) {
					cache[i].load_status++;
					break;
				}
				
				break;
				
			case 10:	// Special handler if the icon file doesn't exist
				if(gwloader_comm_buf[0] == 0x81) {
					gwloader_comm_buf[0] = 0;

					if(cache[i].id_pending < 0) {
						decode_bmp(default_bmp, i);
					}
					
					cache[i].load_status = 15;
				} else if(gwloader_call_isdone_ingore_missing()) {
					cache[i].load_status++;
					break;
				}
		}
	} else {
		update_hb_info_ctr++;
		update_hb_info_ctr %= 3;
	}
}

void draw_window(int w, int h) {
	int x, y;

	w /= 2;
	h /= 2;
	
	for(y = 120 - h; y < 120 + h; y++) {
		for(x = 160 - w; x < 160 + w; x++) {
			framebuffer[x + y * 320] = LCD_COLOR_GRAYSCALE(8);
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

void start_flash_process() {
	// Fade the screen
	
	int i, j, val, r, g, b;
	
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
	
	// Wait for any processes to finish
	
	draw_window(200, 32);
	lcd_print_centered("Please wait...", 160, 116, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
	lcd_update();

	// Dump all current processes
	
/*	for(i = 0; i < 3; i++) {
		switch(cache[0].load_status) {
			case 1:
				cache[0].load_status = 17;
				
			case 3: case 9:
				cache[0].load_status = 15;

			case 5: case 7: case 11: case 13:
				cache[0].load_status = 13;
		}
	}
*/	
	// Dump all planned processes
	
//	for(i = 0; i < 3; i++) cache[i].id_pending = -1;
	
	while(cache[0].load_status || cache[0].id_pending >= 0 ||
			cache[1].load_status || cache[1].id_pending >= 0 ||
			cache[2].load_status || cache[2].id_pending >= 0) update_hb_info();
	
	// Enter the correct directory and open the internal flash file
	
	gwloader_comm_buf_word[3] = (uint32_t) directory_names[selection];
	gwloader_call(0x0C);
	
	gwloader_comm_buf_word[3] = (uint32_t) intflash_name;
	
	if(gwloader_call_catcherr(0x02)) {
		gwloader_comm_buf_word[3] = (uint32_t) updir_name;
		gwloader_call(0x0C);

		draw_window(240, 40);
		lcd_print_centered("This homebrew is corrupted.", 160, 112, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
		lcd_print_centered("MAIN.BIN is missing.", 160, 120, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
		lcd_update();
		
		while(!buttons_get());
		return;
	}
	
	char buffer[64];
	
	int size = gwloader_comm_buf_word[1];
	int sectors = size / 0x2000;
	if(size % 0x2000) sectors++;
	
	draw_window(280, 64);
	lcd_print_centered("Erasing internal flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
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
	lcd_print_centered("Writing internal flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
	sprintf(buffer, "(%d bytes, %d 8k sectors)", size, sectors);
	lcd_print_centered(buffer, 160, 108, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
	
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
		lcd_print_centered("Erasing external flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
		lcd_update();
		
		OSPI_Init(&hospi1, SPI_MODE, VENDOR_MX);
				
		OSPI_NOR_WriteEnable(&hospi1);
		OSPI_ChipErase(&hospi1);
		
		size = gwloader_comm_buf_word[1];
		sectors = size / 0x10000;
		if(size % 0x10000) sectors++;
		
		draw_window(280, 64);
		lcd_print_centered("Writing external flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
		sprintf(buffer, "(%d bytes, %d 64k sectors)", size, sectors);
		lcd_print_centered(buffer, 160, 108, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
		
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
	lcd_print_centered("Rebooting...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
	lcd_update();
	
	gwloader_call(0x7F);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main() {
	int i;
	
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
	
	for(i = 0; i < 3; i++) cache[i].load_status = 0, cache[i].id = -1, cache[i].id_pending = -1, init_hb_info(i);
	
	// Draw the header
	
	for(i = 0; i < 16 * 320; i++) framebuffer[i] = LCD_COLOR_GRAYSCALE(4);
	lcd_print_centered("G&W Homebrew Loader Menu", 160, 4, 0xFFFF, LCD_COLOR_GRAYSCALE(4));

	while (1) {
		uint32_t buttons = buttons_get();

		if(buttons & B_Up) {
			selection--;
			if(selection == -1) {
				selection = (maxselection - 1);
				scroll = maxselection - 3;
				if(scroll < 0) scroll = 0;
				for(i = 0; i < 3; i++) init_hb_info(scroll + i);
			}
			
			if(selection - scroll == -1) {
				scroll--;
				init_hb_info(selection);
			}
				
		}

		if(buttons & B_Down) {
			selection++;
			if(selection == maxselection) {
				selection = 0;
				scroll = 0;
				for(i = 0; i < 3; i++) init_hb_info(i);
			}
				
			if(selection - scroll == 3) {
				init_hb_info(selection);
				scroll++;
			}
		}

		if(buttons & B_A) {
			start_flash_process();
		}
		
		update_screen();
		update_hb_info();
	}
}
