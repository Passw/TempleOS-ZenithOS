<a href="https://discord.gg/kkNuhNn"><img alt="Discord" src="https://img.shields.io/discord/661062825027829770?style=flat-square"></a>
![Website](https://img.shields.io/website?down_color=lightgray&down_message=offline&style=flat-square&up_color=green&up_message=online&url=https%3A%2F%2Fzenithos.org)
![goto counter](https://img.shields.io/github/search/VoidNV/ZenithOS/goto?style=flat-square)
# ZenithOS

The Zenith Operating System is a modernized, professional fork of the 64-bit Temple Operating System. It is a direct reaction against Jewish technological subversion, and also against the corporate takeover of free and open source software to be used as a mouthpiece for furthering their political agenda.

Features in development include:
  - Fully-functional AHCI support.
  - ~~VBE support~~ 32-bit color VBE graphics.
  - A new GUI framework in 32-bit color.
  - Compiler optimizations for speed improvements.
  - SSE2+ instruction support in compiler and assembler.
  - Network card drivers and a networking stack.

![](/screenshots/screenshot2.png)

## Getting started

See the [Releases](https://github.com/VoidNV/ZenithOS/releases) page for the latest stable release. As ZenithOS is in heavy development the last release may be quite behind from master. Alternatively, you can do the following and build an ISO from inside the OS, export it, and use that.

The source code and binary files for ZenithOS can be found in the `src/` directory, and are assembled into a `ZenithOS.hdd` disk image by running `make` for a fat32 disk or `make echfs` for an echfs disk. After this, you can do `make run` to run the image in QEMU, or more preferably, set up a VM in VirtualBox or VMWare, select `ZenithOS.hdd` as the hard disk image, and boot the VM.

### Contributing

You can contribute to the repository from inside or outside the OS.

To make changes outside the OS, you can edit files in the `src/` directory, and changes will be reflected in the OS image the next time `make` is executed. The exception to this is files related to the Kernel or the Compiler, as both are compiled when `BootHDIns;` is run within ZenithOS. In these cases, you must make changes within the OS.

To make changes within the OS, boot up the VM and edit files as necessary. If modifying Kernel files, recompile with `BootHDIns;`. if modifying Compiler files, recompile with `CompComp;`. Reboot to see if everything is fine before powering-off the VM. When finished, run `make export-fat32` or `make export-echfs` (depending on which filesystem you are using) to pull the files from `ZenithOS.hdd` into the `src/` directory.

## Background

In around November of 2019, I decided I wanted to continue Terry's work in a direction that would make it a viable operating system while still keeping the innovative and frankly, divine-intellect ideas and design strategies intact.

At first, I was developing exclusively inside a VM and occasionally generating ISOs as official releases. This was not a good approach, as things broke and I had no way of telling which changes caused what. So I decided to scrap that and restart from scratch.\
Releases of the "old" Zenith are currently archived on the `mega.nz` website:
  - [Previous Releases](https://mega.nz/#F!ZIEGmSRQ!qvL6Wk6THzE-dazkfT6N3Q)

Changes include:
  - 60FPS.
  - VBE graphics with variable resolutions.
  - 440Hz 'A' tuning changed to 432Hz.
  - HolyC -> CosmiC.
  - System-wide renaming for clarity
  - No weird shift-space mechanism
  - Caps Lock is reassigned as Backspace
  - Reformatted code for readability
  - Added comments and documentation

## Screenshots

System Report, Z Splash and AutoComplete, with Stars wallpaper

![](/screenshots/screenshot3.png)

32-bit color!

![](/screenshots/screenshot1.png)
