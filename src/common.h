extern uint8_t *directory_names[1024];
extern uint8_t dir_buffer[16384] __attribute__((section (".databuf")));

extern uint8_t data_buffer[64 * 1024] __attribute__((section (".databuf")));

extern const char updir_name[];
extern const char intflash_name[];
extern const char extflash_name[];
extern const char icon_name[];
extern const char manifest_name[];
