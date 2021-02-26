#include "main.h"

extern uint32_t hbloader_comm_buf_word[4] __attribute__((section (".hbloader")));
#define hbloader_comm_buf_hword ((uint16_t *)hbloader_comm_buf_word)
#define hbloader_comm_buf ((uint8_t *)hbloader_comm_buf_word)

void hbloader_call(uint8_t call);
uint8_t hbloader_call_catcherr(uint8_t call);
void hbloader_call_nonblock(uint8_t call);
int hbloader_call_isdone();
int hbloader_call_isdone_ingore_missing();
