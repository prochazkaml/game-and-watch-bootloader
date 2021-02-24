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

uint32_t hbloader_comm_buf_word[4] __attribute__((section (".hbloader")));
#define hbloader_comm_buf_hword ((uint16_t *)hbloader_comm_buf_word)
#define hbloader_comm_buf ((uint8_t *)hbloader_comm_buf_word)

uint8_t *directory_names[1024];

uint8_t dir_buffer[16384] __attribute__((section (".databuf")));
uint8_t data_buffer[512 * 1024] __attribute__((section (".databuf")));

const char updir_name[] = "..";
const char intflash_name[] = "MAIN.BIN";
const char extflash_name[] = "EXTFLASH.BIN";
const char icon_name[] = "ICON.BMP";
const char manifest_name[] = "MANIFEST.TXT";

LTDC_HandleTypeDef hltdc;

OSPI_HandleTypeDef hospi1;

SAI_HandleTypeDef hsai_BlockA1;
DMA_HandleTypeDef hdma_sai1_a;

SPI_HandleTypeDef hspi2;

void SystemClock_Config();
static void MX_GPIO_Init();
static void MX_DMA_Init();
static void MX_LTDC_Init();
static void MX_SPI2_Init();
static void MX_OCTOSPI1_Init();
static void MX_SAI1_Init();
static void MX_NVIC_Init();

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

void hbloader_error(uint8_t call) {
	char buffer[64];
	int i, j, frame = 0;
	
	while(1) {
		for(i = 0; i < 320 * 240; i++) {
			framebuffer[i] = 0x001F;
		}
		
		sprintf(buffer, "Yikes! Tried to call function 0x%02X,", call);
		lcd_print_centered(buffer, 160, 8, 0xFFFF, 0x001F);
		
		sprintf(buffer, "but the module returned 0x%02X!", hbloader_comm_buf[0]);
		lcd_print_centered(buffer, 160, 16, 0xFFFF, 0x001F);
		
		lcd_print("Communication buffer dump:", 8, 32, 0xFFFF, 0x001F);
		
		for(j = 0; j < 4; j++) {
			sprintf(buffer, "0x%08X:", ((unsigned int) hbloader_comm_buf) + j * 4);
			lcd_print(buffer, 8, 48 + j * 8, 0xFFFF, 0x001F);
			lcd_print(buffer, 8, 88 + j * 8, 0xFFFF, 0x001F);

			// Bytes
			
			for(i = 0; i < 4; i++) {
				sprintf(buffer, "0x%02X", hbloader_comm_buf[i + j * 4]);
				lcd_print(buffer, 104 + i * 40, 48 + j * 8, 0xFFFF, 0x001F);
				
				lcd_putchar(hbloader_comm_buf[i + j * 4], 264 + i * 8, 48 + j * 8, 0x001F, 0xFFFF);

				sprintf(buffer, "%d", hbloader_comm_buf[i + j * 4]);
				lcd_print(buffer, 104 + i * 40, 88 + j * 8, 0xFFFF, 0x001F);
			}
		}
		
		for(j = 0; j < 2; j++) {
			sprintf(buffer, "0x%08X:", ((unsigned int) hbloader_comm_buf) + j * 8);
			lcd_print(buffer, 8, 128 + j * 8, 0xFFFF, 0x001F);
			lcd_print(buffer, 8, 152 + j * 8, 0xFFFF, 0x001F);
			lcd_print(buffer, 8, 176 + j * 8, 0xFFFF, 0x001F);
			lcd_print(buffer, 8, 200 + j * 8, 0xFFFF, 0x001F);

			// Half-words

			for(i = 0; i < 4; i++) {
				sprintf(buffer, "0x%04X", hbloader_comm_buf_hword[i + j * 4]);
				lcd_print(buffer, 104 + i * 56, 128 + j * 8, 0xFFFF, 0x001F);

				sprintf(buffer, "%d", hbloader_comm_buf_hword[i + j * 4]);
				lcd_print(buffer, 104 + i * 56, 152 + j * 8, 0xFFFF, 0x001F);
			}
			
			// Words
			
			for(i = 0; i < 2; i++) {
				sprintf(buffer, "0x%04lX", hbloader_comm_buf_word[i + j * 2]);
				lcd_print(buffer, 104 + i * 88, 176 + j * 8, 0xFFFF, 0x001F);

				sprintf(buffer, "%lu", hbloader_comm_buf_word[i + j * 2]);
				lcd_print(buffer, 104 + i * 88, 200 + j * 8, 0xFFFF, 0x001F);
			}
		}
		
		lcd_print_rtl("sorry, mate", 312, 224, 0xFFFF, 0x001F);
		
		frame++;
		sprintf(buffer, "%d", frame);
		lcd_print(buffer, 8, 224, 0xFFFF, 0x001F);
		
		lcd_update();
		buttons_get();
	}
}

void hbloader_call(uint8_t call) {
	hbloader_comm_buf[0] = call;
	
	while(hbloader_comm_buf[0] == call);
	
	if(hbloader_comm_buf[0] != 0xFF && hbloader_comm_buf[0] != 0x00) hbloader_error(call);
	
	while(hbloader_comm_buf[0] == 0xFF);

	if(hbloader_comm_buf[0] != 0x00) hbloader_error(call);
}

uint8_t hbloader_call_catcherr(uint8_t call) {
	hbloader_comm_buf[0] = call;
	
	while(hbloader_comm_buf[0] == call);
	
	if(hbloader_comm_buf[0] != 0xFF && hbloader_comm_buf[0] != 0x00) return hbloader_comm_buf[0];
	
	while(hbloader_comm_buf[0] == 0xFF);

	if(hbloader_comm_buf[0] != 0x00) return hbloader_comm_buf[0];
	
	return 0;
}

uint8_t hbloader_nonblock_call_id;

void hbloader_call_nonblock(uint8_t call) {
	hbloader_comm_buf[0] = call;
	hbloader_nonblock_call_id = call;
}

int hbloader_call_isdone() {
	uint8_t val = hbloader_comm_buf[0];
	
	if(val == 0x00) return 1;
	
	if(val >= 0x80 && val < 0xFF) hbloader_error(hbloader_nonblock_call_id);
	
	return 0;
}

int hbloader_call_isdone_ingore_missing() {
	uint8_t val = hbloader_comm_buf[0];
	
	if(val == 0x00) return 1;
	
	if(val == 0x81) return 0;
	
	if(val >= 0x80 && val < 0xFF) hbloader_error(hbloader_nonblock_call_id);
	
	return 0;
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
				hbloader_comm_buf_word[3] = (uint32_t) directory_names[cache[i].id];
				hbloader_call_nonblock(0x0C);
				cache[i].load_status++;
				break;
				
			case 3:		// Open then manifest file for reading
				hbloader_comm_buf_word[3] = (uint32_t) manifest_name;
				hbloader_call_nonblock(0x02);
				cache[i].load_status++;
				break;
				
			case 5:		// Read the manifest file
				hbloader_comm_buf_word[2] = sizeof(data_buffer);
				hbloader_comm_buf_word[3] = (uint32_t) data_buffer;
				hbloader_call_nonblock(0x04);
				cache[i].load_status++;
				break;

			case 7:		// Parse the manifest file and close it
				// Zero-terminate the file
				
				if(cache[i].id_pending < 0) {
					*(data_buffer + hbloader_comm_buf_word[1]) = 0;
					
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
				
				hbloader_call_nonblock(0x06);
				cache[i].load_status++;
				break;
				
			case 9:		// Open the homebrew icon
				hbloader_comm_buf_word[3] = (uint32_t) icon_name;
				hbloader_call_nonblock(0x02);
				cache[i].load_status++;
				break;
				
			case 11:	// Read the icon
				hbloader_comm_buf_word[2] = sizeof(data_buffer);
				hbloader_comm_buf_word[3] = (uint32_t) data_buffer;
				hbloader_call_nonblock(0x04);
				cache[i].load_status++;
				break;

			case 13:	// Parse the icon and close it
				if(cache[i].id_pending < 0) {
					decode_bmp(data_buffer, i);
				}
				
				hbloader_call_nonblock(0x06);
				cache[i].load_status++;
				break;
				
			case 15:		// Exit the directory
				hbloader_comm_buf_word[3] = (uint32_t) updir_name;
				hbloader_call_nonblock(0x0C);
				cache[i].load_status++;
				break;
				
			case 17:		// End the process
				cache[i].load_status = 0;
				
				update_hb_info_ctr++;
				update_hb_info_ctr %= 3;
				break;
				
			case 2: case 6: case 8: case 12: case 14: case 16: 	// Check if the command is done
				if(hbloader_call_isdone()) cache[i].load_status++;
				break;
				
			case 4:		// Special handler if the manifest file doesn't exist
				if(hbloader_comm_buf[0] == 0x81) {
					hbloader_comm_buf[0] = 0;
					
					if(cache[i].id_pending < 0) {
						sprintf(cache[i].name, "Corrupted homebrew");
						sprintf(cache[i].author, "The manifest file is missing.");
						sprintf(cache[i].version, "???");
						
						decode_bmp(default_bmp, i);
					}
					
					cache[i].load_status = 15;
				} else if(hbloader_call_isdone_ingore_missing()) {
					cache[i].load_status++;
					break;
				}
				
				break;
				
			case 10:	// Special handler if the icon file doesn't exist
				if(hbloader_comm_buf[0] == 0x81) {
					hbloader_comm_buf[0] = 0;

					if(cache[i].id_pending < 0) {
						decode_bmp(default_bmp, i);
					}
					
					cache[i].load_status = 15;
				} else if(hbloader_call_isdone_ingore_missing()) {
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
	
	hbloader_comm_buf_word[3] = (uint32_t) directory_names[selection];
	hbloader_call(0x0C);
	
	hbloader_comm_buf_word[3] = (uint32_t) intflash_name;
	
	if(hbloader_call_catcherr(0x02)) {
		hbloader_comm_buf_word[3] = (uint32_t) updir_name;
		hbloader_call(0x0C);

		draw_window(240, 40);
		lcd_print_centered("This homebrew is corrupted.", 160, 112, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
		lcd_print_centered("MAIN.BIN is missing.", 160, 120, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
		lcd_update();
		
		while(!buttons_get());
		return;
	}
	
	char buffer[64];
	
	int size = hbloader_comm_buf_word[1];
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
	
	hbloader_comm_buf_word[2] = 0x2000;
	hbloader_comm_buf_word[3] = (uint32_t) data_buffer;
	
	for(i = 0; i < sectors; i++) {
		draw_progress_bar(i, sectors, 60, 124, 200, 16);
		lcd_update();
		
		hbloader_call(0x04);
		
		for(j = 0; j < 0x2000; j += 16) {
			if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, 0x08000000 + i * 0x2000 + j, ((uint32_t)(data_buffer + j))) != HAL_OK) {
				Error_Handler();
			}
		}
	}
	
	draw_progress_bar(sectors, sectors, 60, 124, 200, 16);
	lcd_update();
	
	hbloader_call(0x06);
	HAL_FLASH_Lock();
	
	// Write the external flash, if required
	
	hbloader_comm_buf_word[3] = (uint32_t) extflash_name;
	
	if(!hbloader_call_catcherr(0x02)) {
		draw_window(280, 64);
		lcd_print_centered("Erasing external flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
		lcd_update();
		
		OSPI_Init(&hospi1, SPI_MODE, VENDOR_MX);
				
		OSPI_NOR_WriteEnable(&hospi1);
		OSPI_ChipErase(&hospi1);
		
		size = hbloader_comm_buf_word[1];
		sectors = size / 0x10000;
		if(size % 0x10000) sectors++;
		
		draw_window(280, 64);
		lcd_print_centered("Writing external flash...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
		sprintf(buffer, "(%d bytes, %d 64k sectors)", size, sectors);
		lcd_print_centered(buffer, 160, 108, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
		
		hbloader_comm_buf_word[2] = 0x10000;
		hbloader_comm_buf_word[3] = (uint32_t) data_buffer;
		
		for(i = 0; i < sectors; i++) {
			draw_progress_bar(i, sectors, 60, 124, 200, 16);
			lcd_update();
			
			hbloader_call(0x04);
			
//			OSPI_BlockErase(&hospi1, i * 0x10000);
			OSPI_Program(&hospi1, i * 0x10000, data_buffer, 0x10000);
		}
		
		draw_progress_bar(sectors, sectors, 60, 124, 200, 16);
		lcd_update();
		
		hbloader_call(0x06);
	}
	
	draw_window(280, 64);
	lcd_print_centered("Rebooting...", 160, 100, 0xFFFF, LCD_COLOR_GRAYSCALE(8));
	lcd_update();
	
	hbloader_call(0x7F);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main() {
	int i, j;
	char buffer[64];
	
	// Reset of all peripherals, Initializes the Flash interface and the Systick.

	HAL_Init();

	HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1_LOW);

	// Configure the system clock

	SystemClock_Config();

	// Initialize all configured peripherals

	MX_GPIO_Init();
	MX_DMA_Init();
	MX_LTDC_Init();
	MX_SPI2_Init();
	MX_OCTOSPI1_Init();
	MX_SAI1_Init();

	// Initialize interrupts

	MX_NVIC_Init();

	// Initialize LCD
	
	lcd_init(&hspi2, &hltdc);
	lcd_backlight_on();

	// Get SD card capacity
	
	lcd_print_centered("Detecting SD card...", 160, 116, 0xFFFF, 0x0000);
	lcd_update();

	hbloader_call(0x01);

	uint64_t tmp;
	
	tmp = hbloader_comm_buf_word[2];
	tmp = tmp * 512 / 1000000;
	card_capacity_mb = tmp;

	tmp = hbloader_comm_buf_word[3];
	tmp = tmp * 512 / 1000000;
	card_free_mb = tmp;

	// Load the directory
	
	lcd_print_centered("Loading root directory...", 160, 116, 0xFFFF, 0x0000);
	lcd_update();

	hbloader_comm_buf_word[3] = (uint32_t) dir_buffer;
	hbloader_call(0x0D);

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
	
	for(j = 0; j < 3; j++) cache[j].load_status = 0, cache[j].id = -1, cache[j].id_pending = -1, init_hb_info(j);
	
	// Draw the header
	
	for(j = 0; j < 16 * 320; j++) framebuffer[j] = LCD_COLOR_GRAYSCALE(4);
	lcd_print_centered("G&W Homebrew Loader Menu", 160, 4, 0xFFFF, LCD_COLOR_GRAYSCALE(4));

	while (1) {
		uint32_t buttons = buttons_get();

		if(buttons & B_Up) {
			selection--;
			if(selection == -1) {
				selection = (maxselection - 1);
				scroll = maxselection - 3;
				if(scroll < 0) scroll = 0;
				for(j = 0; j < 3; j++) init_hb_info(scroll + j);
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
				for(j = 0; j < 3; j++) init_hb_info(j);
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

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config() {
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

	// Supply configuration update enable

	HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

	// Configure the main internal regulator output voltage

	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
	while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

	// Macro to configure the PLL clock source

	__HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSI);

	// Initializes the RCC Oscillators according to the specified parameters
	// in the RCC_OscInitTypeDef structure.

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 16;
	RCC_OscInitStruct.PLL.PLLN = 140;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;

	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	// Initializes the CPU, AHB and APB buses clocks
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
								|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
								|RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK) {
		Error_Handler();
	}

	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC|RCC_PERIPHCLK_SPI2
								|RCC_PERIPHCLK_SAI1|RCC_PERIPHCLK_OSPI
								|RCC_PERIPHCLK_CKPER;
	PeriphClkInitStruct.PLL2.PLL2M = 25;
	PeriphClkInitStruct.PLL2.PLL2N = 192;
	PeriphClkInitStruct.PLL2.PLL2P = 5;
	PeriphClkInitStruct.PLL2.PLL2Q = 2;
	PeriphClkInitStruct.PLL2.PLL2R = 5;
	PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_1;
	PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
	PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
	PeriphClkInitStruct.PLL3.PLL3M = 4;
	PeriphClkInitStruct.PLL3.PLL3N = 9;
	PeriphClkInitStruct.PLL3.PLL3P = 2;
	PeriphClkInitStruct.PLL3.PLL3Q = 2;
	PeriphClkInitStruct.PLL3.PLL3R = 24;
	PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_3;
	PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
	PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
	PeriphClkInitStruct.OspiClockSelection = RCC_OSPICLKSOURCE_CLKP;
	PeriphClkInitStruct.CkperClockSelection = RCC_CLKPSOURCE_HSI;
	PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL2;
	PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_CLKP;

	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
		Error_Handler();
	}
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init() {
	// OCTOSPI1_IRQn interrupt configuration
	HAL_NVIC_SetPriority(OCTOSPI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(OCTOSPI1_IRQn);
}

/**
  * @brief LTDC Initialization Function
  * @retval None
  */
static void MX_LTDC_Init() {
	LTDC_LayerCfgTypeDef pLayerCfg = {0};
	LTDC_LayerCfgTypeDef pLayerCfg1 = {0};

	hltdc.Instance = LTDC;
	hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
	hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
	hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
	hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IIPC;
	hltdc.Init.HorizontalSync = 9;
	hltdc.Init.VerticalSync = 1;
	hltdc.Init.AccumulatedHBP = 60;
	hltdc.Init.AccumulatedVBP = 7;
	hltdc.Init.AccumulatedActiveW = 380;
	hltdc.Init.AccumulatedActiveH = 247;
	hltdc.Init.TotalWidth = 392;
	hltdc.Init.TotalHeigh = 255;
	hltdc.Init.Backcolor.Blue = 0;
	hltdc.Init.Backcolor.Green = 0;
	hltdc.Init.Backcolor.Red = 0;

	if (HAL_LTDC_Init(&hltdc) != HAL_OK) {
		Error_Handler();
	}

	pLayerCfg.WindowX0 = 0;
	pLayerCfg.WindowX1 = 320;
	pLayerCfg.WindowY0 = 0;
	pLayerCfg.WindowY1 = 240;
	pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
	pLayerCfg.Alpha = 255;
	pLayerCfg.Alpha0 = 255;
	pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
	pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
	pLayerCfg.FBStartAdress = 0x24000000;
	pLayerCfg.ImageWidth = 320;
	pLayerCfg.ImageHeight = 240;
	pLayerCfg.Backcolor.Blue = 0;
	pLayerCfg.Backcolor.Green = 255;
	pLayerCfg.Backcolor.Red = 0;

	if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK) {
		Error_Handler();
	}

	pLayerCfg1.WindowX0 = 0;
	pLayerCfg1.WindowX1 = 0;
	pLayerCfg1.WindowY0 = 0;
	pLayerCfg1.WindowY1 = 0;
	pLayerCfg1.Alpha = 0;
	pLayerCfg1.Alpha0 = 0;
	pLayerCfg1.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
	pLayerCfg1.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
	pLayerCfg1.FBStartAdress = GFXMMU_VIRTUAL_BUFFER0_BASE;
	pLayerCfg1.ImageWidth = 0;
	pLayerCfg1.ImageHeight = 0;
	pLayerCfg1.Backcolor.Blue = 0;
	pLayerCfg1.Backcolor.Green = 0;
	pLayerCfg1.Backcolor.Red = 0;

	if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg1, 1) != HAL_OK) {
		Error_Handler();
	}
}

/**
  * @brief OCTOSPI1 Initialization Function
  * @retval None
  */
static void MX_OCTOSPI1_Init() {
	OSPIM_CfgTypeDef sOspiManagerCfg = {0};

	// OCTOSPI1 parameter configuration
	hospi1.Instance = OCTOSPI1;
	hospi1.Init.FifoThreshold = 4;
	hospi1.Init.DualQuad = HAL_OSPI_DUALQUAD_DISABLE;
	hospi1.Init.MemoryType = HAL_OSPI_MEMTYPE_MACRONIX;
	hospi1.Init.DeviceSize = 20;
	hospi1.Init.ChipSelectHighTime = 2;
	hospi1.Init.FreeRunningClock = HAL_OSPI_FREERUNCLK_DISABLE;
	hospi1.Init.ClockMode = HAL_OSPI_CLOCK_MODE_0;
	hospi1.Init.WrapSize = HAL_OSPI_WRAP_NOT_SUPPORTED;
	hospi1.Init.ClockPrescaler = 1;
	hospi1.Init.SampleShifting = HAL_OSPI_SAMPLE_SHIFTING_NONE;
	hospi1.Init.DelayHoldQuarterCycle = HAL_OSPI_DHQC_DISABLE;
	hospi1.Init.ChipSelectBoundary = 0;
	hospi1.Init.ClkChipSelectHighTime = 0;
	hospi1.Init.DelayBlockBypass = HAL_OSPI_DELAY_BLOCK_BYPASSED;
	hospi1.Init.MaxTran = 0;
	hospi1.Init.Refresh = 0;

	if (HAL_OSPI_Init(&hospi1) != HAL_OK) {
		Error_Handler();
	}

	sOspiManagerCfg.ClkPort = 1;
	sOspiManagerCfg.NCSPort = 1;
	sOspiManagerCfg.IOLowPort = HAL_OSPIM_IOPORT_1_LOW;

	if (HAL_OSPIM_Config(&hospi1, &sOspiManagerCfg, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
		Error_Handler();
	}
}

/**
  * @brief SAI1 Initialization Function
  * @retval None
  */
static void MX_SAI1_Init() {
	hsai_BlockA1.Instance = SAI1_Block_A;
	hsai_BlockA1.Init.AudioMode = SAI_MODEMASTER_TX;
	hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS;
	hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
	hsai_BlockA1.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
	hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
	hsai_BlockA1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
	hsai_BlockA1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
	hsai_BlockA1.Init.MonoStereoMode = SAI_MONOMODE;
	hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
	hsai_BlockA1.Init.TriState = SAI_OUTPUT_NOTRELEASED;

	if (HAL_SAI_InitProtocol(&hsai_BlockA1, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_16BIT, 2) != HAL_OK) {
		Error_Handler();
	}
}

/**
  * @brief SPI2 Initialization Function
  * @retval None
  */
static void MX_SPI2_Init() {
	// SPI2 parameter configuration
	hspi2.Instance = SPI2;
	hspi2.Init.Mode = SPI_MODE_MASTER;
	hspi2.Init.Direction = SPI_DIRECTION_2LINES_TXONLY;
	hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi2.Init.CRCPolynomial = 0x0;
	hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
	hspi2.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
	hspi2.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
	hspi2.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	hspi2.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	hspi2.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
	hspi2.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
	hspi2.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
	hspi2.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
	hspi2.Init.IOSwap = SPI_IO_SWAP_DISABLE;

	if (HAL_SPI_Init(&hspi2) != HAL_OK) {
		Error_Handler();
	}
}

/**
  * @brief Enables DMA controller clock
  * @retval None
  */
static void MX_DMA_Init() {
	// DMA controller clock enable
	__HAL_RCC_DMA1_CLK_ENABLE();

	// DMA interrupt init
	// DMA1_Stream0_IRQn interrupt configuration
	HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
}

/**
  * @brief GPIO Initialization Function
  * @retval None
  */
static void MX_GPIO_Init() {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// GPIO Ports Clock Enable
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	// Configure GPIO pin Output Level
	HAL_GPIO_WritePin(GPIO_Speaker_enable_GPIO_Port, GPIO_Speaker_enable_Pin, GPIO_PIN_SET);

	// Configure GPIO pin Output Level
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8|GPIO_PIN_4, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);

	// Configure GPIO pin : GPIO_Speaker_enable_Pin
	GPIO_InitStruct.Pin = GPIO_Speaker_enable_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIO_Speaker_enable_GPIO_Port, &GPIO_InitStruct);

	// Configure GPIO pins : BTN_PAUSE_Pin BTN_GAME_Pin BTN_TIME_Pin
	GPIO_InitStruct.Pin = BTN_PAUSE_Pin|BTN_GAME_Pin|BTN_TIME_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	// Configure GPIO pin : PTN_Power_Pin
	GPIO_InitStruct.Pin = BTN_Power_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(BTN_Power_GPIO_Port, &GPIO_InitStruct);

	// Configure GPIO pins : PA4 PA5 PA6
	GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// Configure GPIO pin : PB12
	GPIO_InitStruct.Pin = GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	// Configure GPIO pins : PD8 PD1 PD4
	GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_1|GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	// Configure GPIO pins : BTN_A_Pin BTN_Left_Pin BTN_Down_Pin BTN_Right_Pin
	//						BTN_Up_Pin BTN_B_Pin
	GPIO_InitStruct.Pin = BTN_A_Pin|BTN_Left_Pin|BTN_Down_Pin|BTN_Right_Pin
							|BTN_Up_Pin|BTN_B_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler() {
	while(1) {
		// Blink display to indicate failure
		lcd_backlight_off();
		HAL_Delay(500);
		lcd_backlight_on();
		HAL_Delay(500);
	}
}

/**
  * @brief  Puts the system into sleep mode.
  * @retval None
  */
void GW_Sleep() {
	HAL_SAI_DMAStop(&hsai_BlockA1);

	HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_LOW);

	lcd_backlight_off();

	HAL_PWR_EnterSTANDBYMode();

	while(1) HAL_NVIC_SystemReset();
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
