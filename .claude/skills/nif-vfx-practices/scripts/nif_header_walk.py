import struct
import sys

def read_sized_string(buf, off):
    (length,) = struct.unpack_from('<I', buf, off)
    off += 4
    s = buf[off:off+length]
    off += length
    return s.decode('ascii', errors='replace'), off

def read_export_string(buf, off):
    length = buf[off]
    off += 1
    s = buf[off:off+length]
    off += length
    return s.decode('ascii', errors='replace'), off

def parse_header(buf):
    off = 0
    nl = buf.index(b'\n', off)
    header_string = buf[off:nl].decode('ascii', errors='replace')
    off = nl + 1

    (version,) = struct.unpack_from('<I', buf, off); off += 4
    endian_type = buf[off]; off += 1
    (user_version,) = struct.unpack_from('<I', buf, off); off += 4
    (num_blocks,) = struct.unpack_from('<I', buf, off); off += 4

    # BSSTREAMHEADER condition: version == 20.2.0.7 (0x14020007) or other listed cases, AND user_version >= 3
    ver_20207 = 0x14020007
    bsstreamheader = (version == ver_20207 or version == 0x14000005 or
                       (0x0A010000 <= version <= 0x14000004 and user_version <= 11)) and user_version >= 3

    bs_version = None
    author = process_script = export_script = max_filepath = None
    if bsstreamheader:
        (bs_version,) = struct.unpack_from('<I', buf, off); off += 4
        author, off = read_export_string(buf, off)
        if bs_version > 130:
            (_unknown_int,) = struct.unpack_from('<I', buf, off); off += 4
        if bs_version < 131:
            process_script, off = read_export_string(buf, off)
        export_script, off = read_export_string(buf, off)
        if bs_version >= 103:
            max_filepath, off = read_export_string(buf, off)

    # Metadata (ByteArray) since 30.0.0.0 -- not applicable for 20.2.0.7, skip

    (num_block_types,) = struct.unpack_from('<H', buf, off); off += 2
    block_types = []
    for _ in range(num_block_types):
        name, off = read_sized_string(buf, off)
        block_types.append(name)

    block_type_index = []
    for _ in range(num_blocks):
        (idx,) = struct.unpack_from('<h', buf, off); off += 2  # signed 16-bit per BlockTypeIndex doc
        block_type_index.append(idx)

    block_size = []
    for _ in range(num_blocks):
        (sz,) = struct.unpack_from('<I', buf, off); off += 4
        block_size.append(sz)

    (num_strings,) = struct.unpack_from('<I', buf, off); off += 4
    (max_string_length,) = struct.unpack_from('<I', buf, off); off += 4
    strings = []
    for _ in range(num_strings):
        s, off = read_sized_string(buf, off)
        strings.append(s)

    (num_groups,) = struct.unpack_from('<I', buf, off); off += 4
    off += 4 * num_groups  # Groups array, uint each

    return {
        'header_string': header_string,
        'version': version,
        'user_version': user_version,
        'num_blocks': num_blocks,
        'bs_version': bs_version,
        'num_block_types': num_block_types,
        'block_types': block_types,
        'block_type_index': block_type_index,
        'block_size': block_size,
        'num_strings': num_strings,
        'strings': strings,
        'data_start': off,
    }

def block_offsets(header):
    offsets = []
    cur = header['data_start']
    for i in range(header['num_blocks']):
        offsets.append(cur)
        cur += header['block_size'][i]
    return offsets

if __name__ == '__main__':
    path = sys.argv[1]
    buf = open(path, 'rb').read()
    h = parse_header(buf)
    print('header_string:', h['header_string'])
    print('version:', hex(h['version']), 'user_version:', h['user_version'], 'bs_version:', h['bs_version'])
    print('num_blocks:', h['num_blocks'], 'num_block_types:', h['num_block_types'])
    print('data_start offset:', h['data_start'], 'file size:', len(buf))
    offsets = block_offsets(h)
    end_of_last = offsets[-1] + h['block_size'][-1]
    print('end of last block:', end_of_last, '(should be <= file size, footer follows)')
    print()
    print('--- primeros 15 bloques: (indice, tipo, offset, tamano) ---')
    for i in range(min(15, h['num_blocks'])):
        t = h['block_types'][h['block_type_index'][i]]
        print(f"  [{i}] {t}  offset={offsets[i]}  size={h['block_size'][i]}")
