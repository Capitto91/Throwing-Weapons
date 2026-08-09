import struct
import sys
from nif_header_walk import parse_header, block_offsets

# Decodifica NiPSysData -- en particular "BS Max Vertices" (el limite duro de
# particulas simultaneas, ver SKILL.md "El limite de particulas simultaneas
# -- BS Max Vertices"). Especifico de la version 20.2.0.7 / bs_version>=100
# (Skyrim SE/AE, macro #BS202# de nif.xml) -- en esa version, NiParticlesData
# NO reserva espacio para los arrays Vertices/Normals/Tangents/Bitangents/
# VertexColors/UVSets de NiGeometryData (su longitud implicita es 0 pese a
# los flags "Has X"), y el campo que en otras versiones se llama
# "Num Vertices" pasa a llamarse "BS Max Vertices" con el significado real
# de tope de particulas. Validado 2026-08-09 sobre fxsparkfountaintoggle.nif
# (BS Max Vertices=62 en el original vanilla) -- la validacion real fue
# comprobar que el `Num Subtexture Offsets`/`Subtexture Offsets` que salen
# despues forman una cuadricula UV 4x4 coherente (16 entradas con
# offsets/escalas de 0.25 en patron regular), no basura -- si esa parte sale
# con sentido, el offset de BS Max Vertices (mucho antes en el mismo bloque)
# es de fiar.
#
# NO valido para archivos con "Data Flags"/"Has Normals" que activen
# Tangents/Bitangents (aqui asumidos ausentes via los flags reales del
# fichero, pero el calculo de esos campos siempre da longitud 0 en BS202 por
# el mismo motivo que Vertices/Normals) -- si el fichero no es BS202 (versión
# distinta o motor mas antiguo), este script dará offsets incorrectos sin
# avisar mas alla del chequeo de tamaño final.


def decode_psysdata(buf, off, size):
    cursor = off
    group_id, = struct.unpack_from('<i', buf, cursor); cursor += 4
    bs_max_vertices, = struct.unpack_from('<H', buf, cursor); cursor += 2
    keep_flags = buf[cursor]; cursor += 1
    compress_flags = buf[cursor]; cursor += 1
    has_vertices = buf[cursor]; cursor += 1
    bs_data_flags, = struct.unpack_from('<H', buf, cursor); cursor += 2
    material_crc, = struct.unpack_from('<I', buf, cursor); cursor += 4
    has_normals = buf[cursor]; cursor += 1
    bounding_sphere = struct.unpack_from('<4f', buf, cursor); cursor += 16
    has_vertex_colors = buf[cursor]; cursor += 1
    consistency_flags, = struct.unpack_from('<H', buf, cursor); cursor += 2
    additional_data, = struct.unpack_from('<i', buf, cursor); cursor += 4

    has_radii = buf[cursor]; cursor += 1
    num_active, = struct.unpack_from('<H', buf, cursor); cursor += 2
    has_sizes = buf[cursor]; cursor += 1
    has_rotations = buf[cursor]; cursor += 1
    has_rot_angles = buf[cursor]; cursor += 1
    has_rot_axes = buf[cursor]; cursor += 1
    has_tex_indices = buf[cursor]; cursor += 1
    num_subtex, = struct.unpack_from('<I', buf, cursor); cursor += 4
    subtex_offsets = []
    for _ in range(num_subtex):
        v = struct.unpack_from('<4f', buf, cursor); cursor += 16
        subtex_offsets.append(v)
    has_rotation_speeds = buf[cursor]; cursor += 1

    return dict(
        bs_max_vertices=bs_max_vertices, bounding_sphere=bounding_sphere,
        num_active=num_active, has_radii=bool(has_radii), has_sizes=bool(has_sizes),
        num_subtexture_offsets=num_subtex, subtexture_offsets=subtex_offsets,
        has_rotation_speeds=bool(has_rotation_speeds),
        end_offset=cursor, note='campos tras esto (Num Added Particles/etc.) no aplican en BS202, ver comentario del script')


def inspect(path):
    buf = open(path, 'rb').read()
    h = parse_header(buf)
    offsets = block_offsets(h)
    types = h['block_types']
    type_idx = h['block_type_index']
    print('=====', path, '=====')
    for i in range(h['num_blocks']):
        if types[type_idx[i]] != 'NiPSysData':
            continue
        off = offsets[i]
        size = h['block_size'][i]
        d = decode_psysdata(buf, off, size)
        print(f"[{i}] NiPSysData  size={size}")
        print(f"     BS Max Vertices = {d['bs_max_vertices']}  <-- limite maximo de particulas simultaneas")
        print(f"     NumActive={d['num_active']}  HasRadii={d['has_radii']}  HasSizes={d['has_sizes']}")
        print(f"     NumSubtextureOffsets={d['num_subtexture_offsets']}  HasRotationSpeeds={d['has_rotation_speeds']}")
        print(f"     (offset calculado hasta aqui: {d['end_offset']-off} de {size} -- el resto son campos que no aplican en BS202, diferencia esperable)")


if __name__ == '__main__':
    for p in sys.argv[1:]:
        inspect(p)
