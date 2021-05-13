# QUICK HEADS-UP: THIS PROGRAM DOES NOT WORK YET. LIKE AT ALL.

# game-and-watch-bootloader

A free and open source homebrew launcher for the Game & Watch.

![GWLoader Main Menu](http://gw.prochazka.ml/menu.jpg)

_(note: this picture does not represent the current state of the project)_

This program replaces the original firmware and loads homebrew from a FAT16 filesystem located on the external flash into RAM. Use the [gwlink](https://github.com/prochazkaml/gwlink) program to mount the Game & Watch's filesystem on your local Linux computer using FUSE (TBD).

## Credits

This project contains different bits and pieces from the following projects:

- [game-and-watch-base](https://github.com/ghidraninja/game-and-watch-base)
- [game-and-watch-retro-go](https://github.com/kbeckmann/game-and-watch-retro-go)
- [game-and-watch-flashloader](https://github.com/ghidraninja/game-and-watch-flashloader)

## Instructions for building the project

### First-time setup

```
sudo cc tools/gwbin.c -o /usr/bin/gwbin
```

This command compiles and installs a small program which can _easily_ extract binary parts from an .elf file. There is probably a way to do this using objcopy, but I couldn't figure it out, so I wrote this tiny program instead.

### Compiling

```
make -j8
```

This command compiles all files and generates gw_boot.bin, which you can then flash onto your Game & Watch.

**It is highly recommended to use the latest version of the [GNU ARM Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) (≥ v10.2.1), since it produces smaller binaries than the one in the Debian/Ubuntu repos (v8.3.1).**

## Homebrew format

Each homebrew needs to be in its separate folder in the root directory of the external flash. Inside, there are 1-3 files:

### MAIN.BIN _(required)_

The homebrew program itself, which will be loaded into the Game & Watch RAM (up to 1 MB).

### MANIFEST.TXT _(not strictly required, but highly recommended)_

A simple text file describing different properties of the homebrew.

Example manifest file:

```
Name=Tapper
Author=Ben Heckendorn
Version=1.0
```

If you want to be really ugly, you can omit this file. It will say "Corrupted homebrew" in the main menu, but it will still boot. It is _highly_ recommended to include it though.

### ICON.BMP _(optional)_

A 64x48 16bpp icon, which will appear in the main menu. If it is not present, then a generic icon will be displayed instead.

## Features / TODO list

- [X] Functional UI
- [X] Filesystem library for reading
- [ ] Launching homebrew
- [ ] External flash formatting
- [ ] Filesystem library for writing
- [ ] PC link
- [ ] Adjustable backlight
- [ ] Battery indicator
