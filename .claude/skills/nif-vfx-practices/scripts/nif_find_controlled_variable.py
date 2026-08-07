import struct
import sys
from nif_header_walk import parse_header, block_offsets

EFFECT_SHADER_VAR = {
    0: 'EmissiveMultiple', 1: 'Falloff Start Angle', 2: 'Falloff Stop Angle',
    3: 'Falloff Start Opacity', 4: 'Falloff Stop Opacity', 5: 'Alpha Transparency',
    6: 'U Offset', 7: 'U Scale', 8: 'V Offset', 9: 'V Scale',
    11: 'Unknown 11', 12: 'Unknown 12', 13: 'Unknown 13', 14: 'Unknown 14',
}
LIGHTING_SHADER_VAR = {
    0: 'Refraction Strength', 3: 'Unknown 3', 4: 'Unknown 4',
    8: 'Environment Map Scale', 9: 'Glossiness', 10: 'Specular Strength',
    11: 'Emissive Multiple', 12: 'Alpha', 13: 'Unknown 13', 14: 'Unknown 14',
    20: 'U Offset', 21: 'U Scale', 22: 'V Offset', 23: 'V Scale',
}

TARGETS = {
    'BSEffectShaderPropertyFloatController': EFFECT_SHADER_VAR,
    'BSLightingShaderPropertyFloatController': LIGHTING_SHADER_VAR,
}

def inspect(path):
    buf = open(path, 'rb').read()
    h = parse_header(buf)
    offsets = block_offsets(h)
    print('=====', path, '=====')
    found_any = False
    for i in range(h['num_blocks']):
        tname = h['block_types'][h['block_type_index'][i]]
        if tname not in TARGETS:
            continue
        found_any = True
        size = h['block_size'][i]
        off = offsets[i]
        expected_size = 34
        raw = buf[off:off+size]
        if size >= 4:
            (controlled,) = struct.unpack_from('<I', raw, size - 4)
        else:
            controlled = None
        flag = '' if size == expected_size else f'  <-- tamano inesperado (esperaba {expected_size}), NO fiarse del valor decodificado'
        label = TARGETS[tname].get(controlled, f'valor {controlled} fuera del enum conocido')
        print(f"  [{i}] {tname}  offset={off} size={size}{flag}")
        print(f"      Controlled Variable (ultimos 4 bytes) = {controlled} -> {label}")
    if not found_any:
        print('  (ninguna instancia de los controladores buscados en este fichero)')
    print()

if __name__ == '__main__':
    for p in sys.argv[1:]:
        inspect(p)
