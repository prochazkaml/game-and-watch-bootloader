#!/bin/bash

# Write initial program position
echo "init_pc: .long `arm-none-eabi-objdump -f build/gw_base.elf | grep "start address" | awk '{print $3}'`" > build/data.s

# Extract ITCM and DTCM data from the elf file (compile gwbin from the "tools" directory)
gwbin build/gw_base.elf build/itcm.bin 0x00000000 0x00010000
gwbin build/gw_base.elf build/dtcm.bin 0x20000000 0x20020000

# Compress the extracted data (https://github.com/lz4/lz4)
#lz4 -f -9 -m build/dtcm.bin build/itcm.bin

# Generate an assembly file from the compressed data
echo "itcm_data: .align 2" >> build/data.s
xxd -ps build/itcm.bin | sed 's/.\{2\}/, 0x&/g' | sed 's/^,/.byte/' >> build/data.s
echo "itcm_data_end:" >> build/data.s
echo "dtcm_data: .align 2" >> build/data.s
xxd -ps build/dtcm.bin | sed 's/.\{2\}/, 0x&/g' | sed 's/^,/.byte/' >> build/data.s
echo "dtcm_data_end:" >> build/data.s

# Assemble!
arm-none-eabi-as src/loader.s -o build/loader.o
arm-none-eabi-ld -Ttext 0x24000010 build/loader.o -o build/loader.elf
arm-none-eabi-objcopy -O binary build/loader.elf build/LOADER.BIN

# To disassemble, run:
# arm-none-eabi-objdump -D build/loader.elf -marm -Mforce-thumb
