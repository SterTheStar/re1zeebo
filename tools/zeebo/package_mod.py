#!/usr/bin/env python3
"""package_mod.py - Prepares an unpacked Zeebo module directory containing
re1zeebo.mod, re1zeebo.mif, and RE1 PC assets (assets/USA/) for zeebx-emu.
"""
import argparse
import os
import shutil
import struct


def build_minimal_mif(clsid: int, app_name: str) -> bytes:
    """Build a valid BREW .mif matching zeebx-core's MifFile::parse layout."""
    _ = app_name
    # 0x00..0x20: MIF header (magic=0x0011, version=1, 1 index entry, 1 section)
    header = struct.pack(
        "<HHHHIIIIII",
        0x0011,  # magic
        1,       # version
        1,       # reserved/always 1
        1,       # index entries
        0x20,    # index offset
        8,       # index size (1 * 8)
        0x28,    # section table offset
        1,       # number of sections
        0x30,    # offset of first section
        20,      # total size of sections
    )
    index_entry = struct.pack("<II", 0, 0)
    section_table = struct.pack("<II", 0x30, 0x44)
    # 20-byte applet record: ClassID, 0, flags/num, 0, flags2
    applet_record = struct.pack("<IIIII", clsid, 0, 1, 0, 0)
    return header + index_entry + section_table + applet_record


def main() -> None:
    parser = argparse.ArgumentParser(description="Stage RE1 Zeebo .mod + assets into a folder for zeebx-emu")
    parser.add_argument("--mod", required=True, help="Path to compiled re1zeebo.mod")
    parser.add_argument("--assets", default="", help="Path to GOG/RE1 USA folder (e.g. C:\\GOG Games\\Resident Evil\\USA)")
    parser.add_argument("--out-dir", default="dist/zeebo/re1zeebo", help="Output module directory")
    parser.add_argument("--clsid", default="0x01099001", help="Applet CLSID hex")
    args = parser.parse_args()

    out_dir = os.path.abspath(args.out_dir)
    os.makedirs(out_dir, exist_ok=True)

    clsid = int(args.clsid, 16)
    mod_dst = os.path.join(out_dir, "re1zeebo.mod")
    mif_dst = os.path.join(out_dir, "re1zeebo.mif")

    shutil.copy2(args.mod, mod_dst)
    with open(mif_dst, "wb") as f:
        f.write(build_minimal_mif(clsid, "Resident Evil"))

    copied = 0
    if args.assets and os.path.isdir(args.assets):
        assets_dst = os.path.join(out_dir, "USA")
        shutil.copytree(args.assets, assets_dst, dirs_exist_ok=True)
        for _, _, files in os.walk(assets_dst):
            copied += len(files)

    print(f"Prepared {out_dir} (mod={mod_dst}, mif={mif_dst}, assets={copied} files, clsid=0x{clsid:08X})")


if __name__ == "__main__":
    main()
