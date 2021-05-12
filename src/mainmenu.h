typedef struct {
	int id;
	char name[32];
	char author[32];
	char version[32];
	uint16_t bitmap[64 * 48];
} HomebrewEntry;

extern HomebrewEntry cache[3];

int mainmenu(char *title);
