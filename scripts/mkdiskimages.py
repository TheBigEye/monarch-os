#!/usr/bin/env python3
"""Create FAT32 and EXT2 raw disk images for Monarch QEMU tests.

The script intentionally uses only Python's standard library.  This keeps the
storage test images reproducible on Windows hosts that do not have dosfstools,
mtools, mke2fs, debugfs, or loop-mount support installed.
"""

from __future__ import annotations

import argparse
from pathlib import Path

SECTOR = 512
PART_START = 2048


def put16(buf: bytearray, off: int, value: int) -> None:
    buf[off:off + 2] = value.to_bytes(2, "little")


def put32(buf: bytearray, off: int, value: int) -> None:
    buf[off:off + 4] = value.to_bytes(4, "little")


def add_mbr(disk: bytearray, part_type: int, start_lba: int, sectors: int) -> None:
    off = 446
    disk[off + 0] = 0x00
    disk[off + 4] = part_type & 0xFF
    put32(disk, off + 8, start_lba)
    put32(disk, off + 12, sectors)
    disk[510] = 0x55
    disk[511] = 0xAA


def sfn_checksum(sfn: bytes) -> int:
    value = 0
    for ch in sfn:
        value = (((value & 1) << 7) + (value >> 1) + ch) & 0xFF
    return value


def fat_lfn_entry(sequence: int, text: str, checksum: int, last: bool) -> bytes:
    entry = bytearray([0xFF] * 32)
    entry[0] = sequence | (0x40 if last else 0)
    entry[11] = 0x0F
    entry[12] = 0
    entry[13] = checksum
    entry[26] = 0
    entry[27] = 0

    codepoints = [ord(ch) for ch in text]
    if len(codepoints) < 13:
        codepoints.append(0)
    while len(codepoints) < 13:
        codepoints.append(0xFFFF)

    offsets = [1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30]
    for off, ch in zip(offsets, codepoints[:13]):
        put16(entry, off, ch)
    return bytes(entry)


def fat_sfn_entry(sfn: bytes, attr: int, cluster: int, size: int) -> bytes:
    entry = bytearray(32)
    entry[0:11] = sfn
    entry[11] = attr
    put16(entry, 20, (cluster >> 16) & 0xFFFF)
    put16(entry, 26, cluster & 0xFFFF)
    put32(entry, 28, size)
    return bytes(entry)


def fat_dir_entry(name: str, sfn: bytes, cluster: int, data: bytes, long_name: bool = False) -> list[bytes]:
    entries: list[bytes] = []
    if long_name:
        checksum = sfn_checksum(sfn)
        chunks = [name[i:i + 13] for i in range(0, len(name), 13)]
        for idx in range(len(chunks), 0, -1):
            entries.append(fat_lfn_entry(idx, chunks[idx - 1], checksum, idx == len(chunks)))
    entries.append(fat_sfn_entry(sfn, 0x20, cluster, len(data)))
    return entries


def make_fat32(path: Path) -> None:
    partition_sectors = 150_000
    disk = bytearray((PART_START + partition_sectors) * SECTOR)
    add_mbr(disk, 0x0C, PART_START, partition_sectors)

    reserved = 32
    fat_count = 2
    sectors_per_cluster = 1
    sectors_per_fat = 1
    while True:
        data_sectors = partition_sectors - reserved - fat_count * sectors_per_fat
        clusters = data_sectors // sectors_per_cluster
        needed = ((clusters + 2) * 4 + SECTOR - 1) // SECTOR
        if needed == sectors_per_fat:
            break
        sectors_per_fat = needed

    part = PART_START * SECTOR
    boot = bytearray(SECTOR)
    boot[0:3] = bytes([0xEB, 0x58, 0x90])
    boot[3:11] = b"MSWIN4.1"
    put16(boot, 11, SECTOR)
    boot[13] = sectors_per_cluster
    put16(boot, 14, reserved)
    boot[16] = fat_count
    put16(boot, 17, 0)
    put16(boot, 19, 0)
    boot[21] = 0xF8
    put16(boot, 22, 0)
    put16(boot, 24, 63)
    put16(boot, 26, 255)
    put32(boot, 28, PART_START)
    put32(boot, 32, partition_sectors)
    put32(boot, 36, sectors_per_fat)
    put16(boot, 40, 0)
    put16(boot, 42, 0)
    put32(boot, 44, 2)       # root cluster
    put16(boot, 48, 1)       # FSInfo sector
    put16(boot, 50, 6)       # backup boot sector
    boot[64] = 0x80
    boot[66] = 0x29
    put32(boot, 67, 0x12345678)
    boot[71:82] = b"MONARCH    "
    boot[82:90] = b"FAT32   "
    boot[510] = 0x55
    boot[511] = 0xAA
    disk[part:part + SECTOR] = boot
    disk[(PART_START + 6) * SECTOR:(PART_START + 7) * SECTOR] = boot

    files: list[tuple[str, bytes, bytes, bool]] = [
        ("README.TXT", b"README  TXT", b"hello from Monarch FAT32 disk image\n", False),
        ("this-is-a-very-long-fat-file-name.txt", b"THISIS~1TXT", b"hello from a FAT32 long filename\n", True),
        ("big.bin", b"BIG     BIN", (b"Monarch FAT32 big file data\n" * 4096), False),
    ]
    docs_file = ("nested.txt", b"NESTED  TXT", b"nested file from FAT32 docs directory\n", False)

    cluster_data: dict[int, bytes] = {}
    next_cluster = 3
    file_clusters: dict[str, int] = {}
    for name, _sfn, data, _long in files:
        file_clusters[name] = next_cluster
        cluster_data[next_cluster] = data
        next_cluster += (len(data) + SECTOR - 1) // SECTOR
    docs_cluster = next_cluster
    next_cluster += 1
    file_clusters["docs/nested.txt"] = next_cluster
    cluster_data[next_cluster] = docs_file[2]
    next_cluster += 1

    fat_entries: dict[int, int] = {0: 0x0FFFFFF8, 1: 0x0FFFFFFF, 2: 0x0FFFFFFF}
    for start_cluster, data in cluster_data.items():
        count = (len(data) + SECTOR - 1) // SECTOR
        for i in range(count):
            cluster = start_cluster + i
            fat_entries[cluster] = 0x0FFFFFFF if i + 1 == count else cluster + 1
    fat_entries[docs_cluster] = 0x0FFFFFFF

    free_clusters = 0
    next_free = 0xFFFFFFFF
    for cluster in range(2, clusters + 2):
        if cluster not in fat_entries:
            free_clusters += 1
            if next_free == 0xFFFFFFFF:
                next_free = cluster

    fsinfo = bytearray(SECTOR)
    put32(fsinfo, 0, 0x41615252)
    put32(fsinfo, 484, 0x61417272)
    put32(fsinfo, 488, free_clusters)
    put32(fsinfo, 492, next_free)
    put32(fsinfo, 508, 0xAA550000)
    disk[(PART_START + 1) * SECTOR:(PART_START + 2) * SECTOR] = fsinfo
    disk[(PART_START + 7) * SECTOR:(PART_START + 8) * SECTOR] = fsinfo

    for fat_index in range(fat_count):
        fat_start = (PART_START + reserved + fat_index * sectors_per_fat) * SECTOR
        fat = bytearray(sectors_per_fat * SECTOR)
        for cluster, value in fat_entries.items():
            put32(fat, cluster * 4, value)
        disk[fat_start:fat_start + len(fat)] = fat

    first_data_lba = PART_START + reserved + fat_count * sectors_per_fat

    root = bytearray(SECTOR)
    offset = 0
    for name, sfn, data, long_name in files:
        for entry in fat_dir_entry(name, sfn, file_clusters[name], data, long_name):
            root[offset:offset + 32] = entry
            offset += 32
    root[offset:offset + 32] = fat_sfn_entry(b"DOCS       ", 0x10, docs_cluster, 0)
    offset += 32
    root[offset] = 0
    disk[first_data_lba * SECTOR:(first_data_lba + 1) * SECTOR] = root

    docs = bytearray(SECTOR)
    docs[0:32] = fat_sfn_entry(b".          ", 0x10, docs_cluster, 0)
    docs[32:64] = fat_sfn_entry(b"..         ", 0x10, 2, 0)
    docs[64:96] = fat_sfn_entry(docs_file[1], 0x20, file_clusters["docs/nested.txt"], len(docs_file[2]))
    docs[96] = 0
    disk[(first_data_lba + (docs_cluster - 2)) * SECTOR:(first_data_lba + (docs_cluster - 1)) * SECTOR] = docs

    for cluster, data in cluster_data.items():
        lba = first_data_lba + (cluster - 2)
        disk[lba * SECTOR:lba * SECTOR + len(data)] = data

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(disk)
    print(f"mkdiskimages: wrote {path} ({len(disk)} bytes, FAT32)")


def ext2_dirent(ino: int, name: str, file_type: int, rec_len: int) -> bytes:
    entry = bytearray(rec_len)
    put32(entry, 0, ino)
    put16(entry, 4, rec_len)
    entry[6] = len(name)
    entry[7] = file_type
    entry[8:8 + len(name)] = name.encode("ascii")
    return bytes(entry)


def ext2_inode(mode: int, size: int, blocks512: int, block_ptrs: list[int], links: int) -> bytes:
    inode = bytearray(128)
    put16(inode, 0, mode)
    put32(inode, 4, size)
    put16(inode, 26, links)
    put32(inode, 28, blocks512)
    for i, block in enumerate(block_ptrs[:15]):
        put32(inode, 40 + i * 4, block)
    return bytes(inode)


def make_ext2(path: Path) -> None:
    partition_sectors = 16_000
    block_size = 1024
    partition_blocks = partition_sectors * SECTOR // block_size
    inode_count = 128
    disk = bytearray((PART_START + partition_sectors) * SECTOR)
    add_mbr(disk, 0x83, PART_START, partition_sectors)
    base = PART_START * SECTOR

    superblock = bytearray(1024)
    put32(superblock, 0, inode_count)
    put32(superblock, 4, partition_blocks)
    put32(superblock, 8, 0)
    put32(superblock, 12, partition_blocks - 900)
    put32(superblock, 16, inode_count - 20)
    put32(superblock, 20, 1)      # first data block for 1 KiB block size
    put32(superblock, 24, 0)      # log block size
    put32(superblock, 28, 0)
    put32(superblock, 32, 8192)
    put32(superblock, 36, 8192)
    put32(superblock, 40, inode_count)
    put16(superblock, 52, 0)
    put16(superblock, 54, 0xFFFF)
    put16(superblock, 56, 0xEF53)
    put16(superblock, 58, 1)
    put16(superblock, 60, 1)
    put16(superblock, 62, 0)
    put32(superblock, 76, 1)      # dynamic revision
    put32(superblock, 84, 11)
    put16(superblock, 88, 128)
    put16(superblock, 90, 0)
    put32(superblock, 92, 0)
    put32(superblock, 96, 0x2)    # file_type in dirents
    put32(superblock, 100, 0)
    disk[base + 1024:base + 2048] = superblock

    group_desc = bytearray(1024)
    put32(group_desc, 0, 3)       # block bitmap
    put32(group_desc, 4, 4)       # inode bitmap
    put32(group_desc, 8, 5)       # inode table
    put16(group_desc, 12, partition_blocks - 900)
    put16(group_desc, 14, inode_count - 20)
    put16(group_desc, 16, 2)
    disk[base + 2 * block_size:base + 3 * block_size] = group_desc

    next_block = 21
    root_block = next_block; next_block += 1
    docs_block = next_block; next_block += 1
    hello_block = next_block; next_block += 1
    nested_block = next_block; next_block += 1

    big_data = b"Monarch EXT2 big file data\n" * 30000
    big_blocks = (len(big_data) + block_size - 1) // block_size
    big_start = next_block
    next_block += big_blocks
    single_indirect = next_block
    next_block += 1
    double_indirect = next_block
    next_block += 1
    double_second = next_block
    next_block += 1

    root = bytearray(block_size)
    entries = [
        ext2_dirent(2, ".", 2, 12),
        ext2_dirent(2, "..", 2, 12),
        ext2_dirent(12, "docs", 2, 12),
        ext2_dirent(13, "hello.txt", 1, 20),
    ]
    used = sum(len(e) for e in entries)
    entries.append(ext2_dirent(15, "big.bin", 1, block_size - used))
    off = 0
    for entry in entries:
        root[off:off + len(entry)] = entry
        off += len(entry)
    disk[base + root_block * block_size:base + (root_block + 1) * block_size] = root

    docs = bytearray(block_size)
    entries = [
        ext2_dirent(12, ".", 2, 12),
        ext2_dirent(2, "..", 2, 12),
    ]
    used = sum(len(e) for e in entries)
    entries.append(ext2_dirent(14, "nested.txt", 1, block_size - used))
    off = 0
    for entry in entries:
        docs[off:off + len(entry)] = entry
        off += len(entry)
    disk[base + docs_block * block_size:base + (docs_block + 1) * block_size] = docs

    hello = b"hello from Monarch EXT2 disk image\n"
    nested = b"nested file from EXT2 docs directory\n"
    disk[base + hello_block * block_size:base + hello_block * block_size + len(hello)] = hello
    disk[base + nested_block * block_size:base + nested_block * block_size + len(nested)] = nested
    disk[base + big_start * block_size:base + big_start * block_size + len(big_data)] = big_data

    big_ptrs = [0] * 15
    for i in range(min(12, big_blocks)):
        big_ptrs[i] = big_start + i
    remaining = max(0, big_blocks - 12)
    single_count = min(remaining, block_size // 4)
    if single_count:
        big_ptrs[12] = single_indirect
        table = bytearray(block_size)
        for i in range(single_count):
            put32(table, i * 4, big_start + 12 + i)
        disk[base + single_indirect * block_size:base + (single_indirect + 1) * block_size] = table
    remaining -= single_count
    if remaining:
        big_ptrs[13] = double_indirect
        first = bytearray(block_size)
        second = bytearray(block_size)
        put32(first, 0, double_second)
        for i in range(remaining):
            put32(second, i * 4, big_start + 12 + single_count + i)
        disk[base + double_indirect * block_size:base + (double_indirect + 1) * block_size] = first
        disk[base + double_second * block_size:base + (double_second + 1) * block_size] = second

    inode_table = base + 5 * block_size
    inode_map = {
        2: ext2_inode(0x41ED, block_size, 2, [root_block], 2),
        12: ext2_inode(0x41ED, block_size, 2, [docs_block], 2),
        13: ext2_inode(0x81A4, len(hello), 2, [hello_block], 1),
        14: ext2_inode(0x81A4, len(nested), 2, [nested_block], 1),
        15: ext2_inode(0x81A4, len(big_data), ((len(big_data) + 511) // 512), big_ptrs, 1),
    }
    for ino, inode in inode_map.items():
        index = ino - 1
        disk[inode_table + index * 128:inode_table + (index + 1) * 128] = inode

    # Keep the allocation metadata consistent with the handcrafted contents.
    # The first 21 blocks contain the superblock, descriptors, bitmaps and inode
    # table; the remaining used blocks are the directory, file and indirect data.
    used_blocks = set(range(21))
    used_blocks.update({root_block, docs_block, hello_block, nested_block})
    used_blocks.update(range(big_start, big_start + big_blocks))
    used_blocks.update({single_indirect, double_indirect, double_second})
    block_bitmap = bytearray(block_size)
    for block in used_blocks:
        block_bitmap[block // 8] |= 1 << (block % 8)
    inode_bitmap = bytearray(block_size)
    for ino in {1, 2, 12, 13, 14, 15}:
        inode_bitmap[(ino - 1) // 8] |= 1 << ((ino - 1) % 8)
    disk[base + 3 * block_size:base + 4 * block_size] = block_bitmap
    disk[base + 4 * block_size:base + 5 * block_size] = inode_bitmap
    free_blocks = partition_blocks - len(used_blocks)
    free_inodes = inode_count - len({1, 2, 12, 13, 14, 15})
    put32(disk, base + 1024 + 12, free_blocks)
    put32(disk, base + 1024 + 16, free_inodes)
    put16(disk, base + 2 * block_size + 12, free_blocks)
    put16(disk, base + 2 * block_size + 14, free_inodes)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(disk)
    print(f"mkdiskimages: wrote {path} ({len(disk)} bytes, EXT2)")


def fat12_set(fat: bytearray, cluster: int, value: int) -> None:
    offset = cluster + cluster // 2
    value &= 0x0FFF
    if cluster & 1:
        fat[offset] = (fat[offset] & 0x0F) | ((value << 4) & 0xF0)
        fat[offset + 1] = (value >> 4) & 0xFF
    else:
        fat[offset] = value & 0xFF
        fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F)


def make_floppy(path: Path) -> None:
    image = bytearray(1_474_560)
    image[0:3] = bytes([0xEB, 0x3C, 0x90])
    image[3:11] = b"MONARCH "
    put16(image, 11, 512)
    image[13] = 1
    put16(image, 14, 1)
    image[16] = 2
    put16(image, 17, 224)
    put16(image, 19, 2880)
    image[21] = 0xF0
    put16(image, 22, 9)
    put16(image, 24, 18)
    put16(image, 26, 2)
    image[510] = 0x55
    image[511] = 0xAA

    # Build a minimal but structurally valid FAT12 volume.  The FDC tests use
    # raw controller commands, while this layout also makes the image useful
    # to BIOS tools and later FAT12 mounting work.
    message = b"Monarch floppy controller test image.\r\n"
    fat_start = 1 * SECTOR
    root_start = (1 + 2 * 9) * SECTOR
    data_start = root_start + 14 * SECTOR
    fat = bytearray(9 * SECTOR)
    fat[0:3] = bytes([0xF0, 0xFF, 0xFF])
    fat12_set(fat, 2, 0xFFF)
    fat12_set(fat, 3, 0xFFF)
    fat12_set(fat, 4, 0xFFF)
    fat12_set(fat, 5, 6)
    fat12_set(fat, 6, 0xFFF)
    image[fat_start:fat_start + len(fat)] = fat
    image[fat_start + len(fat):fat_start + 2 * len(fat)] = fat
    root = bytearray(14 * SECTOR)
    root[0:11] = b"README  TXT"
    root[11] = 0x20
    put16(root, 26, 2)
    put32(root, 28, len(message))
    root[32:43] = b"DOCS       "
    root[43] = 0x10
    put16(root, 32 + 26, 3)
    root[64:75] = b"BIG     TXT"
    root[75] = 0x20
    put16(root, 64 + 26, 5)
    big = b"FAT12 multi-cluster test data.\\r\\n" * 50
    put32(root, 64 + 28, len(big))
    image[root_start:root_start + len(root)] = root
    image[data_start:data_start + len(message)] = message
    image[data_start + 3 * SECTOR:data_start + 3 * SECTOR + len(big[:SECTOR])] = big[:SECTOR]
    image[data_start + 4 * SECTOR:data_start + 4 * SECTOR + len(big[SECTOR:])] = big[SECTOR:]
    nested = b"nested FAT12 file from Monarch floppy.\r\n"
    directory = bytearray(SECTOR)
    directory[0:11] = b".          "
    directory[11] = 0x10
    put16(directory, 26, 3)
    directory[32:43] = b"..         "
    directory[43] = 0x10
    put16(directory, 32 + 26, 0)
    directory[64:75] = b"NESTED  TXT"
    directory[75] = 0x20
    put16(directory, 64 + 26, 4)
    put32(directory, 64 + 28, len(nested))
    image[data_start + SECTOR:data_start + 2 * SECTOR] = directory
    image[data_start + 3 * SECTOR:data_start + 3 * SECTOR + len(nested)] = nested
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(image)
    print(f"mkdiskimages: wrote {path} (1440 KiB floppy)")


def main() -> None:
    parser = argparse.ArgumentParser(description="Create Monarch FAT32/EXT2/floppy QEMU disk images")
    parser.add_argument("--fat32", required=True, type=Path)
    parser.add_argument("--ext2", required=True, type=Path)
    parser.add_argument("--floppy", required=True, type=Path)
    args = parser.parse_args()
    make_fat32(args.fat32)
    make_ext2(args.ext2)
    make_floppy(args.floppy)


if __name__ == "__main__":
    main()
