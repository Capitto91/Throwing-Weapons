import struct
import sys
from nif_header_walk import parse_header, block_offsets

# Offsets from block start (NiPSysModifier(13) + NiPSysEmitter(56) + NiPSysVolumeEmitter(4) = 73)
INITIAL_RADIUS_OFF = 53
LIFE_SPAN_OFF = 61
EMITTER_OWN_OFF = 73

def inspect(path):
    buf = open(path, 'rb').read()
    h = parse_header(buf)
    offsets = block_offsets(h)
    print('=====', path, '=====')
    for i in range(h['num_blocks']):
        tname = h['block_types'][h['block_type_index'][i]]
        if tname not in ('NiPSysCylinderEmitter', 'NiPSysBoxEmitter', 'NiPSysMeshEmitter'):
            continue
        off = offsets[i]
        size = h['block_size'][i]
        initial_radius, = struct.unpack_from('<f', buf, off + INITIAL_RADIUS_OFF)
        life_span, = struct.unpack_from('<f', buf, off + LIFE_SPAN_OFF)
        print(f"  [{i}] {tname}  size={size}")
        print(f"      InitialRadius(particula)={initial_radius:.3f}  LifeSpan={life_span:.3f}s")
        if tname == 'NiPSysCylinderEmitter' and size == 81:
            radius, height = struct.unpack_from('<2f', buf, off + EMITTER_OWN_OFF)
            print(f"      Cylindro emisor: Radius={radius:.3f}  Height={height:.3f}")
        elif tname == 'NiPSysBoxEmitter' and size == 85:
            w, hh, d = struct.unpack_from('<3f', buf, off + EMITTER_OWN_OFF)
            print(f"      Caja emisora: Width={w:.3f}  Height={hh:.3f}  Depth={d:.3f}")
        else:
            print(f"      (tamaño de bloque {size} no coincide con el esperado para decodificar la forma propia con seguridad)")
    print()

if __name__ == '__main__':
    for p in sys.argv[1:]:
        inspect(p)
