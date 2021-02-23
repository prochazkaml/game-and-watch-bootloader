# game-and-watch-bootloader

The program which is loaded onto the G&W when the button on the GWLoader is pressed.

## What is the GWLoader?

It is (or rather, _will_ be, it's still under development) a small board containing a micro SD slot, a button and a small microcontroller (probably an ATtiny), which fits inside the G&W. Its purpose is to eliminate the need to disassemble the G&W and hook up an STM32 programmer everytime the user wants to load different homebrew onto it. Instead, with the GWLoader, when its button is pressed, it springs into life, resets the G&W's CPU and loads this bootloader onto it, which then displays a nice menu to the user. There, when a homebrew is selected, it is flashed onto the G&W's internal (and external, if the homebrew needs it) flash and then run.

Sadly, this magical device does not exist yet. However, I have created an [emulator](https://github.com/prochazkaml/gwloader-emulator). It was created only for development and testing purposes, it is as impractical as loading homebrew the usual way, since an OpenOCD-compatible programmer is still required to be connected.

## Instructions for building the project

### First-time setup

```
sudo cc tools/gwbin.c -o /usr/bin/gwbin
```

This command compiles and installs a small program which can _easily_ extract binary parts from an .elf file. There is probably a way to do this using objcopy, but I couldn't figure it out.

### Compiling

```
make -j4
./makeloader.sh
```

The first command generates the .elf file. The second command splits the .elf file into two parts (ITCM and DTCM) and attaches a small assembly program to them, which automatically re-assembles the executable and jumps to it. This eliminates the need for the host to be capable of decoding an .elf file, and also opens the possibility for executable compression, which I might try in the near future.

## TO-DO list

- [X] Create a functional UI
- [X] External flash writing
- [ ] Flash dumping
- [ ] Internal flash writing
- [ ] Support homebrew which use the GWLoader's file access functions

## Communication protocol

The G&W communicates with the GWLoader through a 16-byte buffer at the beginning of the G&W's RAM (addresses 0x24000000-0x2400000F). The first byte is the command/status byte, through which the G&W tells the GWLoader what operation to do, and the GWLoader puts there its status.

Learn more [here](https://github.com/prochazkaml/gwloader-emulator/blob/main/Protocol.md).

You, the programmer, won't have to worry about any of this though, because I'm planning to release a simple library and example code which can communicate to the GWLoader.
