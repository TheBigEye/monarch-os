<!-- Logo -->
<p align="center">
    <a href="https://github.com/TheBigEye/Monarch-OS#gh-light-mode-only"> <!-- if light mode -->
        <img width="42%" src="https://github.com/TheBigEye/TheBigEye/blob/main/assets/projects/Monarch-OS/Light-header.svg?raw=true" alt="Light mode Monarch OS logo!"/>
    </a>
    <a href="https://github.com/TheBigEye/Monarch-OS#gh-dark-mode-only"> <!-- if dark mode -->
        <img width="42%" src="https://github.com/TheBigEye/TheBigEye/blob/main/assets/projects/Monarch-OS/Dark-header.svg?raw=true" alt="Dark mode Monarch OS logo!!"/>
    </a>
</p>

<!-- Badges -->
<p align="center">
     <a href="https://github.com/TheBigEye#gh-light-mode-only"> <!-- if light mode -->
          <img src="https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=FF9641&color=4f4f4f" title="Made with C"/>
          <img src="https://komarev.com/ghpvc/?username=Eye-Monarch-OS&label=Views&color=FF9641&style=for-the-badge" title="Views" />
          <img src="https://img.shields.io/badge/VSCode-0078D4?style=for-the-badge&logo=visual%20studio%20code&logoColor=FF9641&color=4f4f4f" title="Programmed using VScode"/>
     </a>
     <a href="https://github.com/TheBigEye#gh-dark-mode-only"> <!-- if dark mode -->
          <img src="https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=FF9641&color=4f4f4f" title="Made with C"/>
          <img src="https://komarev.com/ghpvc/?username=Eye-Monarch-OS&label=Views&color=000000&style=for-the-badge" title="Views"/>
          <img src="https://img.shields.io/badge/VSCode-0078D4?style=for-the-badge&logo=visual%20studio%20code&logoColor=FF9641&color=000000" title="Programmed using VScode"/>
     </a>
</p>


**Monarch OS** is an simple Operating System designed to run on 32-bit architectures, supports VGA 90x60 text mode and 640x480 4bpp graphic (planar) mode, it is composed of a Kernel (as the main program) and GRUB Legacy as bootloader, it also contains a Memory manager, PC Speaker drivers and a Commands interpreter.

It has been created from scratch using some videos from Youtube as a base, and https://wiki.osdev.org, with the aim of trying to make a real OS, currently still under development :)

## So, why do I do this?
**The reason is simple:** the pure passion for creating and the challenge it entails. Although this project may be a minimalist kernel, and its practical utility may be _questionable_, it turns out to be enjoyable. First, there’s _the gratification of building something from scratch_, understanding every bit and byte that constitutes the heart of a PC. It’s also _an excellent way to learn and refine low-level concepts and computer system architecture_. Of course, **the disadvantages are evident:** it’s a journey filled with complexities, frustrations, and errors that could even overwhelm the most experienced developer. However, even in those moments of despair, there’s an intrinsic beauty in the learning process and problem-solving that only true enthusiasts can appreciate. After all, _isn’t that the true essence of programming?_.

## How do I build this?
To build, you will need NASM, GCC and a 32 bit processor, **IMPORTANT** we will need a UNIX environment, to use tools such as QEMU, make, xorriso, some bash features and the C cross compiler

### MSYS2 (Windows):
- Download and install [**MSYS2**](https://www.msys2.org/), once installed and updated, you will need to install some packages under the **MINGW32 shell**, `pacman -S mingw-w64-i686-gcc mingw-w64-i686-nasm mingw-w64-i686-make mingw-w64-i686-qemu xorriso`, once the packages are installed, you must add `C:\msys64\mingw32\bin` to the Windows PATH so that it can be used externally, for that, you open the taskbar search box and type `env`, you should see the option to edit system environment variables, in the pop-up window you click `Environment variables`, in the next window you select `Path` of System section, and there you add that path to the bin folder, now typing `gcc --version` in CMD should work :)
- And... what happens if I want to use a cross-compiler like GCC-ELF?, To avoid complications, download the zip corresponding to your architecture and system from [**here**](https://github.com/lordmilko/i686-elf-tools/releases/tag/7.1.0) (remember x86_64 is 64 bit, and i686 is 32 bit, in this case **we are using 32 bit**), once downloaded, go to the following path `C:\msys64\mingw32`, drag the zip folders into the mingw32 folder, and that will be it, you can try running `i686-elf-gcc --version` from CMD to check if works.
- **NOTE**: You also need to have **python** and **pillow** installed, or the Makefile will fail when it needs to process the binaries.

Once the compilers and tools are configured, under the **MINGW32 shell**, inside the project directory, type `make` to build the project, it should generate an ISO image and launch QEMU

### Arch Linux
- The following packages are installed: `i686-elf-gcc` `nasm` `make` `xorriso` `qemu-full` `python` `python-pillow`, (it's recommended to use **YAY** or **PARU** instead of **PACMAN**, since the cross-compiler is only available from the AUR), There's no need to manually configure environment variables here—everything is automatic :D.

Then, inside the project directory, type `make` to build the project. It should generate an ISO image and launch QEMU.

## How do I run this?
To run the system it is necessary to use virtualization programs, such as Virtual Box, VMware, QEMU or Bochs (in theory it works on real hardware, but it is not recommended)

<!-- -------------------------------------------------------------------------- Credits ------------------------------------------------------------------------------>
<!-- Header and footer svgs --- kyechan99/capsule-render -->
<!-- Views counter --- antonkomarev/github-profile-views-counter -->
<!-- ---------------------------------------------------------------------------- END -------------------------------------------------------------------------------->
