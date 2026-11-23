#!/usr/bin/env python3
"""make_pak.py - Junta a arvore USA/ compactada em um unico RE1DATA.PAK (R1PA).

O conteudo de cada arquivo NAO e' modificado: ele ja vem R1LZ (LZ4/ADPCM) do
re1pack.py. Aqui so concatenamos os bytes e gravamos um indice ordenado.

Formato (little-endian):
  header 32B: 'R1PA' | u16 ver=1 | u16 flags=0 | u32 count | u32 indexOff |
              u32 indexSize | u32 dataOff | u32 r0 | u32 r1
  entrada 88B: 72B nome (MAIUSCULAS, '/') | u32 offset | u32 size |
               u32 flags | u32 reserved
"""
import argparse
import os
import struct
import sys

MAGIC = b"R1PA"
HDR = struct.Struct("<4sHHIIIIII")
ENTRY = struct.Struct("<72sIIII")
assert HDR.size == 32 and ENTRY.size == 88


def normalize(path):
    return path.replace("\\", "/").upper()


def build(src, out, verify):
    files = []
    for base, _dirs, names in os.walk(src):
        for name in names:
            full = os.path.join(base, name)
            rel = normalize(os.path.relpath(full, src))
            files.append((rel, full))
    files.sort(key=lambda x: x[0])
    if not files:
        raise RuntimeError("arvore vazia: %s" % src)
    if len(files) > 20000:
        raise RuntimeError("muitas entradas")

    data = bytearray()
    entries = []
    for rel, full in files:
        with open(full, "rb") as f:
            blob = f.read()
        entries.append((rel, HDR.size + len(data), len(blob), full))
        data += blob

    index_off = HDR.size + len(data)
    header = HDR.pack(MAGIC, 1, 0, len(entries), index_off,
                      len(entries) * ENTRY.size, HDR.size, 0, 0)
    with open(out, "wb") as f:
        f.write(header)
        f.write(data)
        for rel, off, size, _full in entries:
            f.write(ENTRY.pack(rel.encode("ascii")[:72].ljust(72, b"\0"),
                               off, size, 0, 0))

    print("PAK %s" % out)
    print("  %d entradas" % len(entries))
    print("  dados: %.2f MB  indice: %.2f MB  total: %.2f MB" %
          (len(data) / 1048576.0, len(entries) * ENTRY.size / 1048576.0,
           os.path.getsize(out) / 1048576.0))

    if verify:
        with open(out, "rb") as f:
            got_hdr = HDR.unpack(f.read(HDR.size))
            if got_hdr[0] != MAGIC or got_hdr[3] != len(entries):
                raise RuntimeError("header invalido")
            for rel, off, size, full in entries:
                f.seek(off)
                got = f.read(size)
                with open(full, "rb") as sf:
                    want = sf.read()
                if got != want:
                    raise RuntimeError("divergencia em %s" % rel)
        print("  verify OK: %d/%d entradas identicas" % (len(entries), len(entries)))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src", help="pasta USA/ compactada")
    ap.add_argument("out", help="arquivo RE1DATA.PAK de saida")
    ap.add_argument("--verify", action="store_true")
    args = ap.parse_args()
    build(args.src, args.out, args.verify)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as exc:
        print("erro: %s" % exc, file=sys.stderr)
        sys.exit(1)
