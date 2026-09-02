#!/usr/bin/env python3
"""Generate tiny audio fixtures for Monarch's early audio tests.

The files are intentionally small and use only uncompressed PCM so they can be
parsed by the freestanding userspace SFX library without third-party codecs.
"""

from __future__ import annotations

import argparse
from pathlib import Path

DURATION_MS = 120
FREQUENCY = 660


def le16(value: int) -> bytes:
    return value.to_bytes(2, "little", signed=False)


def le32(value: int) -> bytes:
    return value.to_bytes(4, "little", signed=False)


def be32(value: int) -> bytes:
    return value.to_bytes(4, "big", signed=False)


def pcm_s16(rate: int, endian: str) -> bytes:
    out = bytearray()
    frames = rate * DURATION_MS // 1000
    period = max(2, rate // FREQUENCY)
    for frame in range(frames):
        sample = 9000 if (frame % period) < (period // 2) else -9000
        packed = int(sample).to_bytes(2, endian, signed=True)
        out += packed
        out += packed
    return bytes(out)


def write_wav(path: Path, pcm: bytes, rate: int) -> None:
    block_align = 4
    byte_rate = rate * block_align
    fmt = (
        le16(1) +          # PCM
        le16(2) +          # stereo
        le32(rate) +
        le32(byte_rate) +
        le16(block_align) +
        le16(16)
    )
    riff_size = 4 + 8 + len(fmt) + 8 + len(pcm)
    path.write_bytes(b"RIFF" + le32(riff_size) + b"WAVE" + b"fmt " + le32(len(fmt)) + fmt + b"data" + le32(len(pcm)) + pcm)


def write_au(path: Path, pcm_be: bytes, rate: int) -> None:
    header_size = 24
    encoding = 3  # 16-bit linear PCM, big endian
    path.write_bytes(
        b".snd" +
        be32(header_size) +
        be32(len(pcm_be)) +
        be32(encoding) +
        be32(rate) +
        be32(2) +
        pcm_be
    )


def write_msfx(path: Path, pcm: bytes, rate: int) -> None:
    # Monarch Simple SFX: magic, rate, channels, bits, data size, then PCM.
    path.write_bytes(b"MSFX" + le32(rate) + le32(2) + le32(16) + le32(len(pcm)) + pcm)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--outdir", required=True, type=Path)
    args = parser.parse_args()

    args.outdir.mkdir(parents=True, exist_ok=True)

    write_wav(args.outdir / "tone.wav", pcm_s16(48_000, "little"), 48_000)
    write_au(args.outdir / "tone.au", pcm_s16(48_000, "big"), 48_000)
    write_msfx(args.outdir / "tone.msfx", pcm_s16(48_000, "little"), 48_000)

    write_wav(args.outdir / "tone44100.wav", pcm_s16(44_100, "little"), 44_100)
    write_au(args.outdir / "tone22050.au", pcm_s16(22_050, "big"), 22_050)
    write_msfx(args.outdir / "tone11025.msfx", pcm_s16(11_025, "little"), 11_025)

    print("mkaudio: wrote wav/au/msfx fixtures at 48000, 44100, 22050 and 11025 Hz")


if __name__ == "__main__":
    main()
