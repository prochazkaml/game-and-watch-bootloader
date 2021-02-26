#include "hbloader.h"
#include "lcd.h"
#include "buttons.h"
#include <stdio.h>

uint32_t hbloader_comm_buf_word[4];

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
