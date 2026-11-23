#!/usr/bin/env python3
"""re1pack.py - Empacota os assets do RE1 no formato R1LZ (LZ4 bloco + header).

Nao apaga nem altera nenhum arquivo de origem: le a arvore SRC e escreve uma
arvore OUT com os MESMOS nomes relativos, mas com conteudo compactado.

Camadas:
  * arquivosNormais (.wav/.rdt/.pak/.tim/.emd/...) -> R1LZ inteiro
  * AVI Cinepak -> cada chunk 00dc/00db/01wb e' compactado separadamente
    (o demuxer do port descompacta um frame por vez, sem carregar 55 MB na RAM)

O runtime correspondente esta' em src/platform/zeebo/re1_lz4.* e e' ligado
de forma transparente em plat_file_read_all(), syscalls (_open/_read) e
avi_demux.cpp.

Uso:
  python tools/zeebo/re1pack.py selftest
  python tools/zeebo/re1pack.py scan  SRC
  python tools/zeebo/re1pack.py pack  SRC OUT [--verify] [--max-file 4]
"""
import argparse
import ctypes
import os
import struct
import sys
import zlib

from lz4.block import compress as lz4_compress, decompress as lz4_decompress

MAGIC = b"R1LZ"
HDR = struct.Struct("<4sHHIII")  # magic, version, flags, orig, comp, crc
HEADER_SIZE = HDR.size
MAX_MEMFILE = 4 * 1024 * 1024  # deve casar com RE1_MEMFILE_MAX no syscalls.cpp
AVI_TAGS = (b"00dc", b"00db", b"01wb")

# IMA ADPCM: tabelas padrao (mesmas do re1_lz4.cpp)
ADPCM_STEP = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 875, 963, 1059, 1165, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
)
ADPCM_INDEX = (-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8)
HDR_ADPCM = struct.Struct("<4sHHIIIIHHII")  # 36 bytes
assert HDR_ADPCM.size == 36

# O encoder em C e' ~200x mais rapido que o loop Python para 181 MB de WAV.
_FAST = None
try:
    _dll = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    "adpcm_fast.dll"))
    _dll.re1_adpcm_encode.argtypes = [ctypes.POINTER(ctypes.c_int16),
                                      ctypes.c_int, ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_uint8)]
    _dll.re1_adpcm_decode.argtypes = [ctypes.POINTER(ctypes.c_uint8),
                                      ctypes.c_int, ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_int16)]
    _FAST = _dll
except OSError:
    pass  # fallback: implementacao Python abaixo


def u32(v):
    return struct.pack("<I", v & 0xFFFFFFFF)


def rd32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def u16(v):
    return struct.pack("<H", v & 0xFFFF)


def rd16(data, off):
    return struct.unpack_from("<H", data, off)[0]


def _compress_block(data):
    try:
        return lz4_compress(data, mode="high_compression", compression=9,
                            store_size=False)
    except (TypeError, ValueError):
        return lz4_compress(data, store_size=False)


def pack_blob(data, min_gain=0.04):
    """Devolve bytes R1LZ, ou os bytes originais se nao compensar."""
    if len(data) < 128:
        return data
    comp = _compress_block(data)
    if len(comp) + HEADER_SIZE > len(data) * (1.0 - min_gain):
        return data
    return HDR.pack(MAGIC, 1, 1, len(data), len(comp), zlib.crc32(data)) + comp


# ---------------------------------------------------------------------------
# IMA ADPCM 4-bit: perda pequena e audivel, metade do tamanho do PCM16.
# ---------------------------------------------------------------------------
def adpcm_encode(pcm, channels):
    """Codifica PCM16 LE em um bloco IMA por canal. Devolve bytes."""
    per = len(pcm) // (2 * channels)
    if _FAST is not None and per:
        src = (ctypes.c_int16 * (per * channels)).from_buffer_copy(pcm)
        out = (ctypes.c_uint8 * ((4 + (per + 1) // 2) * channels))()
        _FAST.re1_adpcm_encode(src, per, channels, out)
        return bytes(out)
    blocks = []
    for ch in range(channels):
        pred, idx = 0, 0
        nibbles = bytearray()
        for i in range(per):
            sample = struct.unpack_from("<h", pcm, (i * channels + ch) * 2)[0]
            step = ADPCM_STEP[idx]
            diff = sample - pred
            absd = abs(diff)
            n = 0
            if absd >= step:
                n |= 4
                absd -= step
            if absd >= step >> 1:
                n |= 2
                absd -= step >> 1
            if absd >= step >> 2:
                n |= 1
            if diff < 0:
                n |= 8
            d = step >> 3
            if n & 1:
                d += step >> 2
            if n & 2:
                d += step >> 1
            if n & 4:
                d += step
            pred = pred - d if n & 8 else pred + d
            pred = max(-32768, min(32767, pred))
            idx = max(0, min(88, idx + ADPCM_INDEX[n]))
            nibbles.append(n)
        out = bytearray(struct.pack("<hBB", 0, 0, 0))
        for i in range(0, len(nibbles), 2):
            hi = nibbles[i]
            lo = nibbles[i + 1] if i + 1 < len(nibbles) else 0
            out.append((hi << 4) | lo)
        blocks.append(bytes(out))
    return b"".join(blocks)


def adpcm_decode(adpcm, channels, pcm_bytes):
    """Inverso exato de adpcm_encode (usado no --verify)."""
    per = pcm_bytes // (2 * channels)
    block = 4 + (per + 1) // 2
    if _FAST is not None and per:
        if len(adpcm) < block * channels:
            raise ValueError("payload ADPCM truncado")
        src = (ctypes.c_uint8 * (block * channels)).from_buffer_copy(adpcm[:block * channels])
        out = (ctypes.c_int16 * (per * channels))()
        _FAST.re1_adpcm_decode(src, per, channels, out)
        return bytes(out)
    out = bytearray()
    for ch in range(channels):
        b = adpcm[ch * block:(ch + 1) * block]
        pred = struct.unpack_from("<h", b, 0)[0]
        idx = b[2]
        for i in range(per):
            byte = b[4 + i // 2]
            n = (byte & 0x0F) if (i & 1) else (byte >> 4)
            step = ADPCM_STEP[idx]
            d = step >> 3
            if n & 1:
                d += step >> 2
            if n & 2:
                d += step >> 1
            if n & 4:
                d += step
            pred = pred - d if n & 8 else pred + d
            pred = max(-32768, min(32767, pred))
            idx = max(0, min(88, idx + ADPCM_INDEX[n]))
            out += struct.pack("<h", pred)
    # Reinterleaves: adpcm_encode guardou um bloco inteiro por canal.
    chans = []
    for ch in range(channels):
        seg = out[ch * per * 2:(ch + 1) * per * 2]
        chans.append([seg[i * 2:(i + 1) * 2] for i in range(per)])
    result = bytearray()
    for i in range(per):
        for ch in range(channels):
            result += chans[ch][i]
    return bytes(result)


def read_wav_info(data):
    if len(data) < 44 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        return None
    pos, fmt, dat = 12, None, None
    while pos + 8 <= len(data):
        tag = data[pos:pos + 4]
        sz = rd32(data, pos + 4)
        p = pos + 8
        if tag == b"fmt " and sz >= 16:
            fmt = (rd16(data, p), rd16(data, p + 2), rd32(data, p + 4),
                   rd32(data, p + 8), rd16(data, p + 12), rd16(data, p + 14))
        elif tag == b"data":
            dat = (p, sz)
        pos = p + sz + (sz & 1)
        if fmt and dat:
            break
    if not fmt or not dat:
        return None
    return fmt, dat


def pack_adpcm_wav(data):
    info = read_wav_info(data)
    if not info:
        return None
    fmt, (data_off, data_len) = info
    tag, ch, rate, _avg, _align, bits = fmt
    if tag != 1 or bits != 16 or ch not in (1, 2) or rate < 8000:
        return None
    pcm = data[data_off:data_off + data_len]
    if len(pcm) != data_len or data_len < 64:
        return None
    adpcm = adpcm_encode(pcm, ch)
    trailing = data[data_off + data_len:]   # chunks depois do 'data' (ex.: fact)
    # O CRC e' do WAV RECONSTRUDO (ADPCM e' lossy). O decoder C calcula o
    # mesmo CRC apos decodificar; Python e C usam a mesma tabela/estado.
    rebuilt = data[:data_off] + adpcm_decode(adpcm, ch, data_len) + trailing
    comp_len = data_off + len(adpcm) + len(trailing)
    hdr = HDR_ADPCM.pack(MAGIC, 1, 2, len(data), comp_len,
                         zlib.crc32(rebuilt), rate, ch, 16, data_len, data_off)
    return hdr + data[:data_off] + adpcm + trailing


def unpack_blob(data):
    """Inverte pack_blob / pack_adpcm_wav. Levanta ValueError se invalido."""
    if len(data) < HEADER_SIZE or data[:4] != MAGIC:
        return data
    magic, ver, flags, orig, comp, crc = HDR.unpack_from(data, 0)
    if magic != MAGIC or ver != 1 or flags not in (1, 2):
        raise ValueError("header R1LZ invalido")
    hdr_size = HDR_ADPCM.size if flags == 2 else HEADER_SIZE
    if len(data) < hdr_size + comp:
        raise ValueError("payload truncado")
    payload = data[hdr_size:hdr_size + comp]
    if flags == 1:
        out = lz4_decompress(payload, uncompressed_size=orig)
    else:
        _m, _v, _f, orig, _c, _crc, rate, ch, bits, pcm_bytes, off = \
            HDR_ADPCM.unpack_from(data, 0)
        trailing = orig - off - pcm_bytes
        adpcm = payload[off:len(payload) - trailing]
        out = bytearray(payload[:off])
        out += adpcm_decode(adpcm, ch, pcm_bytes)
        if trailing:
            out += payload[len(payload) - trailing:]
        out = bytes(out)
    if len(out) != orig or zlib.crc32(out) != crc:
        raise ValueError("CRC/div tamanho divergente")
    return out


# ---------------------------------------------------------------------------
# RIFF / AVI: reescreve chunks recursivamente, mantendo a estrutura e o padding.
# ---------------------------------------------------------------------------
def rebuild_region(data, start, end, transform):
    out = bytearray()
    pos = start
    while pos + 8 <= end:
        tag = bytes(data[pos:pos + 4])
        size = rd32(data, pos + 4)
        pstart = pos + 8
        pend = pstart + size
        if pend > end:
            break
        if tag == b"LIST" and size >= 4:
            inner = rebuild_region(data, pstart + 4, pend, transform)
            payload = bytes(data[pstart:pstart + 4]) + bytes(inner)
        else:
            payload = transform(tag, bytes(data[pstart:pend]))
        out += tag + u32(len(payload)) + payload
        # O padding e' do NOVO payload; o avanco no arquivo de entrada usa
        # o tamanho antigo (onde o padding original estava).
        if len(payload) & 1:
            out += b"\x00"
        pos = pend + (size & 1)
    return out


def is_avi(data):
    return len(data) >= 12 and data[:4] == b"RIFF" and data[8:12] == b"AVI "


def transform_avi(data, fn):
    if not is_avi(data):
        raise ValueError("nao e AVI")
    body = rebuild_region(data, 12, len(data), fn)
    return b"RIFF" + u32(len(body) + 4) + b"AVI " + body


def walk_avi(data, start, end, visit):
    pos = start
    while pos + 8 <= end:
        tag = data[pos:pos + 4]
        sz = rd32(data, pos + 4)
        p = pos + 8
        pend = p + sz
        if pend > end:
            break
        visit(tag, p, sz)
        if tag == b"LIST" and sz >= 4:
            walk_avi(data, p + 4, pend, visit)
        pos = pend + (sz & 1)


def avi_audio_info(data):
    state = {"auds": False, "rate": 0, "ch": 0, "bits": 0}

    def visit(tag, p, sz):
        if tag == b"LIST" and sz >= 4 and data[p:p + 4] == b"strl":
            state["auds"] = False
        if tag == b"strh" and sz >= 8:
            state["auds"] = data[p:p + 4] == b"auds"
        elif tag == b"strf" and state["auds"] and sz >= 16:
            state["ch"] = rd16(data, p + 2)
            state["rate"] = rd32(data, p + 4)
            state["bits"] = rd16(data, p + 14)
    walk_avi(data, 12, len(data), visit)
    return state["rate"], state["ch"], state["bits"]


def pack_pcm_adpcm(pcm, rate, ch):
    """PCM cru de um chunk 01wb -> container ADPCM (dataOffset=0)."""
    if ch not in (1, 2) or rate == 0 or len(pcm) < 64:
        return None
    adpcm = adpcm_encode(pcm, ch)
    crc = zlib.crc32(adpcm_decode(adpcm, ch, len(pcm)))
    hdr = HDR_ADPCM.pack(MAGIC, 1, 2, len(pcm), len(adpcm), crc,
                         rate, ch, 16, len(pcm), 0)
    return hdr + adpcm


def build_movie_audio(data):
    """Extrai o PCM inteiro de um AVI e devolve um container ADPCM
    (flags=2, dataOffset=0) para streaming via ISource no Zeebo."""
    rate, ch, bits = avi_audio_info(data)
    if rate == 0 or ch not in (1, 2) or bits != 16:
        return None
    pcm = bytearray()

    def visit(tag, p, sz):
        if tag == b"01wb":
            pcm += data[p:p + sz]
    walk_avi(data, 12, len(data), visit)
    if len(pcm) < 64:
        return None
    adpcm = adpcm_encode(bytes(pcm), ch)
    crc = zlib.crc32(adpcm_decode(adpcm, ch, len(pcm)))
    hdr = HDR_ADPCM.pack(MAGIC, 1, 2, len(pcm), len(adpcm), crc,
                         rate, ch, 16, len(pcm), 0)
    return hdr + adpcm


def pack_avi(data):
    rate, ch, bits = avi_audio_info(data)
    def fn(tag, payload):
        if tag in (b"00dc", b"00db"):
            return pack_blob(payload)
        if tag == b"01wb" and bits == 16:
            q = pack_pcm_adpcm(payload, rate, ch)
            if q is not None and len(q) < len(payload) * 0.8:
                return q
            return pack_blob(payload)
        return payload
    return transform_avi(data, fn)


def unpack_avi(data):
    def dec(tag, p):
        if tag in AVI_TAGS and p[:4] == MAGIC:
            return unpack_blob(p)
        return p
    return transform_avi(data, dec)


def pack_file(data, max_file, adpcm=True):
    if data[:4] == MAGIC and data[6:8] == struct.pack("<H", 2) and data[4:6] == struct.pack("<H", 1):
        return data, ("audio-stream", "raw")  # ja' e' R1LZ/ADPCM
    if is_avi(data):
        return pack_avi(data), ("avi-chunks", "compressed")
    if len(data) <= max_file:
        if adpcm and len(data) >= 64 and data[:4] == b"RIFF" and data[8:12] == b"WAVE":
            wav = pack_adpcm_wav(data)
            if wav is not None and len(wav) < len(data) * 0.8:
                return wav, ("adpcm", "compressed")
        packed = pack_blob(data)
        return packed, ("file", "compressed" if packed is not data else "raw")
    return data, ("file", "too-big-raw")


def unpack_file(data, how):
    if how == "avi-chunks":
        return unpack_avi(data)
    return unpack_blob(data)


def verify_result(new, old):
    """ADPCM e' lossy: valida tamanho/estrutura/CRC, nao igualdade de audio."""
    if is_avi(new):
        back = unpack_avi(new)
        if len(back) != len(old):
            raise RuntimeError("AVI: tamanho divergiu")

        def chunks(data):
            vids, auds = [], []
            def visit(tag, p, sz):
                if tag in (b"00dc", b"00db"):
                    vids.append(data[p:p + sz])
                elif tag == b"01wb":
                    auds.append((p, sz))
            walk_avi(data, 12, len(data), visit)
            return vids, auds
        old_v, _ = chunks(old)
        new_v, new_a = chunks(back)
        if len(old_v) != len(new_v) or any(a != b for a, b in zip(old_v, new_v)):
            raise RuntimeError("AVI: frames de video divergiram")
        _, packed_a = chunks(new)
        for p, sz in packed_a:
            payload = new[p:p + sz]
            if payload[:4] == MAGIC:
                unpack_blob(payload)  # valida CRC do PCM decodificado
        return

    back = unpack_blob(new)
    if len(back) != len(old):
        raise RuntimeError("tamanho divergiu: %d != %d" % (len(back), len(old)))
    if new[:4] == MAGIC:
        _m, _v, flags, _o, _c, crc = HDR.unpack_from(new, 0)
        if flags == 2 and zlib.crc32(back) != crc:
            raise RuntimeError("CRC ADPCM divergente")
        if flags == 1 and back != old:
            raise RuntimeError("LZ4 nao e' identico")
    elif back != old:
        raise RuntimeError("arquivo nao e' identico")


# ---------------------------------------------------------------------------
# Arvore
# ---------------------------------------------------------------------------
def category(path, root):
    rel = os.path.relpath(path, root).replace("\\", "/")
    return rel.split("/")[0] if "/" in rel else "(raiz)"


def selftest():
    import hashlib
    import random

    rnd = random.Random(0x1234)
    samples = [
        b"ABCDEFGH" * 1000,
        bytes(rnd.getrandbits(8) for _ in range(65536)),
        b"".join(struct.pack("<H", (i * 7) & 0xFFFF) for i in range(20000)),
        bytes(range(256)) * 400,
    ]
    for s in samples:
        p = pack_blob(s)
        assert unpack_blob(p) == s, "roundtrip de blob falhou"

    # WAV PCM16 -> ADPCM 4-bit e volta exatamente (os bytes do cabecalho,
    # incluindo chunks smpl, sao preservados).
    import math
    for chans in (1, 2):
        samples_n = 4096
        pcm = bytearray()
        for i in range(samples_n):
            v = int(11000.0 * math.sin(i / 17.0))
            for _ in range(chans):
                pcm += struct.pack("<h", v)
        pcm = bytes(pcm)
        wav = (b"RIFF" + u32(36 + len(pcm)) + b"WAVE" +
               b"fmt " + u32(16) + u16(1) + u16(chans) +
               u32(22050) + u32(22050 * 2 * chans) + u16(2 * chans) + u16(16) +
               b"data" + u32(len(pcm)) + pcm)
        packed = pack_adpcm_wav(wav)
        assert packed is not None, "pack ADPCM falhou (%d ch)" % chans
        verify_result(packed, wav)
        print("  ADPCM %dch: %d -> %d bytes" %
              (chans, len(wav), len(packed)))
    # AVI sintetico: header + LIST movi com 2 chunks de video e 1 de audio.
    # Os frames entram CRUS; pack_avi/unpack_avi e' que devem comprimi-los.
    frames = samples[:3]
    movi = b"".join(tag + u32(len(f)) + f + (b"\x00" if len(f) & 1 else b"")
                    for tag, f in zip((b"00dc", b"00dc", b"01wb"), frames))
    lst = b"LIST" + u32(4 + len(movi)) + b"movi" + movi
    avi = b"RIFF" + u32(4 + len(lst)) + b"AVI " + lst
    out = pack_avi(avi)
    assert unpack_avi(out) == avi, "roundtrip de AVI falhou"
    print("selftest OK: %d blobs + AVI sintetico" % len(samples))


def scan(root):
    total = 0
    counts = 0
    by_cat = {}
    by_ext = {}
    for base, _dirs, files in os.walk(root):
        for name in files:
            p = os.path.join(base, name)
            n = os.path.getsize(p)
            total += n
            counts += 1
            cat = category(p, root)
            ext = os.path.splitext(name)[1].lower()
            by_cat[cat] = by_cat.get(cat, 0) + n
            by_ext[ext] = by_ext.get(ext, 0) + n
    print("SCAN %s" % root)
    print("  %d arquivos, %.2f MB" % (counts, total / 1048576.0))
    print("  por pasta:")
    for k, v in sorted(by_cat.items(), key=lambda kv: -kv[1]):
        print("    %-12s %9.2f MB" % (k, v / 1048576.0))
    print("  por extensao:")
    for k, v in sorted(by_ext.items(), key=lambda kv: -kv[1])[:12]:
        print("    %-10s %9.2f MB" % (k, v / 1048576.0))


def pack_tree(src, out, max_file, verify):
    orig_total = packed_total = 0
    files = 0
    by_cat = {}
    methods = {}

    for base, _dirs, names in os.walk(src):
        relbase = os.path.relpath(base, src)
        dstbase = out if relbase == "." else os.path.join(out, relbase)
        os.makedirs(dstbase, exist_ok=True)
        for name in names:
            sp = os.path.join(base, name)
            dp = os.path.join(dstbase, name)
            with open(sp, "rb") as f:
                data = f.read()
            new, (kind, method) = pack_file(data, max_file)
            if verify:
                try:
                    verify_result(new, data)
                except Exception as exc:
                    raise RuntimeError("%s: %s" % (sp, exc)) from exc
            with open(dp, "wb") as f:
                f.write(new)

            # Entrada extra de audio do FMV em ADPCM puro, para o IMedia
            # tocar via ISource sem montar o WAV PCM inteiro na RAM.
            if is_avi(data):
                audio = build_movie_audio(data)
                if audio:
                    ap = os.path.splitext(dp)[0] + ".AUDIO"
                    with open(ap, "wb") as af:
                        af.write(audio)
                    if verify:
                        unpack_blob(audio)  # valida CRC do PCM decodificado
                    packed_total += len(audio)
                    files += 1
                    by_cat[cat] = (by_cat[cat][0], by_cat[cat][1] + len(audio))

            n0, n1 = len(data), len(new)
            orig_total += n0
            packed_total += n1
            files += 1
            cat = category(sp, src)
            a, b = by_cat.get(cat, (0, 0))
            by_cat[cat] = (a + n0, b + n1)
            methods[method] = methods.get(method, 0) + 1

    print("PACK %s -> %s" % (src, out))
    print("  %d arquivos (verify=%s)" % (files, "on" if verify else "off"))
    print("  %-12s %12s %12s %8s" % ("pasta", "original MB", "pack MB", "ganho"))
    for cat, (a, b) in sorted(by_cat.items(), key=lambda kv: -kv[1][0]):
        print("  %-12s %12.2f %12.2f %7.1f%%" %
              (cat, a / 1048576.0, b / 1048576.0,
               100.0 * (a - b) / a if a else 0.0))
    print("  metodos: %s" % methods)
    print("  TOTAL: %.2f MB -> %.2f MB  (economia %.2f MB, %.1f%%)" %
          (orig_total / 1048576.0, packed_total / 1048576.0,
           (orig_total - packed_total) / 1048576.0,
           100.0 * (orig_total - packed_total) / orig_total))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("selftest")
    p = sub.add_parser("scan")
    p.add_argument("src")
    p = sub.add_parser("pack")
    p.add_argument("src")
    p.add_argument("out")
    p.add_argument("--verify", action="store_true")
    p.add_argument("--max-file", type=int, default=4, metavar="MB")
    args = ap.parse_args()

    if args.cmd == "selftest":
        selftest()
    elif args.cmd == "scan":
        scan(args.src)
    else:
        pack_tree(args.src, args.out, args.max_file * 1024 * 1024, args.verify)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError) as exc:
        print("erro: %s" % exc, file=sys.stderr)
        sys.exit(1)
