#include "submenu.h"

#include "stm32.h"
#include "lcd.h"
#include "gwloader.h"
#include "buttons.h"
#include "flashload.h"

#define SUBMENU_ENTRIES 4
#define MENU_WIDTH 128

char *submenu_names[SUBMENU_ENTRIES] = {
	"Dump flash", "Battery status", "Reboot device", "Suspend device"
};

/**
  * @brief  Popup menu loop.
  * @param  title: String to draw in the header.
  * @return Nothing.
  */
void submenu() {
	lcd_fade();
	
	int i, selection = 0;
	
	while(1) {
		uint32_t buttons = buttons_get();

		if(buttons & B_Up) {
			selection--;
			if(selection < 0) selection = SUBMENU_ENTRIES - 1;
		}
		
		if(buttons & B_Down) {
			selection++;
			if(selection >= SUBMENU_ENTRIES) selection = 0;
		}
		
		if(buttons & B_B) {
			return;
		}
		
		if(buttons & B_A) {
			switch(selection) {
				case 0:
					start_dump_process();
					return;
				
				case 2:
					gwloader_call(0x7F);
					break;
				
				case 3:
					GW_Sleep();
					break;
			}
		}
		
		lcd_draw_window(MENU_WIDTH, 16 + 8 * SUBMENU_ENTRIES);
		
		for(i = 0; i < SUBMENU_ENTRIES; i++) {
			lcd_print(submenu_names[i], 160 - MENU_WIDTH / 2 + 8, 120 - SUBMENU_ENTRIES * 4 + i * 8,
					  (i == selection) ? 0xFFFF : LCD_COLOR_GRAYSCALE(10), LCD_COLOR_GRAYSCALE(4));
		}
		
		lcd_update();
	}
}
