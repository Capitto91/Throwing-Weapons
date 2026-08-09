import struct
import sys
from nif_header_walk import parse_header, block_offsets

# Decodifica NiControllerManager (nombre, secuencias) y cada NiControllerSequence
# (Cycle Type/Frequency/Start/Stop Time + su lista ControlledBlock completa:
# Interpolator/Controller ref+tipo, y los nombres InterpolatorID/CtrlType via
# tabla de strings). Validado 2026-08-09 sobre fxsparkfountaintoggle.nif y
# lightningstormhandeffects.nif -- ver SKILL.md, seccion "Activacion de
# particulas Bethesda via NiControllerManager".
#
# Asume version 20.2.0.7 / bs_version>=100 (Skyrim SE/AE) -- el layout de
# ControlledBlock (29 bytes fijos: Interpolator Ref(4) + Controller Ref(4) +
# Priority(1) + Node Name/PropType/CtrlType/CtrlID/InterpID, 5 indices de
# string de 4 bytes cada uno = 20) es especifico de esta version (desde
# 20.1.0.1, sin los campos de string-palette de versiones intermedias).

CYCLE_TYPES = {0: 'CYCLE_LOOP', 1: 'CYCLE_REVERSE', 2: 'CYCLE_CLAMP'}


def s(strings, idx):
    return strings[idx] if 0 <= idx < len(strings) else None


def decode_controlled_block(buf, base, strings):
    interp, ctrl = struct.unpack_from('<ii', buf, base)
    prio = buf[base + 8]
    node_name, prop_type, ctrl_type, ctrl_id, interp_id = struct.unpack_from('<5i', buf, base + 9)
    return dict(
        interpolator=interp, controller=ctrl, priority=prio,
        node_name=s(strings, node_name), property_type=s(strings, prop_type),
        controller_type=s(strings, ctrl_type), controller_id=s(strings, ctrl_id),
        interpolator_id=s(strings, interp_id))


def decode_sequence(buf, off, strings, types, type_idx):
    name_idx, = struct.unpack_from('<i', buf, off)
    num_cb, = struct.unpack_from('<I', buf, off + 4)
    cb_off = off + 12
    blocks = [decode_controlled_block(buf, cb_off + k * 29, strings) for k in range(num_cb)]
    trailer_off = cb_off + num_cb * 29
    weight, text_keys, cycle_type, freq, start, stop = struct.unpack_from('<fiIfff', buf, trailer_off)
    return dict(
        name=s(strings, name_idx), num_controlled_blocks=num_cb, controlled_blocks=blocks,
        weight=weight, text_keys=text_keys, cycle_type=CYCLE_TYPES.get(cycle_type, cycle_type),
        frequency=freq, start_time=start, stop_time=stop)


def decode_manager(buf, off, types, type_idx):
    # NiTimeController header (26B) ya no hace falta para esto -- solo los
    # campos propios de NiControllerManager, justo despues.
    cumulative = buf[off + 26]
    num_seq, = struct.unpack_from('<I', buf, off + 27)
    cursor = off + 31
    seqs = []
    for _ in range(num_seq):
        (seq,) = struct.unpack_from('<i', buf, cursor)
        seqs.append(seq)
        cursor += 4
    palette, = struct.unpack_from('<i', buf, cursor)
    return dict(cumulative=bool(cumulative), num_sequences=num_seq, sequences=seqs, object_palette=palette)


def inspect(path):
    buf = open(path, 'rb').read()
    h = parse_header(buf)
    offsets = block_offsets(h)
    types = h['block_types']
    type_idx = h['block_type_index']
    strings = h['strings']

    def t(i):
        return types[type_idx[i]] if i is not None and i != -1 else None

    print('=====', path, '=====')
    for i in range(h['num_blocks']):
        if t(i) == 'NiControllerManager':
            m = decode_manager(buf, offsets[i], types, type_idx)
            print(f"[{i}] NiControllerManager  Cumulative={m['cumulative']}  NumSequences={m['num_sequences']}")
            print(f"     Sequences={[(s_, t(s_)) for s_ in m['sequences']]}  ObjectPalette={m['object_palette']}({t(m['object_palette'])})")
        elif t(i) == 'NiControllerSequence':
            seq = decode_sequence(buf, offsets[i], strings, types, type_idx)
            print(f"[{i}] NiControllerSequence Name={seq['name']!r}  CycleType={seq['cycle_type']}  Freq={seq['frequency']}  Start={seq['start_time']}  Stop={seq['stop_time']}")
            for k, b in enumerate(seq['controlled_blocks']):
                it = t(b['interpolator'])
                ct = t(b['controller'])
                print(f"      [{k}] Interp={b['interpolator']}({it}) Ctrl={b['controller']}({ct}) Node={b['node_name']!r} CtrlType={b['controller_type']!r} InterpID={b['interpolator_id']!r}")
    print()


if __name__ == '__main__':
    for p in sys.argv[1:]:
        inspect(p)
