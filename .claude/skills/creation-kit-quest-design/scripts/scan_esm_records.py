#!/usr/bin/env python3
"""Busqueda de SOLO LECTURA de registros por Editor ID (subcadena) o FormID en un .esp/.esm de Skyrim SE.

Uso:  python scan_esm_records.py <plugin> <patron> [<patron> ...]
      patron = subcadena del Editor ID (sin distinguir mayusculas) o FormID en hexadecimal (0x...)

Muestra tipo, FormID, Editor ID y campos decodificados de los tipos mas utiles: SHOU (palabras), WOOP, SPEL/MGEF
(efectos, arquetipo, projectil, explosion), EXPL, PROJ. Nunca escribe nada. Recorre todo el fichero (los masters
grandes tardan unos segundos).
"""
import struct
import sys
import zlib

path = sys.argv[1]
pats = [p.lower() for p in sys.argv[2:]]
data = open(path, "rb").read()
found = []


def subs_of(buf):
    out, i, xx = [], 0, None
    while i + 6 <= len(buf):
        t = buf[i:i + 4].decode("latin1")
        sz = struct.unpack_from("<H", buf, i + 4)[0]
        i += 6
        if t == "XXXX":
            xx = struct.unpack_from("<I", buf, i)[0]; i += sz; continue
        if xx is not None:
            sz, xx = xx, None
        out.append((t, buf[i:i + sz])); i += sz
    return out


def z(d):
    return d.split(b"\x00")[0].decode("cp1252", "replace")


def match(fid, ed):
    for p in pats:
        if p.startswith("0x"):
            if (fid & 0xFFFFFF) == (int(p, 16) & 0xFFFFFF):
                return True
        elif p in ed.lower():
            return True
    return False


def walk(b, s, e):
    i = s
    while i < e:
        t = b[i:i + 4]
        if t == b"GRUP":
            size = struct.unpack_from("<I", b, i + 4)[0]
            walk(b, i + 24, i + size); i += size
        else:
            dsz, fl, fid = struct.unpack_from("<III", b, i + 4)
            body = b[i + 24:i + 24 + dsz]
            # solo se descomprime si el EDID (no comprimido en el registro) no basta: se descomprime lo comprimido
            if fl & 0x40000:
                try:
                    body = zlib.decompress(body[4:])
                except Exception:
                    body = b""
            subs = subs_of(body)
            ed = next((z(d) for s2, d in subs if s2 == "EDID"), "")
            if t in (b"SHOU", b"WOOP", b"SPEL", b"MGEF", b"EXPL", b"PROJ", b"HAZD", b"WTHR", b"ACTI", b"MISC", b"WEAP") and match(fid, ed):
                found.append((t.decode(), fid, ed, subs))
            i += 24 + dsz


walk(data, 0, len(data))
by = {f[1]: f for f in found}
lab = lambda x: (f"{by[x][2]}({by[x][0]} {x:08X})" if x in by else f"{x:08X}")
for t, fid, ed, subs in found:
    print(f"\n{t} {fid:08X} {ed}")
    for s, d in subs:
        if s == "EDID":
            continue
        if s == "FULL" and t != "WOOP":
            print(f"   FULL: {z(d)!r}")
        elif s == "SNAM" and t == "SHOU" and len(d) >= 12:
            w, sp, rc = struct.unpack("<IIf", d[:12]); print(f"   palabra {lab(w)}  hechizo {lab(sp)}  recarga {rc}")
        elif s == "EFID":
            print(f"   efecto: {lab(struct.unpack('<I', d)[0])}")
        elif s == "EFIT":
            m, a, du = struct.unpack("<fII", d[:12]); print(f"      mag={m} area={a} dur={du}")
        elif s == "SPIT" and len(d) >= 24:
            cost, fl2, typ, ch, cast, deliv = struct.unpack("<IIIfII", d[:24]); print(f"   SPIT tipo={typ} (11=Voice Power) cast={cast} delivery={deliv}")
        elif s == "DATA" and t == "MGEF" and len(d) >= 0x58:
            arch = struct.unpack_from("<I", d, 0x40)[0]
            proj, expl, cast, deliv = struct.unpack_from("<IIII", d, 0x48)
            print(f"   DATA arquetipo={arch} projectil={lab(proj)} explosion={lab(expl)} cast={cast} delivery={deliv}")
        elif s in ("MODL", "NAM1", "DESC"):
            print(f"   {s}: {z(d)!r}")
        elif s == "DATA" and t == "EXPL" and len(d) >= 20:
            print(f"   DATA(EXPL) light={struct.unpack_from('<I', d, 4)[0]:08X} sound1={struct.unpack_from('<I', d, 8)[0]:08X} flags=0x{struct.unpack_from('<I', d, 24)[0]:X}" if len(d) >= 28 else f"   DATA(EXPL) {d.hex()}")
