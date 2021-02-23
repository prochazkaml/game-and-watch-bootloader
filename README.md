# game-and-watch-bootloader

The program which is loaded onto the G&W when the button on the module is pressed.

## First-time setup

```
sudo cc tools/gwbin.c -o /usr/bin/gwbin
```

This command compiles and installs a small program which can _easily_ extract binary parts from an .elf file. There is probably a way to do this using objcopy, but I couldn't figure it out.

## How to build

```
make -j4
./makeloader.sh
```

The first command generates the .elf file. The second command splits the .elf file into two parts (ITCM and DTCM) and attaches a small assembly program to them, which automatically re-assembles the executable and jumps to it. This eliminates the need for the host to be capable of decoding an .elf file, and also opens the possibility for executable compression, which I might try in the near future.
