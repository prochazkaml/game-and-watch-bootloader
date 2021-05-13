#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mainmenu.h"

#include "buttons.h"
#include "lcd.h"
#include "stm32.h"
#include "fslib.h"

#include "default.h"

HomebrewEntry cache[3];

int selection, maxselection, scroll;

/**
  * @brief  Draw a selection border.
  * @param  i: Position on the screen (0-2).
  * @param  color: Color of the border.
  * @return Nothing.
  */
void draw_border(int i, int color) {
	int j;

	for(j = 8; j < 320 - 8; j++) {
		framebuffer[320 * 21 + i * 320 * 72 + j] = color;
		framebuffer[320 * (21 + 53) + i * 320 * 72 + j] = color;
	}

	for(j = 1; j < 53; j++) {
		framebuffer[320 * 21 + i * 320 * 72 + j * 320 + 7] = color;
		framebuffer[320 * 21 + i * 320 * 72 + j * 320 + (320 - 8)] = color;
	}
}

/**
  * @brief  Update the homebrew information on the screen.
  * @return Nothing.
  */
void update_screen() {
	int i, j;
	char buffer[32];

	// Clear the working area

	for(j = 16 * 320; j < 224 * 320; j++)
		framebuffer[j] = 0x0000;	

	// Draw the footer

	for(j = 224 * 320; j < 240 * 320; j++) framebuffer[j] = LCD_COLOR_GRAYSCALE(4);
	sprintf(buffer, "%d/%d", selection + 1, maxselection);
	lcd_print_rtl(buffer, 312, 228, 0xFFFF, LCD_COLOR_GRAYSCALE(4));

	sprintf(buffer, "%d kB free", fsgetfreespace());
	lcd_print(buffer, 8, 228, 0xFFFF, LCD_COLOR_GRAYSCALE(4));
	
	// Draw the boxes, text and icons

	for(i = 0; i < ((maxselection < 3) ? maxselection : 3); i++) {
		draw_border(i, (i == (selection - scroll)) ? 0xFFFF : LCD_COLOR_GRAYSCALE(4));
		
		j = (i + scroll) % 3;
		lcd_draw_icon(cache[j].bitmap, 10, 24 + i * 72);
		
		lcd_print(cache[j].name, 80, 28 + i * 72, 0xFFFF, 0x0000);
		lcd_print(cache[j].author, 80, 44 + i * 72, LCD_COLOR_GRAYSCALE(24), 0x0000);
		lcd_print(cache[j].version, 80, 60 + i * 72, LCD_COLOR_GRAYSCALE(24), 0x0000);
	}

	lcd_update();
}

/**
  * @brief  Copy raw bitmap to the homebrew cache.
  * @param  imgdata: Pointer to 16-bit raw data.
  * @param  id: Homebrew cache ID (0-2).
  * @return Nothing.
  */
void copy_bmp(uint16_t *imgdata, int id) {
	id %= 3;
	
	int i;
	
	for(i = 0; i < 64 * 48; i++)
		cache[id].bitmap[i] = imgdata[i];
}

/**
  * @brief  Decode bitmap to the homebrew cache.
  * @param  bmp: Pointer to a valid BMP file.
  * @param  id: Homebrew cache ID (0-2).
  * @return Nothing.
  */
void decode_bmp(unsigned char *bmp, int id) {
	copy_bmp((uint16_t *)(bmp + bmp[0x0A]), id);
}

/**
  * @brief  Prints an error message to the homebrew cache.
  * @param  id: Homebrew cache ID (0-2).
  * @param  msg: Error message.
  * @return Nothing.
  */
void hb_error(int id, char *msg) {
	cache[id].name[0] = 0;
	cache[id].version[0] = 0;
	strcpy(cache[id].author, msg);
}

/**
  * @brief  Load homebrew info to the homebrew cache.
  * @param  id: Homebrew ID.
  * @param  dir: Directory name.
  * @return Nothing.
  */
void load_hb_info(int id, char *dir) {
	long size;
	char *lineparser;

	int i = id % 3;

	// Check if this homebrew ID has already been loaded

	if(cache[i].id != id) {
		// If it hasn't, then enter its directory and load the manifest & icon

		if(!fschdir(dir)) {
			if((size = fsloadfile("MANIFEST.TXT", data_buffer, sizeof(data_buffer))) > 0) {
				// Use default values first

				sprintf(cache[i].name, "Unnamed homebrew");
				sprintf(cache[i].author, "Unknown author");
				sprintf(cache[i].version, "1.0");

				// Parse each line

				lineparser = strtok((char *) data_buffer, "\n");

				while(lineparser != NULL) {
					if(!memcmp("Name=", lineparser, 5))
						strncpy(cache[i].name, lineparser + 5, 32);

					if(!memcmp("Author=", lineparser, 7))
						strncpy(cache[i].author, lineparser + 7, 32);

					if(!memcmp("Version=", lineparser, 8))
						strncpy(cache[i].version, lineparser + 8, 32);

					lineparser = strtok(NULL, "\n");
				}
			} else {
				hb_error(i, "Corrputed homebrew");
			}

			if(fsloadfile("ICON.BMP", data_buffer, sizeof(data_buffer)) > 0) {
				decode_bmp(data_buffer, i);
			} else {
				copy_bmp((uint16_t *) default_bmp, i);
			}

			fschdir("..");
		} else {
			hb_error(i, "Fatal error loading homebrew.");
		}

		cache[i].id = id;
	}
}

/**
  * @brief  Main menu loop.
  * @param  title: String to draw in the header.
  * @return -1 if B button pressed, otherwise the ID of the homebrew to load.
  */
int mainmenu(char *title) {
	int i;

	DirEntry *dir = fsreaddir(1, &maxselection);
	char buffer[16];

	selection = 0;
	scroll = 0;

	for(i = 0; i < 16 * 320; i++) framebuffer[i] = LCD_COLOR_GRAYSCALE(4);
	lcd_print_centered(title, 160, 4, 0xFFFF, LCD_COLOR_GRAYSCALE(4));

	for(i = 0; i < 3; i++) {
		cache[i].id = -1;

		fatname_to_filename((char *)(&dir[i]), buffer);
		load_hb_info(i, buffer);
	}

	while(1) {
		uint32_t buttons = buttons_get();

		if(buttons & B_Up) {
			selection--;
			if(selection == -1) {
				selection = (maxselection - 1);
				scroll = maxselection - 3;
				if(scroll < 0) scroll = 0;
				for(i = 0; i < 3; i++) {
					fatname_to_filename((char *)(&dir[scroll + i]), buffer);
					load_hb_info(scroll + i, buffer);
				}
			}
			
			if(selection - scroll == -1) {
				scroll--;
				fatname_to_filename((char *)(&dir[selection]), buffer);
				load_hb_info(selection, buffer);
			}
				
		}

		if(buttons & B_Down) {
			selection++;
			if(selection == maxselection) {
				selection = 0;
				scroll = 0;
				for(i = 0; i < 3; i++) {
					fatname_to_filename((char *)(&dir[i]), buffer);
					load_hb_info(i, buffer);
				}
			}
				
			if(selection - scroll == 3) {
				fatname_to_filename((char *)(&dir[selection]), buffer);
				load_hb_info(selection, buffer);
				scroll++;
			}
		}

		if(buttons & B_A) {
			free(dir);
			return selection;
		}
		
		if(buttons & B_B) {
			free(dir);
			return -1;
		}
		
		update_screen();
	}
}
