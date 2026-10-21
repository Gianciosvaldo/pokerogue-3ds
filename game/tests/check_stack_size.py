#!/usr/bin/env python3
"""Check the actual ARM startup stack symbol, including stripped builder ELFs."""
import argparse
import struct
from pathlib import Path

MINIMUM = 512 * 1024

def stack_size(path):
    data = path.read_bytes()
    if data[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', data, 18)[0] != 40:
        raise ValueError('expected a little-endian ELF32 ARM executable')
    offset = struct.unpack_from('<I', data, 32)[0]
    width, count = struct.unpack_from('<HH', data, 46)
    sections = [struct.unpack_from('<10I', data, offset + i * width) for i in range(count)]
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB, retained by strip --strip-debug
            continue
        strings = sections[section[6]]
        names = data[strings[4]:strings[4] + strings[5]]
        for pos in range(section[4], section[4] + section[5], section[9]):
            name, address, size, info, _, index = struct.unpack_from('<IIIBBH', data, pos)
            if names[name:].split(b'\0', 1)[0] != b'__stacksize__':
                continue
            if info >> 4 != 1 or (info & 15) != 1 or size != 4 or index >= len(sections):
                raise ValueError('__stacksize__ must be the app\'s strong 32-bit startup override')
            storage = sections[index]
            if not storage[3] <= address <= storage[3] + storage[5] - 4:
                raise ValueError('startup stack symbol is outside its data section')
            return struct.unpack_from('<I', data, storage[4] + address - storage[3])[0]
    raise ValueError('missing __stacksize__ symbol')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    parser.add_argument('--usage-dir', type=Path)
    args = parser.parse_args()
    try:
        size = stack_size(args.elf)
        if size < MINIMUM:
            raise ValueError(f'ARM startup stack {size} bytes is below {MINIMUM}')
        print(f'ARM startup stack: strong override, {size} bytes')
        if args.usage_dir:
            frames = []
            for file in args.usage_dir.glob('*.su'):
                for line in file.read_text().splitlines():
                    fields = line.split('\t')
                    if len(fields) != 3 or fields[2] != 'static':
                        raise ValueError(f'unbounded/unrecognized compiler stack record: {line}')
                    frames.append((int(fields[1]), fields[0]))
            if not frames:
                raise ValueError('missing ARM compiler stack-usage reports')
            amount, function = max(frames)
            if amount >= size:
                raise ValueError(f'single frame {function} requires {amount} bytes')
            print(f'ARM frames checked: {len(frames)}; largest {amount} bytes ({function})')
    except (ValueError, IndexError, struct.error, OSError) as error:
        parser.exit(1, f'3DS stack check failed: {error}\n')

if __name__ == '__main__':
    main()
