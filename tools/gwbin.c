#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define get16b(ptr, index) (ptr[index] | (ptr[index + 1] << 8))
#define get32b(ptr, index) (ptr[index] | (ptr[index + 1] << 8) | (ptr[index + 2] << 16) + (ptr[index + 3] << 24))

unsigned char *loadFile(char *filename) {
	FILE *f = fopen(filename, "rb");

	int loadedFileSize;

	if(f == NULL) {
		printf("Error opening file %s!\n", filename);
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	loadedFileSize = ftell(f);
	fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

	unsigned char *data = (unsigned char *) malloc(loadedFileSize);
	fread(data, 1, loadedFileSize, f);
	fclose(f);

	return data;
}

int main(int argc, char *argv[]) {
	if(argc != 3 && argc != 5) {
		printf("Usage: %s inputelf outputbin <baseaddr (default 0x08000000) topaddr (default 0x08020000)>\n", argv[0]);
		exit(0);
	}

	int baseaddr, maxaddr;
	
	if(argc == 5) {
		baseaddr = strtol(argv[3], NULL, 16);
		maxaddr = strtol(argv[4], NULL, 16);
	} else {
		baseaddr = 0x08000000;
		maxaddr = 0x08020000;
	}
	
	printf("Base address 0x%08X, top address 0x%08X.\n", baseaddr, maxaddr);
	
	unsigned char *elf = loadFile(argv[1]);

	printf("Loaded %s.\n", argv[1]);

	if(memcmp(elf, "\x7F""ELF""\x01\x01\x01", 7) || elf[0x12] != 0x28) {
		printf("Error: %s is not an STM32 ELF!\n", argv[1]);
		exit(1);
	}

	printf("Verified STM32 ELF.\n");

	FILE *outfile = fopen(argv[2], "wb");
	unsigned char *out = malloc(maxaddr - baseaddr);

	if(outfile == NULL) {
		printf("Error opening file %s for writing!\n", argv[2]);
		exit(1);
	}

	printf("Opened %s for writing.\n", argv[2]);

	int phoff = get32b(elf, 0x1C);
	int phentsize = get16b(elf, 0x2A);
	int phnum = get16b(elf, 0x2C);

	printf("%d segment entries.\n", phnum);

	int i, p_offset, p_paddr, p_filesz, filesize = 0;

	for(i = 0; i < phnum; i++) {
		printf("\nEntry %d:\n", i);

		p_offset = get32b(elf, phoff + 0x4);
		p_paddr = get32b(elf, phoff + 0xC);
		p_filesz = get32b(elf, phoff + 0x10);

		printf("ELF location: 0x%06X, dest addr 0x%08X, length 0x%06X\n", p_offset, p_paddr, p_filesz);

		if(p_paddr < baseaddr || p_paddr >= maxaddr) {
			printf("Segment not in range. Ignoring.\n");
		} else {
			memcpy(out + p_paddr - baseaddr, elf + p_offset, p_filesz);
			printf("Written %d bytes to output @ location 0x%05X.\n", p_filesz, p_paddr - baseaddr);
			if(p_paddr - baseaddr + p_filesz > filesize) {
				printf("Increased output file size to %d bytes.\n", filesize = p_paddr - baseaddr + p_filesz);
			}
		}

		phoff += phentsize;
	}

	printf("Saving %d bytes to file %s.\n", filesize, argv[2]);
	fwrite(out, 1, filesize, outfile);

	free(elf);
	free(out);
	fclose(outfile);
}
