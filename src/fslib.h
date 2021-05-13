#include <stdint.h>

typedef struct __attribute__((__packed__)) {
	char OEMLabel[8];
	uint16_t BytesPerSector;
	uint8_t SectorsPerCluster;
	uint16_t ReservedSectors;
	uint8_t NumberOfFats;
	uint16_t RootDirEntries;
	uint16_t LogicalSectors;
	uint8_t MediumType;
	uint16_t SectorsPerFat;
	uint16_t SectorsPerTrack;
	uint16_t Sides;
	uint32_t HiddenSectors;
	uint32_t LargeSectors;
	uint16_t DriveNo;
	uint8_t Signature;
	uint32_t VolumeID;
	char VolumeLabel[11];
	char FileSystem[8];
} BIOSParams;

typedef struct __attribute__((__packed__)) {
	char Basename[8];
	char Extension[3];
	uint8_t Attribute;
	uint8_t Reserved8;
	uint8_t CreationMs;
	uint16_t CreationTime;
	uint16_t CreationDate;
	uint16_t LastAccessDate;
	uint16_t Reserved16;
	uint16_t LastWriteTime;
	uint16_t LastWriteDate;
	uint16_t StartCluster;
	uint32_t Size;
} DirEntry;

int fsmount(uint8_t *fsimage);
long fsloadfile(char *filename, uint8_t *buffer, uint32_t maxsize);
int fswritefile(char *filename, uint8_t *data, uint32_t size);
int fsdeletefile(char *filename);
DirEntry *fsreaddir(int dirs_only, int *entries);
int fschdir(char *filename);
int fsgetfreespace();

void fatname_to_filename(char *src, char *dest);
void filename_to_fatname(char *src, char *dest);

