#define GWL_STATUS_OK 0x00
#define GWL_STATUS_PROCESSING 0xFF
#define GWL_ERR_NO_STORAGE 0x80
#define GWL_ERR_FILE_NOT_FOUND 0x81
#define GWL_ERR_NOT_ENOUGH_SPACE 0x82
#define GWL_ERR_IO_ERROR 0x83
#define GWL_ERR_FILE_ALREADY_OPEN 0x84
#define GWL_ERR_UNKNOWN_WRITE_MODE 0x85

#define GWL_ISERR(val) (val >= 0x80 && val < 0xFF)

#define GWL_DETECTION_CHECK 0x01
#define GWL_OPEN_READ 0x02
#define GWL_OPEN_WRITE 0x03
#define GWL_READ 0x04
#define GWL_WRITE 0x05
#define GWL_CLOSE 0x06
#define GWL_TELL 0x07
#define GWL_SEEK_START 0x08
#define GWL_SEEK_END 0x09
#define GWL_SEEK_OFFSET 0x0A
#define GWL_SEEK_POSITION 0x0B
#define GWL_CHDIR 0x0C
#define GWL_READ_DIR 0x0D
#define GWL_HALT 0x7D
#define GWL_RESET 0x7E
#define GWL_RESET_HALT 0x7F

extern uint32_t gwloader_comm_buf_word[4] __attribute__((section (".gwloader")));
#define gwloader_comm_buf_hword ((uint16_t *)gwloader_comm_buf_word)
#define gwloader_comm_buf ((uint8_t *)gwloader_comm_buf_word)

void gwloader_call(uint8_t call);
uint8_t gwloader_call_catcherr(uint8_t call);
void gwloader_call_nonblock(uint8_t call);
int gwloader_call_isdone();
int gwloader_call_isdone_ingore_missing();
