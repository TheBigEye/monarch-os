#!/usr/bin/env sh
# Install the host tools needed to build and test Monarch in a Debian/Ubuntu-like
# environment.  This is intentionally small: it installs the tools that are
# commonly available from distro packages.  A full i686-elf GCC cross compiler is
# still recommended for real OSDev builds, but this script is enough for local
# smoke tests that use `make CC=gcc LD=ld`.
set -eu

if [ "${1:-}" = "--update" ]; then
    sudo apt-get update
fi

sudo apt-get install -y \
    nasm \
    xorriso \
    qemu-system-x86 \
    curl \
    tar \
    python3

printf '%s\n' "bootstrap-tools: installed nasm, xorriso, qemu-system-x86, curl, tar, python3"
printf '%s\n' "bootstrap-tools: for smoke tests use: make CC=gcc LD=ld"
printf '%s\n' "bootstrap-tools: for normal builds use your i686-elf-gcc/i686-elf-ld toolchain"
