extern uint32_t gwloader_comm_buf_word[4] __attribute__((section (".gwloader")));
#define gwloader_comm_buf_hword ((uint16_t *)gwloader_comm_buf_word)
#define gwloader_comm_buf ((uint8_t *)gwloader_comm_buf_word)

void gwloader_call(uint8_t call);
uint8_t gwloader_call_catcherr(uint8_t call);
void gwloader_call_nonblock(uint8_t call);
int gwloader_call_isdone();
int gwloader_call_isdone_ingore_missing();
