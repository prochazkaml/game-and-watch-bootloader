#include "config.h"
#include "stm32.h"
#include <assert.h>

uint32_t reg;
SystemConfig *syscfg = (SystemConfig *) &reg;

void config_init() {
	assert(sizeof(SystemConfig) == 4);

	reg = rtc_readreg(CFG_REG);

	if(syscfg->Magic != 0x6502) {
		syscfg->Magic = 0x6502;
		syscfg->Brightness = 4;
		config_update();
	}
}

void config_update() {
	rtc_writereg(CFG_REG, reg);
}
