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

### Prerequisites

- Intel VT-x/AMD-V enabled in your BIOS settings (required to virtualize any 64-bit operating system properly).
- A brain.

### Unix-like Systems

The source code and binary files for ZenithOS can be found in the `src/` directory, and are assembled into a `ZenithOS.hdd` disk image by running `make` for a fat32 disk or `make echfs` for an echfs disk. After this, you can do `make run` to run the image in QEMU, or more preferably, set up a VM in VirtualBox or VMWare, select `ZenithOS.hdd` as the hard disk image, and boot the VM.

### Windows
You can also build the disk image on Windows (I personally think it's a more robust system). You will need Git installed and usable through the terminal. You will need to be on an edition of Windows that is not Home Edition in order to enable the needed Hyper-V disk image tools. There are multiple ways to upgrade to Professional or Education Edition you can find with a quick web search. You should also enable Developer mode under `Settings -> Developer Settings`. The required setting is under "Powershell", to enable execution of local unsigned Powershell scripts.

The Powershell script `make.ps1` must be run from an elevated Powershell process. It will guide you through enabling the two needed Hyper-V features. Avoiding enabling the other Hyper-V features is recommended, because if they are enabled (specifically the hypervisor), it will force any other virtualization software (like VirtualBox) to use the native Hyper-V API, which is terribly slow.

After cloning the repo, run `.\make.ps1 get-deps`. This will clone [qloader2](https://github.com/qloader2/qloader2) into the repo. You will need a compiler to create the `qloader2-install` executable in order to write the bootloader to the disk image. If you have Visual Studio with the C++ workload installed, it would be as simple as launching the x64/x86 Native Tools Command Prompt, going to the qloader2 directory, and running `cl qloader2-install.c`. If you wish to avoid pulling in the entirety of Visual Studio, you could look into installing just the [Visual C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/), and if you want to avoid having MSVC on your machine altogether, the program is in ANSI C, so any C compiler would work. I have tested both [clang](https://releases.llvm.org/download.html) and [tcc](https://bellard.org/tcc/) for this purpose. Avoid GNU bloat.

 Once you have the executable, you can just run `.\make.ps1` to create the FAT32 disk image.
 
 ```make.ps1 create <size=2GB> <driveletter=Z>```

 You can specify sizes using `KB`, `MB`, `GB`, `TB` postfixes:

 ```powershell
 .\make.ps1 create 20MB #creates a 20MiB VHD
 ```

 There are more commands in the PS script, you should take a peek. I suggest running VSCode as Administrator and having a `tasks.json` file with the script commands assigned to various keybindings.

#### VirtualBox

If you are using VirtualBox, you should put the install directory of VirtualBox (usually `C:\Program Files\Oracle\VirtualBox`) in your PATH for a couple of reasons. After generating a disk image, attach it to your VM. 
VirtualBox stores the UUID of that disk image in its internal volume manager. When you generate a new image, it has a new UUID, and VirtualBox refuses to run because the UUID of the disk has changed. The first reason is so that the script can reuse the same UUID everytime a new disk is generated. This way, VirtualBox won't complain that the attached disk has the wrong UUID. The second reason is so that you can use the `run` command in the script to launch the VM from the terminal.

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
