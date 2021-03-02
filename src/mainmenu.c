#include <stdio.h>
#include <string.h>

#include "mainmenu.h"
#include "buttons.h"
#include "lcd.h"
#include "gwloader.h"
#include "common.h"

#include "default.h"
#include "hourglass.h"

HomebrewEntry cache[3];

int selection, maxselection, scroll;
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
		lcd_draw_icon(cache[j].bitmap, 10, 24 + i * 72);
		
		lcd_print(cache[j].name, 80, 28 + i * 72, 0xFFFF, 0x0000);
		lcd_print(cache[j].author, 80, 44 + i * 72, LCD_COLOR_GRAYSCALE(24), 0x0000);
		lcd_print(cache[j].version, 80, 60 + i * 72, LCD_COLOR_GRAYSCALE(24), 0x0000);
	}

	lcd_update();
}

void decode_bmp(unsigned char *bmp, int id) {
	id %= 3;
	
	unsigned char *imgdata = bmp + hourglass_bmp[0x0A];
	int i;
	
	for(i = 0; i < 64 * 48; i++) {
		cache[id].bitmap[i] = imgdata[i * 2] | (imgdata[i * 2 + 1] << 8);
	}
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

void mainmenu_finish() {
	while(cache[0].load_status || cache[0].id_pending >= 0 ||
			cache[1].load_status || cache[1].id_pending >= 0 ||
			cache[2].load_status || cache[2].id_pending >= 0) update_hb_info();
}

void initmenu(int num_of_hb, int capacity, int free) {
	int i;
	
	selection = 0;
	scroll = 0;
	card_capacity_mb = capacity;
	card_free_mb = free;
	maxselection = num_of_hb;

	for(i = 0; i < 3; i++) cache[i].load_status = 0, cache[i].id = -1, cache[i].id_pending = -1, init_hb_info(i);
}

int mainmenu(char *title) {
	int i;
	
	for(i = 0; i < 16 * 320; i++) framebuffer[i] = LCD_COLOR_GRAYSCALE(4);
	lcd_print_centered(title, 160, 4, 0xFFFF, LCD_COLOR_GRAYSCALE(4));

	while(1) {
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
			return selection;
		}
		
		update_screen();
		update_hb_info();
	}
}
