#!/usr/bin/env python3
"""Build a Linux-compatible uncompressed initramfs image (CPIO newc).

No third-party dependencies are required. The output is a plain `newc` CPIO
archive compatible with Linux initramfs readers and simple OSDev kernels.
"""

from __future__ import annotations

import argparse
import os
import stat
from pathlib import Path

CPIO_MAGIC = "070701"
TRAILER = "TRAILER!!!"


def align4(value: int) -> int:
    return (value + 3) & ~3


def pad4(blob: bytearray) -> None:
    while len(blob) % 4:
        blob.append(0)


def field(value: int) -> str:
    return f"{value & 0xFFFFFFFF:08x}"


def header(
    ino: int,
    mode: int,
    uid: int,
    gid: int,
    nlink: int,
    mtime: int,
    filesize: int,
    namesize: int,
) -> bytes:
    # newc header fields:
    # magic, ino, mode, uid, gid, nlink, mtime, filesize,
    # devmajor, devminor, rdevmajor, rdevminor, namesize, check
    text = (
        CPIO_MAGIC
        + field(ino)
        + field(mode)
        + field(uid)
        + field(gid)
        + field(nlink)
        + field(mtime)
        + field(filesize)
        + field(0)
        + field(0)
        + field(0)
        + field(0)
        + field(namesize)
        + field(0)
    )
    assert len(text) == 110
    return text.encode("ascii")


def add_entry(out: bytearray, name: str, data: bytes, mode: int, ino: int, mtime: int) -> None:
    encoded_name = name.encode("utf-8") + b"\0"
    out += header(
        ino=ino,
        mode=mode,
        uid=0,
        gid=0,
        nlink=1,
        mtime=mtime,
        filesize=len(data),
        namesize=len(encoded_name),
    )
    out += encoded_name
    pad4(out)
    out += data
    pad4(out)


def collect(root: Path) -> list[Path]:
    paths: list[Path] = []
    for current, dirs, files in os.walk(root):
        dirs.sort()
        files.sort()
        current_path = Path(current)
        if current_path != root:
            paths.append(current_path)
        for name in files:
            paths.append(current_path / name)
    return paths


def relname(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def build(root: Path) -> bytes:
    root = root.resolve()
    if not root.is_dir():
        raise SystemExit(f"initrd root is not a directory: {root}")

    out = bytearray()
    ino = 1

    for path in collect(root):
        st = path.stat()
        name = relname(root, path)
        mtime = int(st.st_mtime)

        if path.is_dir():
            mode = stat.S_IFDIR | 0o755
            data = b""
        elif path.is_file():
            executable = bool(st.st_mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH))
            mode = stat.S_IFREG | (0o755 if executable else 0o644)
            data = path.read_bytes()
        else:
            # Keep the image deterministic and simple. Symlinks/devnodes can be
            # added later when the kernel knows how to consume them.
            continue

        add_entry(out, name, data, mode, ino, mtime)
        ino += 1

    add_entry(out, TRAILER, b"", 0, ino, 0)
    return bytes(out)


def main() -> None:
    parser = argparse.ArgumentParser(description="Create a CPIO newc initrd.img")
    parser.add_argument("root", type=Path, help="directory to pack")
    parser.add_argument("-o", "--output", type=Path, required=True, help="output initrd image")
    args = parser.parse_args()

    image = build(args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)
    print(f"mkinitrd: wrote {args.output} ({len(image)} bytes)")


if __name__ == "__main__":
    main()
