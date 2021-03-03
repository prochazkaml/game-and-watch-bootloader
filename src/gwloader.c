#include "stm32h7xx_hal.h"
#include "gwloader.h"
#include "lcd.h"
#include "buttons.h"
#include <stdio.h>

uint32_t gwloader_comm_buf_word[4];

/**
  * @brief  Display an error screen and loop forever.
  * @param  call: GWLoader command ID.
  * @return This function should never return.
  */
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

/**
  * @brief  Call a GWLoader function (blocking, handles all errors).
  * @param  call: GWLoader command ID.
  * @return Nothing.
  */
void gwloader_call(uint8_t call) {
	gwloader_comm_buf[0] = call;
	
	while(gwloader_comm_buf[0] == call);
	
	if(gwloader_comm_buf[0] != GWL_STATUS_PROCESSING && gwloader_comm_buf[0] != GWL_STATUS_OK) gwloader_error(call);
	
	while(gwloader_comm_buf[0] == GWL_STATUS_PROCESSING);

	if(gwloader_comm_buf[0] != GWL_STATUS_OK) gwloader_error(call);
}

/**
  * @brief  Call a GWLoader function (blocking, returns error status).
  * @param  call: GWLoader command ID.
  * @return GWLoader error status.
  */
uint8_t gwloader_call_catcherr(uint8_t call) {
	gwloader_comm_buf[0] = call;
	
	while(gwloader_comm_buf[0] == call);
	
	if(gwloader_comm_buf[0] != GWL_STATUS_PROCESSING && gwloader_comm_buf[0] != GWL_STATUS_OK) return gwloader_comm_buf[0];
	
	while(gwloader_comm_buf[0] == GWL_STATUS_PROCESSING);

	if(gwloader_comm_buf[0] != GWL_STATUS_OK) return gwloader_comm_buf[0];
	
	return 0;
}

uint8_t gwloader_nonblock_call_id;

/**
  * @brief  Call a GWLoader function (nonblocking).
  * @param  call: GWLoader command ID.
  * @return Nothing.
  */
void gwloader_call_nonblock(uint8_t call) {
	gwloader_comm_buf[0] = call;
	gwloader_nonblock_call_id = call;
}

/**
  * @brief  Check if a GWLoader function has finished (handles all errors).
  * @return 1 if it has, 0 if it has not.
  */
int gwloader_call_isdone() {
	uint8_t val = gwloader_comm_buf[0];
	
	if(val == GWL_STATUS_OK) return 1;
	
	if(GWL_ISERR(val)) gwloader_error(gwloader_nonblock_call_id);
	
	return 0;
}

/**
  * @brief  Check if a GWLoader function has finished
  *         (handles all errors except File not found,
  *         which is treated the same as if it has not finished).
  * @return 1 if it has, 0 if it has not.
  */
int gwloader_call_isdone_ingore_missing() {
	uint8_t val = gwloader_comm_buf[0];
	
	if(val == GWL_STATUS_OK) return 1;
	
	if(val == GWL_ERR_FILE_NOT_FOUND) return 0;
	
	if(GWL_ISERR(val)) gwloader_error(gwloader_nonblock_call_id);
	
	return 0;
}
