#include <stdint.h>
#include "common.h"

uint8_t *directory_names[1024];
uint8_t dir_buffer[16384];

uint8_t data_buffer[64 * 1024];

const char updir_name[] = "..";
const char intflash_name[] = "MAIN.BIN";
const char extflash_name[] = "EXTFLASH.BIN";
const char icon_name[] = "ICON.BMP";
const char manifest_name[] = "MANIFEST.TXT";
