# _THIS SOFTWARE/HARDWARE IS STILL UNDER HEAVY DEVELOPMENT, NOT MEANT FOR DAILY USE!_

# game-and-watch-bootloader

The main menu program which is loaded onto the G&W when the button on the GWLoader is pressed.

![GWLoader Main Menu](http://gw.prochazka.ml/menu.jpg)

This program resides in RAM and does not touch the flash until it is told by the user to do so.

## Credits

The code is based on [game-and-watch-base](https://github.com/ghidraninja/game-and-watch-base). Internal flash writing routine based on an [example project from STM32CubeH7](https://github.com/STMicroelectronics/STM32CubeH7/tree/master/Projects/STM32H7B3I-DK/Examples/FLASH/FLASH_EraseProgram). External flash writing routine based on the [game-and-watch-flashloader](https://github.com/ghidraninja/game-and-watch-flashloader).

## What is the GWLoader?

It is (or rather, _will_ be, it's still under development) a small board containing a micro SD slot, a button and a small microcontroller (not sure whether I'm going to go with ARM or AVR), which fits inside the G&W behind its D-Pad. Its purpose is to eliminate the need to disassemble the G&W and hook up an STM32 programmer everytime the you want to load different homebrew onto it. Instead, with the GWLoader, you can just press its button, it will spring into life, reset the G&W's CPU and load this bootloader onto it, which will then display a nice menu. There, when a homebrew is selected, it is flashed onto the G&W's internal (and external, if necessary) flash and then run.

![3D render of the PCB design](http://gw.prochazka.ml/3drender.png)

3D render of the PCB design, subject to change. Designed in EasyEDA, available [here](https://oshwlab.com/Michal_Prochazka/gw_hbloader).

Sadly, this magical device does not exist yet. However, I have created an [emulator](https://github.com/prochazkaml/gwloader-emulator) in Python which communicates using OpenOCD. It was created only for development and testing purposes, it is as impractical as loading homebrew the usual way, since an OpenOCD-compatible programmer is still required to be connected.

## Instructions for building the project

### First-time setup

```
sudo cc tools/gwbin.c -o /usr/bin/gwbin
```

This command compiles and installs a small program which can _easily_ extract binary parts from an .elf file. There is probably a way to do this using objcopy, but I couldn't figure it out, so I wrote this tiny program instead.

### Compiling

```
make -j4
./makeloader.sh
```

The first command generates the .elf file. The second command splits the .elf file into two parts (ITCM and DTCM) and attaches a small assembly program to them, which automatically re-assembles the executable on startup and jumps to it. This eliminates the need for the GWLoader to be capable of decoding an .elf file, and also opens the possibility for executable compression, which I might try in the near future.

## Communication protocol

The G&W communicates with the GWLoader through a 16-byte buffer at the beginning of the G&W's RAM (addresses 0x24000000-0x2400000F). The first byte is the command/status byte, through which the G&W tells the GWLoader what operation to do, and the GWLoader puts there its status.

For more information about the communication protocol, click [here](https://github.com/prochazkaml/gwloader-emulator/blob/main/Protocol.md).

You, the programmer, won't have to worry about any of this though, because I'm planning to release a simple library and example code which can communicate with the GWLoader.

## Homebrew format

Each homebrew needs to be in its separate folder in the root directory of the SD card. Inside, there are 2-4 files:

### MANIFEST.TXT _(required)_

A simple text file describing different properties of the homebrew.

Example manifest file:

```
Name=Tapper
Author=Ben Heckendorn
Version=1.0
UsesFileAccess=False
```

If you want to be really ugly, you can omit this file. It will say "Corrupted homebrew" in the main menu, but it should still boot. It is _highly_ recommended to include it though.

Note: If UsesFileAccess is not present in the manifest, it is assumed that the value is supposed to be False.

### MAIN.BIN _(required)_

The homebrew program itself, which will be loaded onto the G&W's internal flash (up to 128 kB).

### EXTFLASH.BIN _(optional)_

Supplementary data for the homebrew, which will be loaded onto the G&W's external flash (up to 1 MB). Some homebrew do not use it, thus it is optional.

### ICON.BMP _(optional)_

A 64x48 16bpp icon, which will appear in the main menu. If it is not present, then a generic icon will be displayed instead.

## Features

- [X] Working UI
- [X] Internal flash writing
- [X] External flash writing
- [ ] Flash dumping
- [ ] Flash validation
- [ ] Homebrew list sorting
- [ ] Main executable compression
- [ ] File compression
- [ ] Support homebrew which use the GWLoader's file access functions
