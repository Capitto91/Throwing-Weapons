#!/usr/bin/env python3
"""Volcado de SOLO LECTURA de una quest de Skyrim SE guardada en un .esp/.esm.

Uso:  python dump_quest_esp.py <plugin.esp> [EditorID de la quest] [prefijo de scripts]

Que hace: abre el fichero con open(ruta, 'rb') (jamas escribe), recorre GRUPs/registros/subregistros,
descomprime los registros comprimidos (zlib) y muestra: flags de la quest, stages (log), objectives,
tabla de fragmentos, ramas/topics/Infos de dialogo (flags, condiciones decodificadas, Link To, fragmentos)
y los scripts adjuntos con sus properties. No es xEdit: solo interpreta los campos programados aqui.

Verificado contra capturas de la CK (2026-09-19): flags de INFO (Goodbye 0x01, Force Subtitle 0x0200,
No LIP 0x0800), Link To (TCLT), operadores de CTDA. Los nombres de funcion de condicion solo se
etiquetan para los indices observados; el resto se muestra como numero.
"""
import struct
import sys
import zlib

PATH = sys.argv[1]
QUEST = sys.argv[2] if len(sys.argv) > 2 else "CAP_ThorMjolnir_Quest_01"
PREFIX = (sys.argv[3] if len(sys.argv) > 3 else "cap").lower()

data = open(PATH, "rb").read()
recs = []  # (tipo, formid, flags, subrecords, formid del DIAL padre o 0)


def subs_of(buf):
    out, i, xx = [], 0, None
    while i + 6 <= len(buf):
        t = buf[i:i + 4].decode("latin1")
        sz = struct.unpack_from("<H", buf, i + 4)[0]
        i += 6
        if t == "XXXX":
            xx = struct.unpack_from("<I", buf, i)[0]
            i += sz
            continue
        if xx is not None:
            sz, xx = xx, None
        out.append((t, buf[i:i + sz]))
        i += sz
    return out


def walk(b, s, e, parent):
    i = s
    while i < e:
        t = b[i:i + 4].decode("latin1")
        if t == "GRUP":
            size = struct.unpack_from("<I", b, i + 4)[0]
            gtype = struct.unpack_from("<I", b, i + 12)[0]
            walk(b, i + 24, i + size, struct.unpack_from("<I", b, i + 8)[0] if gtype == 7 else parent)
            i += size
        else:
            dsz, fl, fid = struct.unpack_from("<III", b, i + 4)
            body = b[i + 24:i + 24 + dsz]
            if fl & 0x40000:
                body = zlib.decompress(body[4:])
            recs.append((t, fid, fl, subs_of(body), parent if t == "INFO" else 0))
            i += 24 + dsz


walk(data, 0, len(data), 0)
by = {r[1]: r for r in recs}


def z(d):
    return d.split(b"\x00")[0].decode("cp1252", "replace")


def sub(subs, name):
    return [d for t, d in subs if t == name]


def edid(r):
    e = sub(r[3], "EDID")
    return z(e[0]) if e else ""


def lab(fid):
    r = by.get(fid)
    return f"{edid(r) or r[0]}({r[0]} {fid:08X})" if r else f"{fid:08X}"


FUNC = {47: "GetItemCount", 72: "GetIsID", 249: "IsInDialogueWithPlayer", 560: "(#560 - condicion de keyword de inmunidad)"}
OPS = {0: "==", 1: "!=", 2: ">", 3: ">=", 4: "<", 5: "<="}
RUN = {0: "Subject", 1: "Target", 2: "Reference", 3: "Combat Target", 4: "Linked Ref", 5: "Quest Alias", 6: "Package Data", 7: "Event Data"}


def cond(d):
    op = d[0]
    val = struct.unpack_from("<f", d, 4)[0]
    fn = struct.unpack_from("<H", d, 8)[0]
    p1, p2, ro, ref = struct.unpack_from("<IIII", d, 12)
    return (f"func#{fn} {FUNC.get(fn, '')} p1={lab(p1) if p1 in by else f'{p1:08X}'} p2={p2} "
            f"{OPS.get(op >> 5, hex(op))} {val:g}  runOn={RUN.get(ro, ro)}" + (f" ref={ref:08X}" if ref else "")
            + ("  [OR]" if op & 1 else ""))


class R:
    def __init__(s, b):
        s.b, s.i = b, 0

    def u8(s): v = s.b[s.i]; s.i += 1; return v
    def u16(s): v = struct.unpack_from("<H", s.b, s.i)[0]; s.i += 2; return v
    def i16(s): v = struct.unpack_from("<h", s.b, s.i)[0]; s.i += 2; return v
    def u32(s): v = struct.unpack_from("<I", s.b, s.i)[0]; s.i += 4; return v
    def i32(s): v = struct.unpack_from("<i", s.b, s.i)[0]; s.i += 4; return v
    def f32(s): v = struct.unpack_from("<f", s.b, s.i)[0]; s.i += 4; return v
    def ws(s): n = s.u16(); v = s.b[s.i:s.i + n].decode("latin1"); s.i += n; return v


def obj(r, fmt):
    if fmt == 2:
        r.i16(); al = r.i16(); fid = r.u32()
    else:
        fid = r.u32(); al = r.i16(); r.i16()
    return (lab(fid) if fid else "NULL") + (f" alias={al}" if al != -1 else "")


def prop(r, fmt):
    name = r.ws(); t = r.u8(); r.u8()
    if t == 0: v = None
    elif t == 1: v = obj(r, fmt)
    elif t == 2: v = repr(r.ws())
    elif t == 3: v = r.i32()
    elif t == 4: v = r.f32()
    elif t == 5: v = bool(r.u8())
    elif t in (11, 12, 13, 14, 15):
        v = []
        for _ in range(r.u32()):
            v.append(obj(r, fmt) if t == 11 else repr(r.ws()) if t == 12 else r.i32() if t == 13 else r.f32() if t == 14 else bool(r.u8()))
    else: v = f"<tipo {t} no soportado>"
    return name, v


def vmad(d):
    """-> (scripts [(nombre, [(prop, valor)])], lector posicionado tras los scripts, formato)"""
    r = R(d); r.i16(); fmt = r.i16(); n = r.u16(); out = []
    for _ in range(n):
        sn = r.ws(); r.u8(); pc = r.u16()
        out.append((sn, [prop(r, fmt) for _ in range(pc)]))
    return out, r, fmt


quests = [r for r in recs if r[0] == "QUST" and edid(r) == QUEST]
if not quests:
    sys.exit(f"Quest {QUEST!r} no encontrada")
q = quests[0]; qid = q[1]
print("MASTERS:", [z(d) for t, d in recs[0][3] if t == "MAST"])
dn = (sub(q[3], "DNAM") or [b"\0" * 12])[0]
print(f"\nQUEST {lab(qid)}  flags=0x{dn[0]:02X} (0x01 = Start Game Enabled: {'SI' if dn[0] & 1 else 'NO'})  prioridad={dn[2]}")

print("\n=== STAGES ===")
cur = None
for s, d in q[3]:
    if s == "INDX":
        cur = struct.unpack_from("<H", d, 0)[0]; print(f"  Stage {cur}")
    elif s == "QSDT":
        print(f"     flags=0x{d[0]:02X} (UESP QSDT: 0x01 = Complete Quest, 0x02 = Fail Quest)")
    elif s == "CNAM" and cur is not None:
        print(f"     log: {z(d)!r}")
    elif s == "QOBJ":
        cur = None

print("\n=== OBJECTIVES ===")
o, of = None, 0
for s, d in q[3]:
    if s == "QOBJ":
        o = struct.unpack_from("<H", d, 0)[0]; of = 0
    elif s == "FNAM" and o is not None and len(d) == 4:
        of = struct.unpack("<I", d)[0]
    elif s == "NNAM" and o is not None:
        print(f"  objective {o}: {z(d)!r}  flags=0x{of:X} (0x01 = ORed With Previous)"); o = None

ALIAS_FL = {0x1: "Reserves", 0x2: "Optional", 0x4: "QuestObject", 0x8: "AllowReuse", 0x10: "AllowDead", 0x20: "Essential",
            0x40: "AllowDisabled", 0x100: "AllowReserved", 0x200: "Protected"}
print()
print("=== ALIAS ===")
al = False
for s, d in q[3]:
    if s == "ALST":
        print(f"  alias #{struct.unpack('<I', d)[0]}", end=""); al = True
    elif s == "ALID" and al:
        print(f" {z(d)}", end="")
    elif s == "FNAM" and al and len(d) == 4:
        f = struct.unpack("<I", d)[0]; print(f"  flags=0x{f:X} {[n for m, n in ALIAS_FL.items() if f & m]}")
    elif s == "ALFR" and al:
        fid = struct.unpack("<I", d)[0]; r0 = by.get(fid)
        nm = [struct.unpack("<I", x)[0] for t, x in (r0[3] if r0 else []) if t == "NAME"]
        print(f"     forzada a {lab(fid)}  base={lab(nm[0]) if nm else '?'}")
print()
print("=== TARGETS DE OBJECTIVES (con sus condiciones) ===")
o = None
for s, d in q[3]:
    if s == "QOBJ":
        o = struct.unpack_from("<H", d, 0)[0]
    elif s == "QSTA" and o is not None:
        a, fl = struct.unpack("<iI", d[:8]); print(f"  objective {o}: target alias #{a} flags=0x{fl:X}")
    elif s == "CTDA" and o is not None:
        print(f"       cond: {cond(d)}")

v = sub(q[3], "VMAD")
if v:
    try:
        scripts, r, fmt = vmad(v[0])
        r.u8(); fc = r.u16(); fn = r.ws()
        print(f"\n=== FRAGMENTOS DE STAGE ({fc}) en {fn} ===")
        for _ in range(fc):
            st = r.u16(); r.i16(); r.i32(); r.u8(); r.ws(); fu = r.ws()
            print(f"  stage {st} -> {fu}")
    except Exception as e:
        print("  (no se pudo leer la tabla de fragmentos:", e, ")")

print("\n=== DIALOGO ===")
for r0 in recs:
    if r0[0] == "DLBR" and sub(r0[3], "QNAM") and struct.unpack("<I", sub(r0[3], "QNAM")[0])[0] == qid:
        dnm = sub(r0[3], "DNAM"); fl = struct.unpack("<I", dnm[0])[0] if dnm else 0
        sn = sub(r0[3], "SNAM"); st = struct.unpack("<I", sn[0])[0] if sn else 0
        print(f"  RAMA {lab(r0[1])} flags={fl} (1 Top-Level, 2 Blocking, 4 Exclusive) inicio={lab(st) if st else 'NONE'}")
dials = {}
for r0 in recs:
    if r0[0] == "DIAL" and sub(r0[3], "QNAM") and struct.unpack("<I", sub(r0[3], "QNAM")[0])[0] == qid:
        dials[r0[1]] = r0
        sn = sub(r0[3], "SNAM"); bn = sub(r0[3], "BNAM"); fu = sub(r0[3], "FULL")
        print(f"  TOPIC {lab(r0[1])} subtipo={sn[0].decode('latin1') if sn else '?'} "
              f"rama={lab(struct.unpack('<I', bn[0])[0]) if bn else '-'} texto={z(fu[0]) if fu else ''!r}")
for r0 in recs:
    if r0[0] != "INFO" or r0[4] not in dials:
        continue
    en = sub(r0[3], "ENAM"); fl = struct.unpack_from("<H", en[0], 0)[0] if en else 0
    names = [n for m, n in ((0x1, "Goodbye"), (0x200, "ForceSubtitle"), (0x800, "NoLIP")) if fl & m]
    print(f"\n  INFO {r0[1]:08X} de {lab(r0[4])}")
    print(f"     flags=0x{fl:04X} {names}  otros=0x{fl & ~(0x1 | 0x200 | 0x800):04X}")
    for s, d in r0[3]:
        if s == "NAM1": print(f"     texto: {z(d)!r}")
        elif s == "CTDA": print(f"     cond: {cond(d)}")
        elif s == "TCLT": print(f"     Link To: {lab(struct.unpack('<I', d)[0])}")
        elif s == "ANAM": print(f"     Speaker: {lab(struct.unpack('<I', d)[0])}")
        elif s == "VMAD":
            sc, _, _ = vmad(d); print(f"     fragmento(s): {[x[0] for x in sc]}")

print(f"\n=== SCRIPTS ADJUNTOS (prefijo {PREFIX!r}) ===")
for r0 in recs:
    for s, d in r0[3]:
        if s != "VMAD" or r0[0] in ("QUST", "INFO"):
            continue
        try:
            sc, _, _ = vmad(d)
        except Exception:
            continue
        sel = [x for x in sc if x[0].lower().startswith((PREFIX, "defaultsetstage"))]
        if sel:
            print(f"\n  {r0[0]} {lab(r0[1])}")
            for sn, props in sel:
                print(f"    script {sn}")
                for pn, pv in props:
                    print(f"       {pn} = {pv}")
