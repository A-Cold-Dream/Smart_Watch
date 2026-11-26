"""Build an OTA payload from the same flash load segments a debugger programs."""
from pathlib import Path
import struct
import sys
import zlib

APP_BASE = 0x08008000
RECORD = 0x0807FFF0


def package(elf_path, bin_path):
    elf = bytearray(elf_path.read_bytes())
    if elf[:6] != b"\x7fELF\x01\x01":
        raise ValueError("Expected a little-endian ELF32")
    phoff, = struct.unpack_from("<I", elf, 28)
    stride, count = struct.unpack_from("<HH", elf, 42)
    payload = bytearray(b"\xff" * (RECORD - APP_BASE))
    size = 0
    record_offset = None
    for index in range(count):
        kind, offset, _, address, filesz, _, _, _ = struct.unpack_from(
            "<8I", elf, phoff + index * stride)
        if kind != 1 or not filesz:  # PT_LOAD, excluding zero-initialized data
            continue
        if offset + filesz > len(elf):
            raise ValueError("Truncated load segment")
        if address == RECORD and filesz == 16:
            record_offset = offset
            continue
        if address < APP_BASE or address + filesz > RECORD:
            raise ValueError(f"Unexpected load segment at 0x{address:08X}")
        start = address - APP_BASE
        payload[start:start + filesz] = elf[offset:offset + filesz]
        size = max(size, start + filesz)

    if record_offset is None or payload[0x198:0x1A0] != struct.pack("<4sI", b"SWA1", APP_BASE):
        raise ValueError("Missing Smart_Watch OTA target or commit record")
    del payload[size:]
    struct.pack_into("<IIII", elf, record_offset, 0x31524459, size,
                     zlib.crc32(payload), size ^ 0xFFFFFFFF)
    # Preserve all ELF program and debug headers, only replace the reserved 16 bytes.
    stamped = elf_path.with_suffix(".stamped")
    stamped.write_bytes(elf)
    stamped.replace(elf_path)
    bin_path.write_bytes(payload)


if __name__ == "__main__":
    package(Path(sys.argv[1]), Path(sys.argv[2]))
