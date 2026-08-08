import struct
import sys
from nif_header_walk import parse_header, block_offsets

def read_root_transform(path, block_index=0):
    buf = open(path, 'rb').read()
    h = parse_header(buf)
    offsets = block_offsets(h)
    off = offsets[block_index]
    tname = h['block_types'][h['block_type_index'][block_index]]
    cur = off
    name_idx, = struct.unpack_from('<i', buf, cur); cur += 4
    num_extra, = struct.unpack_from('<I', buf, cur); cur += 4
    cur += 4 * num_extra  # Extra Data List refs
    controller, = struct.unpack_from('<i', buf, cur); cur += 4
    flags, = struct.unpack_from('<I', buf, cur); cur += 4
    tx, ty, tz = struct.unpack_from('<3f', buf, cur); cur += 12
    rot = struct.unpack_from('<9f', buf, cur); cur += 36
    scale, = struct.unpack_from('<f', buf, cur); cur += 4
    collision, = struct.unpack_from('<i', buf, cur); cur += 4
    name = h['strings'][name_idx] if 0 <= name_idx < len(h['strings']) else f'(idx {name_idx})'
    print(f"[{block_index}] {tname}  name={name!r}")
    print(f"    Translation = ({tx:.4f}, {ty:.4f}, {tz:.4f})")
    print(f"    Scale = {scale:.4f}  Flags=0x{flags:X}  NumExtraData={num_extra}  Controller={controller}")

if __name__ == '__main__':
    for p in sys.argv[1:]:
        print('=====', p, '=====')
        read_root_transform(p)
        print()
