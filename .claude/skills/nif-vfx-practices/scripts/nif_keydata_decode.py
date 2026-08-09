import struct
import sys
from nif_header_walk import parse_header, block_offsets

# Decodifica un NiFloatData (KeyGroup<float>) completo -- Num Keys,
# Interpolation (LINEAR_KEY=1/QUADRATIC_KEY=2), y cada clave con
# Time/Value/Forward/Backward (los dos ultimos solo existen si
# QUADRATIC_KEY). Validado 2026-08-09 -- ver SKILL.md, "Trampa de las
# tangentes al alargar una animacion". Uso: nif_keydata_decode.py fichero.nif
# bloque1 bloque2 ...

INTERP_TYPES = {1: 'LINEAR_KEY', 2: 'QUADRATIC_KEY', 3: 'TBC_KEY', 4: 'XYZ_ROTATION_KEY', 5: 'CONST_KEY'}


def decode_floatdata(buf, off):
    num_keys, = struct.unpack_from('<I', buf, off)
    cursor = off + 4
    interp_type = None
    keys = []
    if num_keys != 0:
        interp_type, = struct.unpack_from('<I', buf, cursor)
        cursor += 4
        for _ in range(num_keys):
            if interp_type == 2:
                kt, kv, fwd, bwd = struct.unpack_from('<4f', buf, cursor)
                keys.append(dict(time=kt, value=kv, forward=fwd, backward=bwd))
                cursor += 16
            else:
                kt, kv = struct.unpack_from('<2f', buf, cursor)
                keys.append(dict(time=kt, value=kv))
                cursor += 8
    return dict(num_keys=num_keys, interp_type=INTERP_TYPES.get(interp_type, interp_type), keys=keys, end_offset=cursor)


def inspect(path, block_indices):
    buf = open(path, 'rb').read()
    h = parse_header(buf)
    offsets = block_offsets(h)
    types = h['block_types']
    type_idx = h['block_type_index']
    print('=====', path, '=====')
    for i in block_indices:
        tname = types[type_idx[i]]
        if tname != 'NiFloatData':
            print(f'[{i}] no es NiFloatData (es {tname}) -- saltado')
            continue
        off = offsets[i]
        size = h['block_size'][i]
        d = decode_floatdata(buf, off)
        computed_size = d['end_offset'] - off
        flag = '' if computed_size == size else f'  <-- tamano calculado {computed_size} != real {size}, revisar'
        print(f"[{i}] NiFloatData  NumKeys={d['num_keys']}  Interpolation={d['interp_type']}{flag}")
        for k, key in enumerate(d['keys']):
            print(f"     [{k}] {key}")


if __name__ == '__main__':
    path = sys.argv[1]
    indices = [int(x) for x in sys.argv[2:]]
    inspect(path, indices)
