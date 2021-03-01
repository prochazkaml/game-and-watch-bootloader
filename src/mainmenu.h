typedef struct {
	int id;
	int id_pending;
	int load_status;
	char name[32];
	char author[32];
	char version[32];
	uint16_t bitmap[64 * 48];
} HomebrewEntry;

extern HomebrewEntry cache[3];

void initmenu(int num_of_hb, int free, int capacity);
int mainmenu();
void mainmenu_finish();
