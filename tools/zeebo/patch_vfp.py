#!/usr/bin/env python3
"""Neutraliza instrucoes VFP/WMMX no .mod final (watchdog para o Zeebo real).

O -mfloat-abi=soft elimina VFP do codigo do jogo, mas o runtime de excecoes
precompilado (libgcc/libstdc++) puxa os helpers _Unwind_*, que usam
vldmia/vstmia/fldmiax (VFP) e ldc2/stc2 (WMMX). Como compilamos com
-fno-exceptions e o elf2mod.x descarta .ARM.exidx, esses helpers nunca sao
executados -- mas as instrucoes ficam no binario. Este script as troca por
ARM NOP (0xE320F000).

O elf2mod grava as palavras EM BIG-ENDIAN (byte-swapped em relacao ao ELF
little-endian que o objdump mostra), entao as duas ordens de bytes sao
procuradas. Se uma codificacao aparecer mais de uma vez no .mod, o script
para: nao e seguro patchar por assinatura nesse caso.

Uso:
  python tools/zeebo/patch_vfp.py build/zeebo/re1zeebo.elf build/zeebo/re1zeebo.mod
"""

import re
import subprocess
import sys
from pathlib import Path

NOP_LE = bytes.fromhex("00f020e3")   # 0xE320F000 em little-endian
NOP_BE = NOP_LE[::-1]                # e3 20 f0 00 -- como o elf2mod grava
MNEMONIC = re.compile(r"^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(\S+)")
BAD = re.compile(r"^(v[a-z]\w*|f(?:ldm|stm|ldr|str|add|mul|sub|cpy)\w*|ldc2?|stc2?|mcrr|mrrc)$")


def find_all(data: bytes, pattern: bytes):
    out, start = [], 0
    while True:
        i = data.find(pattern, start)
        if i < 0:
            return out
        out.append(i)
        start = i + 1


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    elf, mod = sys.argv[1], sys.argv[2]
    objdump = sys.argv[3] if len(sys.argv) > 3 else "arm-none-eabi-objdump"

    dis = subprocess.check_output([objdump, "-d", elf], text=True, errors="ignore")
    encodings = {}
    for line in dis.splitlines():
        m = MNEMONIC.match(line)
        if m and BAD.match(m.group(3)):
            encodings[m.group(2)] = m.group(3)
    if not encodings:
        print("patch_vfp: nenhuma instrucao VFP/WMMX no ELF -- nada a fazer")
        return 0

    data = bytearray(Path(mod).read_bytes())
    patched = 0
    for enc, mnem in sorted(encodings.items()):
        le = bytes.fromhex(enc)
        be = le[::-1]
        hits = find_all(data, le) + find_all(data, be)
        if len(hits) != 1:
            print(f"ERRO: {enc} ({mnem}) aparece {len(hits)}x no .mod -- abortando",
                  file=sys.stderr)
            return 1
        off = hits[0]
        # Word grande: escreve o NOP na mesma ordem de bytes encontrada.
        data[off:off + 4] = NOP_LE if data[off:off + 4] == le else NOP_BE
        patched += 1
        print(f"  0x{off:08x}  {enc}  {mnem} -> NOP")

    Path(mod).write_bytes(data)
    # Verificacao final: nenhuma das codificaciones pode restar no .mod.
    restante = sum(len(find_all(data, bytes.fromhex(e))) +
                   len(find_all(data, bytes.fromhex(e)[::-1])) for e in encodings)
    if restante:
        print(f"ERRO: {restante} instrucoes VFP/WMMX restantes", file=sys.stderr)
        return 1
    print(f"patch_vfp: {patched} instrucoes VFP/WMMX neutralizadas em {mod}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
