#include "stm32h7xx_hal.h"
#include "gwloader.h"
#include "lcd.h"
#include "buttons.h"
#include <stdio.h>

uint32_t gwloader_comm_buf_word[4];

void gwloader_error(uint8_t call) {
	char buffer[64];
	int i, frame = 0;
	
	while(1) {
		for(i = 0; i < 320 * 240; i++) {
			framebuffer[i] = 0x001F;
		}
		
		sprintf(buffer, "Yikes! Function 0x%02X, error 0x%02X!", call, gwloader_comm_buf[0]);
		lcd_print_centered(buffer, 160, 116, 0xFFFF, 0x001F);
		
		lcd_print_rtl("sorry, mate", 312, 224, 0xFFFF, 0x001F);
		
		frame++;
		sprintf(buffer, "%d", frame);
		lcd_print(buffer, 8, 224, 0xFFFF, 0x001F);
		
		lcd_update();
		buttons_get();
	}
}

void gwloader_call(uint8_t call) {
	gwloader_comm_buf[0] = call;
	
	while(gwloader_comm_buf[0] == call);
	
	if(gwloader_comm_buf[0] != 0xFF && gwloader_comm_buf[0] != 0x00) gwloader_error(call);
	
	while(gwloader_comm_buf[0] == 0xFF);

	if(gwloader_comm_buf[0] != 0x00) gwloader_error(call);
}

uint8_t gwloader_call_catcherr(uint8_t call) {
	gwloader_comm_buf[0] = call;
	
	while(gwloader_comm_buf[0] == call);
	
	if(gwloader_comm_buf[0] != 0xFF && gwloader_comm_buf[0] != 0x00) return gwloader_comm_buf[0];
	
	while(gwloader_comm_buf[0] == 0xFF);

	if(gwloader_comm_buf[0] != 0x00) return gwloader_comm_buf[0];
	
	return 0;
}

uint8_t gwloader_nonblock_call_id;

void gwloader_call_nonblock(uint8_t call) {
	gwloader_comm_buf[0] = call;
	gwloader_nonblock_call_id = call;
}

int gwloader_call_isdone() {
	uint8_t val = gwloader_comm_buf[0];
	
	if(val == 0x00) return 1;
	
	if(val >= 0x80 && val < 0xFF) gwloader_error(gwloader_nonblock_call_id);
	
	return 0;
}

int gwloader_call_isdone_ingore_missing() {
	uint8_t val = gwloader_comm_buf[0];
	
	if(val == 0x00) return 1;
	
	if(val == 0x81) return 0;
	
	if(val >= 0x80 && val < 0xFF) gwloader_error(gwloader_nonblock_call_id);
	
	return 0;
}
