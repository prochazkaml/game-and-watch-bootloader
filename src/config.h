#include <stdint.h>

typedef struct {
	uint16_t Magic;
	uint32_t Brightness : 3;
} SystemConfig;

extern SystemConfig *syscfg;

#define CFG_REG 0

#define CFG_BACKLIGHT 0x01

void config_init();
void config_update();
