/*
 * Generic FAT16 filesystem I/O library
 * Written by Michal Procházka, 2021.
 * 
 * Docs: http://www.maverick-os.dk/FileSystemFormats/FAT16_FileSystem.html
 */

#include "fslib.h"

//#define FSDEBUG

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <fnmatch.h>

#ifdef FSDEBUG
#include <stdio.h>
#define msg(...) printf("[FSLIB] "__VA_ARGS__)
#define err(...) { printf("[ERROR] "__VA_ARGS__); return 1; }
#define errptr(...) { printf("[ERROR] "__VA_ARGS__); return NULL; }
#else
#define msg(...)
#define err(...) return 1
#define errptr(...) return NULL
#endif

uint8_t *image;

BIOSParams fsinfo;

uint16_t *fat;
DirEntry *rootdir;

int currentdir, currentdirsize;
int datasector;
int freespace;

int fsgetfilesector(int startclust, int offset) {
	int i;

	for(i = 0; i < offset; i++) {
		if(startclust == 0xFFFF) return 0;
		startclust = fat[startclust];
	}

	return startclust + datasector - 2;
}

int fsgetclustlength(int startclust) {
	int i;

	for(i = 0; ; i++) {
		if(startclust >= 0xFFF8) return i;
		startclust = fat[startclust];
	}
}

DirEntry *fsreaddirentry(int id) {
	if(currentdir == 0)
		return &rootdir[id];
	else
		return &((DirEntry *)(image + fsgetfilesector(currentdir, id * sizeof(DirEntry) / fsinfo.BytesPerSector) * fsinfo.BytesPerSector))[id % (fsinfo.BytesPerSector / sizeof(DirEntry))];
}

void fatname_to_filename(char *src, char *dest) {
	int i;

	// Copy the base name

	for(i = 0; i < 8; i++)
		if(src[i] != ' ')
			*dest++ = src[i];
		else
			break;

	// If the file has an extension, add a period and copy the extension

	if(src[8] != ' ') {
		*dest++ = '.';

		for(i = 0; i < 3; i++)
			if(src[i + 8] != ' ')
				*dest++ = src[i + 8];
			else
				break;
	}

	// Zero-terminate the string

	*dest = 0;
}

void filename_to_fatname(char *src, char *dest) {
	int ptr = 0, i;

	// Special case for ".."

	if(!strcmp(src, "..")) {
		memcpy(dest, "..         ", 11);
		return;
	}

	// Copy the base name

	for(i = 0; i < 8; i++)
		if(src[ptr] != '.' && src[ptr] != 0)
			dest[i] = toupper(src[ptr++]);
		else
			break;

	// Fill the rest of the base name with blank space

	for(; i < 8; i++) dest[i] = ' ';

	// Skip the rest of the base name if it is larger than 8 chars

	while(src[ptr] != '.' && src[ptr] != 0) ptr++;

	// If the filename ends here, pad the extension with blank space

	if(src[ptr] == 0) {
		for(i = 8; i < 11; i++) dest[i] = ' ';
		return;
	}

	// Point to the source extension

	ptr++;

	// Copy the extension

	for(i = 8; i < 11; i++)
		if(src[ptr] != 0)
			dest[i] = toupper(src[ptr++]);
		else
			break;

	// Fill the rest of the extension with blank space

	for(; i < 11; i++) dest[i] = ' ';
}

void updatefreespace() {
	freespace = 0;
	
	int i;
	for(i = 0; i < fsinfo.LogicalSectors - datasector; i++) {
		if(fat[i + 2] == 0) freespace += fsinfo.BytesPerSector;
	}

	freespace /= 1024;
}

int fsgetfreespace() {
	return freespace;
}

int fsmount(uint8_t *fsimage) {
	// First of all, check if the compiler hadn't messed with our structs

	assert(sizeof(BIOSParams) == 59);
	assert(sizeof(DirEntry) == 32);

	// Copy the BIOS filesystem parameters

	memcpy(&fsinfo, fsimage + 3, sizeof(BIOSParams));

	msg("Mounting volume \"%.11s\", filesystem %.8s\n", fsinfo.VolumeLabel, fsinfo.FileSystem);
	msg("Disk contains %d sectors, %d bytes each.\n", fsinfo.LogicalSectors, fsinfo.BytesPerSector);

	// Check the validity of the filesystem

	if(memcmp(fsinfo.FileSystem, "FAT16   ", 8)) err("Incompatible filesystem!\n");
	if(fsinfo.NumberOfFats != 1) err("Only 1 FAT is supported!\n");
	if(fsinfo.SectorsPerCluster != 1) err("Only 1 sector per cluster is supported!\n");

	// Initialize some variables

	image = fsimage;

	fat = (uint16_t *)(image + fsinfo.ReservedSectors * fsinfo.BytesPerSector);
	rootdir = (DirEntry *)(image + (fsinfo.ReservedSectors + fsinfo.SectorsPerFat) * fsinfo.BytesPerSector);

	datasector = fsinfo.ReservedSectors + fsinfo.SectorsPerFat + (fsinfo.RootDirEntries * sizeof(DirEntry)) / fsinfo.BytesPerSector;
	if((fsinfo.RootDirEntries * sizeof(DirEntry)) % fsinfo.BytesPerSector) datasector++;

	msg("First usable cluster points to disk sector %d.\n\n", datasector);

	currentdir = 0;
	currentdirsize = fsinfo.RootDirEntries;

	updatefreespace();

	return 0;
}

DirEntry *fsreaddir(int dirs_only, int *entries) {
	DirEntry *output = malloc(currentdirsize * sizeof(DirEntry));

	msg("Allocated enough memory for %d entries\n", currentdirsize);

	int i, ptr = 0;
	char buf[16];
	DirEntry *tmp;

	for(i = 0; i < currentdirsize; i++) {
		tmp = fsreaddirentry(i);

		if(tmp->Basename[0] == 0) break;

		if((uint8_t)tmp->Basename[0] == 0xE5) continue;

		if(tmp->Attribute & 8) continue;

		fatname_to_filename((char *)tmp, buf);

		if(!dirs_only || (tmp->Attribute & 0x10))
			memcpy(output + ptr++, tmp, sizeof(DirEntry));
	}

	msg("Found %d entr%s matching \"%s\"\n\n", ptr, (ptr == 1) ? "y" : "ies", mask);

	*entries = ptr;
	return output;
}

DirEntry *fsfindfile(char *filename) {
	int i;
	char buf[16];
	DirEntry *tmp;

	filename_to_fatname(filename, buf);

	msg("Matching against FAT name \"%.11s\"...\n", buf);

	for(i = 0; i < currentdirsize; i++) {
		tmp = fsreaddirentry(i);

		if(tmp->Basename[0] == 0) break;

		if((uint8_t)tmp->Basename[0] == 0xE5) continue;

		if(tmp->Attribute & 8) continue;

		if(!memcmp(tmp, buf, 11))
			return tmp;
	}

	return NULL;
}

int fschdir(char *filename) {
	DirEntry *entry;

	msg("Searching for dir \"%s\"...\n", filename);

	if(!strcmp(filename, "/")) {
		msg("Override: enter root directory\n");

		currentdir = 0;
	} else if((entry = fsfindfile(filename)) != NULL) {
		if(!(entry->Attribute & 0x10)) err("\"%s\" is not a dir!\n", filename);

		currentdir = entry->StartCluster;
	} else err("Could not find dir \"%s\"!\n", filename);

	if(currentdir == 0)
		currentdirsize = fsinfo.RootDirEntries;
	else
		currentdirsize = fsgetclustlength(currentdir) * (fsinfo.BytesPerSector / sizeof(DirEntry));

	msg("Found dir \"%s\" on cluster %d, max %d entries\n", filename, currentdir, currentdirsize);
	return 0;
}

uint8_t *fsloadfile(char *filename, uint32_t *size) {
	DirEntry *entry;
	uint8_t *data;
	int i, j = 0, sect;

	msg("Searching for file \"%s\"...\n", filename);
	
	if((entry = fsfindfile(filename)) != NULL) {
		if(entry->Attribute & 0x10) errptr("\"%s\" is a dir, not a file!\n", filename);

		i = entry->Size;
		*size = i;
		data = malloc(i);

		sect = entry->StartCluster;

		while(i > 0) {
			memcpy(data + (j++) * fsinfo.BytesPerSector, image + (sect + datasector - 2) * fsinfo.BytesPerSector, (i > 4096) ? 4096 : i);

			i -= fsinfo.BytesPerSector;

			sect = fat[sect];
		}

		return data;
	} else errptr("Could not find dir \"%s\"!\n", filename);
}

int fswritefile(char *filename, uint8_t *data, uint32_t size) {
	return 1;
}

int fsdeletefile(char *filename) {
	return 1;
}

