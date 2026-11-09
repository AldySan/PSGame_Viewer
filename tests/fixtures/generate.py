#!/usr/bin/env python3
"""Generate the binary test fixtures for psgcore.

Each fixture is a small synthetic file exercising one viewer path. The
layouts follow the same structures the parsers expect in real files, so
the unit tests can compare exact parsed output. Run from this directory:

    python3 generate.py

No third-party dependencies: the AES-128 used for the PS3 retail and
trophy fixtures is implemented inline (validated against FIPS-197 /
NIST SP 800-38A vectors before use).
"""

import hashlib
import json
import os
import shutil
import struct
import zlib

# ---------------------------------------------------------------- AES-128

SBOX = [
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B,
    0xFE, 0xD7, 0xAB, 0x76, 0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0,
    0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0, 0xB7, 0xFD, 0x93, 0x26,
    0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2,
    0xEB, 0x27, 0xB2, 0x75, 0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0,
    0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84, 0x53, 0xD1, 0x00, 0xED,
    0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F,
    0x50, 0x3C, 0x9F, 0xA8, 0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5,
    0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2, 0xCD, 0x0C, 0x13, 0xEC,
    0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14,
    0xDE, 0x5E, 0x0B, 0xDB, 0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,
    0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79, 0xE7, 0xC8, 0x37, 0x6D,
    0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F,
    0x4B, 0xBD, 0x8B, 0x8A, 0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E,
    0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E, 0xE1, 0xF8, 0x98, 0x11,
    0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F,
    0xB0, 0x54, 0xBB, 0x16,
]
RCON = [0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36]


def _xtime(a):
    a <<= 1
    return (a ^ 0x1B) & 0xFF if a & 0x100 else a


def _mul(a, b):
    p = 0
    for _ in range(8):
        if b & 1:
            p ^= a
        a = _xtime(a)
        b >>= 1
    return p


def aes128_expand(key):
    w = [list(key[i:i + 4]) for i in range(0, 16, 4)]
    for i in range(4, 44):
        t = list(w[i - 1])
        if i % 4 == 0:
            t = t[1:] + t[:1]
            t = [SBOX[x] for x in t]
            t[0] ^= RCON[i // 4]
        w.append([a ^ b for a, b in zip(w[i - 4], t)])
    return w


def aes128_encrypt_block(w, block):
    rk = [b for word in w[0:4] for b in word]
    s = [x ^ k for x, k in zip(block, rk)]
    for rnd in range(1, 11):
        s = [SBOX[x] for x in s]
        for r in range(1, 4):  # shift rows
            row = [s[r + 4 * c] for c in range(4)]
            for c in range(4):
                s[r + 4 * c] = row[(c + r) % 4]
        if rnd != 10:
            for c in range(4):  # mix columns
                a = [s[4 * c + i] for i in range(4)]
                s[4 * c + 0] = _mul(a[0], 2) ^ _mul(a[1], 3) ^ a[2] ^ a[3]
                s[4 * c + 1] = a[0] ^ _mul(a[1], 2) ^ _mul(a[2], 3) ^ a[3]
                s[4 * c + 2] = a[0] ^ a[1] ^ _mul(a[2], 2) ^ _mul(a[3], 3)
                s[4 * c + 3] = _mul(a[0], 3) ^ a[1] ^ a[2] ^ _mul(a[3], 2)
        rk = [b for word in w[rnd * 4:rnd * 4 + 4] for b in word]
        s = [x ^ k for x, k in zip(s, rk)]
    return bytes(s)


def aes128_ecb_encrypt(key, data):
    assert len(key) == 16 and len(data) % 16 == 0
    w = aes128_expand(key)
    return b"".join(aes128_encrypt_block(w, data[i:i + 16])
                    for i in range(0, len(data), 16))


def aes128_cbc_encrypt(key, iv, data):
    assert len(data) % 16 == 0
    chain = bytes(iv)
    out = b""
    for i in range(0, len(data), 16):
        blk = bytes(a ^ b for a, b in zip(data[i:i + 16], chain))
        chain = aes128_ecb_encrypt(key, blk)
        out += chain
    return out


def _selftest_aes():
    ct = aes128_ecb_encrypt(
        bytes.fromhex("000102030405060708090a0b0c0d0e0f"),
        bytes.fromhex("00112233445566778899aabbccddeeff"))
    assert ct.hex() == "69c4e0d86a7b0430d8cdb78070b4c55a", ct.hex()
    ct = aes128_cbc_encrypt(
        bytes.fromhex("2b7e151628aed2a6abf7158809cf4f3c"),
        bytes.fromhex("000102030405060708090a0b0c0d0e0f"),
        bytes.fromhex("6bc1bee22e409f96e93d7e117393172a"))
    assert ct.hex() == "7649abac8119b246cee98e9b12e9197d", ct.hex()


_selftest_aes()

# ---------------------------------------------------------------- helpers

def png_bytes(width, height, rgba):
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    raw = b"".join(b"\x00" + bytes(rgba) * width for _ in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


ICON_PNG = png_bytes(4, 4, (0x4F, 0x8E, 0xF7, 0xFF))
BIG_ICON_PNG = png_bytes(240, 240, (0x21, 0x21, 0x21, 0xFF))


def build_sfo(entries):
    """entries: list of (key, fmt, value). fmt 0x0204 str, 0x0404 u32."""
    count = len(entries)
    key_off = 20 + 16 * count
    keys = b""
    data = b""
    table = b""
    for key, fmt, value in entries:
        key_rel = len(keys)
        keys += key.encode() + b"\x00"
        doff = len(data)
        if fmt == 0x0204:
            raw = value.encode("utf-8")
            dlen = len(raw) + 1
            dmax = (dlen + 3) & ~3
            data += raw + b"\x00" * (dmax - len(raw))
        else:
            dlen = dmax = 4
            data += struct.pack("<I", value)
        table += struct.pack("<HHIII", key_rel, fmt, dlen, dmax, doff)
    data_off = key_off + len(keys)
    return (b"\x00PSF" + struct.pack("<4I", 0x101, key_off, data_off, count)
            + table + keys + data)


def pad_to(data, size):
    return data + b"\x00" * (size - len(data))


# ---------------------------------------------------------------- PS4 CNT

def make_ps4_cnt():
    sfo = build_sfo([
        ("TITLE", 0x0204, "PSGame-Viewer Sample"),
        ("TITLE_01", 0x0204, "PSGame-Viewer Beispiel"),
        ("TITLE_ID", 0x0204, "CUSA90001"),
        ("CONTENT_ID", 0x0204, "UP9000-CUSA90001_00-PSGAMEVIEWER01"),
        ("CATEGORY", 0x0204, "gd"),
        ("VERSION", 0x0204, "01.00"),
        ("APP_VER", 0x0204, "01.00"),
        ("SYSTEM_VER", 0x0404, 0x05050000),
        ("SDK_VER", 0x0404, 0x09050000),
        ("PUBTOOLINFO", 0x0204, "c_date=20260102030405 sdk_ver=09050000"),
    ])
    names = b"param.sfo\x00icon0.png\x00\x00"
    entry_table_off = 0x500
    count = 3
    data_base = entry_table_off + 32 * count
    payloads = [(0x1000, 0, sfo), (0x1200, 10, ICON_PNG), (0x200, 21, names)]

    table = b""
    body = b""
    for ent_id, name_off, payload in payloads:
        table += struct.pack(">8I", ent_id, name_off, 0, 0,
                             data_base + len(body), len(payload), 0, 0)
        body += payload

    out = bytearray(entry_table_off)
    hdr = bytearray(0x80)
    hdr[0:4] = b"\x7fCNT"
    struct.pack_into(">I", hdr, 0x04, 0x81000001)  # finalized + VER_1
    struct.pack_into(">I", hdr, 0x10, count)
    struct.pack_into(">H", hdr, 0x14, 1)  # sys entries
    struct.pack_into(">I", hdr, 0x18, entry_table_off)
    struct.pack_into(">Q", hdr, 0x20, entry_table_off)  # body offset
    cid = b"UP9000-CUSA90001_00-PSGAMEVIEWER01"
    hdr[0x40:0x40 + len(cid)] = cid
    out[0:len(hdr)] = hdr
    out += table
    out += body
    return bytes(out)


# ---------------------------------------------------------------- PS5 FIH

def make_ps5_fih():
    return _make_ps5_fih_like(b"\x7fFIH", 0x58)


def make_ps5_lih():
    """LIH (scene backport) wrapper: same CNT container, but the header
    stores the container offset at 0x30 instead of 0x58."""
    return _make_ps5_fih_like(b"\x7fLIH", 0x30)


def _make_ps5_fih_like(magic, cnt_offset_field):
    param = {
        "contentId": "HP9000-PPSA90001_00-PSGAMEVIEWER01",
        "titleId": "PPSA90001",
        "contentVersion": "01.00",
        "masterVersion": "01.00",
        "conceptId": "10000001",
        "applicationCategoryType": 0,
        "applicationDrmType": "paid",
        "requiredSystemSoftwareVersion": 0x0900 << 48,
        "sdkVersion": 0x0900 << 48,
        "localizedParameters": {
            "defaultLanguage": "en-US",
            "en-US": {"titleName": "PSGame-Viewer PS5 Sample"},
        },
    }
    pj = json.dumps(param, separators=(",", ":")).encode()
    names = b"param.json\x00icon0.png\x00\x00"
    emb = 0x1000
    entry_table_off = 0x180
    count = 3
    # CNT entry offsets are relative to the embedded CNT start (emb)
    data_base = entry_table_off + 32 * count
    payloads = [(0x2000, 0, pj), (0x1200, 11, ICON_PNG), (0x200, 22, names)]

    table = b""
    body = b""
    for ent_id, name_off, payload in payloads:
        table += struct.pack(">8I", ent_id, name_off, 0, 0,
                             data_base + len(body), len(payload), 0, 0)
        body += payload

    chdr = bytearray(0x80)
    chdr[0:4] = b"\x7fCNT"
    struct.pack_into(">I", chdr, 0x04, 0x00000001)  # fake pkg flags
    struct.pack_into(">I", chdr, 0x10, count)
    struct.pack_into(">H", chdr, 0x14, 0)
    struct.pack_into(">I", chdr, 0x18, entry_table_off)
    struct.pack_into(">Q", chdr, 0x20, entry_table_off)
    cid = b"HP9000-PPSA90001_00-PSGAMEVIEWER01"
    chdr[0x40:0x40 + len(cid)] = cid

    fih = bytearray(emb)
    fih[0:4] = magic
    fih[5] = 0x80  # official
    struct.pack_into("<Q", fih, 0x10, emb + data_base)  # pfs offset (abs)
    struct.pack_into("<Q", fih, 0x18, len(body))         # pfs size
    struct.pack_into("<Q", fih, cnt_offset_field, emb)   # CNT offset

    out = fih
    out += chdr
    out += pad_to(b"", entry_table_off - len(chdr))
    out += table
    out += body
    return bytes(out)


# ---------------------------------------------------------------- PS3 PKG

PS3_KEY = bytes.fromhex("2e7b71d7c9c9a14ea3221f188828b8f8")


def ps3_debug_keystream(qa, block_index):
    qa0, qa1 = qa[:8], qa[8:16]
    buf = bytearray(64)
    buf[0:8] = qa0
    buf[8:16] = qa0
    buf[16:24] = qa1
    buf[24:32] = qa1
    buf[56:64] = struct.pack(">Q", block_index)
    return hashlib.sha1(bytes(buf)).digest()[:16]


def ps3_retail_keystream(riv, block_index):
    ctr = (int.from_bytes(riv, "big") + block_index) % (1 << 128)
    return aes128_ecb_encrypt(PS3_KEY, ctr.to_bytes(16, "big"))


def ps3_encrypt(stream_off, retail, keymat, plain):
    """Encrypt `plain` placed at `stream_off` in the data stream. The
    payload is padded to full 16-byte blocks so consecutive files never
    share a keystream block."""
    bs = stream_off & ~0xF
    assert bs == stream_off, "payload must be 16-byte aligned"
    nb = (len(plain) + 15) // 16
    padded = plain + b"\x00" * (nb * 16 - len(plain))
    out = bytearray()
    for i in range(nb):
        ks = (ps3_retail_keystream(keymat, bs // 16 + i) if retail
              else ps3_debug_keystream(keymat, bs // 16 + i))
        out += bytes(a ^ b for a, b in zip(padded[i * 16:(i + 1) * 16], ks))
    return bytes(out)


def make_ps3_pkg(retail):
    sfo = build_sfo([
        ("TITLE", 0x0204, "PSGame-Viewer PS3 Sample"),
        ("TITLE_ID", 0x0204, "BLES90001"),
        ("CONTENT_ID", 0x0204, "EP0002-BLES90001_00-PSGAMEVIEWER01"),
        ("CATEGORY", 0x0204, "HG"),
        ("VERSION", 0x0204, "01.00"),
        ("PS3_SYSTEM_VER", 0x0404, 0x04530000),
    ])
    items = [
        ("PARAM.SFO", sfo, 0x00),
        ("ICON0.PNG", ICON_PNG, 0x00),
        ("USRDIR", b"", 0x04),  # directory entry
        ("USRDIR/DATA.BIN", b"PSGame-Viewer payload " * 8, 0x00),
    ]
    qa = bytes(range(0x10))
    riv = bytes(range(0x10, 0x20))
    keymat = riv if retail else qa

    # stream layout: [table 32n][names, 16-aligned][each file 16-aligned]
    n = len(items)
    name_base = 32 * n
    names = b"".join(nm.encode() + b"\x00" for nm, _, _ in items)
    names_padded = pad_to(names, (len(names) + 15) & ~15)
    pos = name_base + len(names_padded)
    file_off = []
    for nm, payload, flags in items:
        file_off.append(pos)
        pos += (len(payload) + 15) & ~15

    # encrypted stream: table (plaintext offsets 0..32n, 16-aligned for n=4)
    # the item table itself is part of the encrypted data stream
    stream = bytearray()
    table = b""
    for i, (nm, payload, flags) in enumerate(items):
        table += struct.pack(">IIQQII", name_base + sum(
            len(x) + 1 for x, _, _ in items[:i]), len(nm) + 1,
            file_off[i], len(payload), flags, 0)
    stream += ps3_encrypt(0, retail, keymat, table)
    stream += ps3_encrypt(name_base, retail, keymat, names_padded)
    for i, (nm, payload, flags) in enumerate(items):
        if len(payload):
            stream += ps3_encrypt(file_off[i], retail, keymat, payload)

    hdr = bytearray(128)
    hdr[0:4] = b"\x7fPKG"
    struct.pack_into(">H", hdr, 4, 0x8000 if retail else 0x0000)
    struct.pack_into(">H", hdr, 6, 1)  # PS3 type
    struct.pack_into(">I", hdr, 0x10, 128)  # header size
    struct.pack_into(">I", hdr, 0x14, n)    # item count
    struct.pack_into(">Q", hdr, 0x18, 128 + len(stream))
    struct.pack_into(">Q", hdr, 0x20, 128)  # data offset
    struct.pack_into(">Q", hdr, 0x28, len(stream))  # data size
    cid = b"EP0002-BLES90001_00-PSGAMEVIEWER01"
    hdr[0x30:0x30 + len(cid)] = cid
    hdr[0x60:0x70] = qa
    hdr[0x70:0x80] = riv
    return bytes(hdr + stream)


# ---------------------------------------------------------------- ISOs

SECTOR = 2048


def iso_pvd(root_lba, root_size, volume_id):
    pvd = bytearray(SECTOR)
    pvd[0] = 1
    pvd[1:6] = b"CD001"
    pvd[6] = 1  # version
    pvd[40:72] = volume_id.encode().ljust(32)
    rec = bytearray(34)
    rec[0] = 34
    struct.pack_into("<I", rec, 2, root_lba)
    struct.pack_into(">I", rec, 6, root_lba)
    struct.pack_into("<I", rec, 10, root_size)
    struct.pack_into(">I", rec, 14, root_size)
    rec[25] = 0x02
    rec[32] = 1
    pvd[156:190] = rec
    return bytes(pvd)


def iso_dir_record(name, lba, size, is_dir, seq=1):
    ident = name.encode()
    body = b"\x00"  # extended attribute record length
    body += struct.pack("<I", lba) + struct.pack(">I", lba)
    body += struct.pack("<I", size) + struct.pack(">I", size)
    body += b"\x00" * 7  # recording time
    body += bytes([0x02 if is_dir else 0x00])
    body += b"\x00\x00"
    body += struct.pack("<H", seq) + struct.pack(">H", seq)
    body += bytes([len(ident)]) + ident
    if len(body) % 2:
        body += b"\x00"
    return bytes([len(body) + 1]) + body


def iso_dot_record(lba, size):
    rec = bytearray(34)
    rec[0] = 34
    rec[1] = 0  # extended attribute record length
    struct.pack_into("<I", rec, 2, lba)
    struct.pack_into(">I", rec, 6, lba)
    struct.pack_into("<I", rec, 10, size)
    struct.pack_into(">I", rec, 14, size)
    rec[25] = 0x02
    rec[32] = 1
    return bytes(rec)


def make_iso_image(volume_id, tree):
    """tree: list of (path, is_dir, payload). Paths use '/'; directories
    carry None. Sector 16 = PVD, 17 = terminator, then directories
    (deepest first), then file payloads in declaration order."""
    files = [(p, payload) for p, d, payload in tree if not d]
    dirs = [p for p, d, _ in tree if d]

    next_lba = 18
    dir_lba = {}
    for path in sorted(dirs, key=lambda t: -t.count("/")):
        dir_lba[path] = next_lba
        next_lba += 1
    root_lba = next_lba
    next_lba += 1
    file_lba = {}
    for path, _ in files:
        file_lba[path] = next_lba
        next_lba += 1

    def parent_of(path):
        return path.rsplit("/", 1)[0] if "/" in path else ""

    sectors = {}
    all_dir_paths = dirs + [""]
    for path in all_dir_paths:
        lba = dir_lba.get(path, root_lba)
        parent_lba = dir_lba.get(parent_of(path), root_lba)
        children = []
        for p, is_dir, payload in tree:
            if parent_of(p) != path:
                continue
            nm = p.rsplit("/", 1)[-1]
            if is_dir:
                children.append(iso_dir_record(nm, dir_lba[p], SECTOR, True))
            else:
                children.append(iso_dir_record(nm, file_lba[p],
                                               len(payload), False))
        recs = (iso_dot_record(lba, SECTOR)
                + iso_dot_record(parent_lba, SECTOR)
                + b"".join(children))
        sectors[lba] = pad_to(recs, SECTOR)

    term = bytearray(SECTOR)
    term[0] = 255
    term[1:6] = b"CD001"
    # file payloads live at their allocated sectors
    for path, payload in files:
        sectors[file_lba[path]] = pad_to(payload, SECTOR)
    out = bytearray(b"\x00" * (16 * SECTOR))
    out += iso_pvd(root_lba, SECTOR, volume_id)
    out += term
    for lba in range(18, next_lba):
        if lba in sectors:
            out += sectors[lba]
        else:
            out += b"\x00" * SECTOR
    return bytes(out)


def make_ps2_iso():
    cnf = ("BOOT2 = cdrom0:\\SLUS_99999;1\n"
           "VER = 1.00\n"
           "VMODE = NTSC\n").encode()
    return make_iso_image("PSGAMEVIEWER", [
        ("SYSTEM.CNF", False, cnf),
        ("SLUS_99999", False, b"BOOT ELF PAYLOAD"),
    ])


def make_ps3_iso():
    sfo = build_sfo([
        ("TITLE", 0x0204, "PSGame-Viewer PS3 Disc"),
        ("TITLE_ID", 0x0204, "BCES90001"),
        ("CATEGORY", 0x0204, "DG"),
        ("VERSION", 0x0204, "01.00"),
        ("APP_VER", 0x0204, "01.00"),
        ("PS3_SYSTEM_VER", 0x0404, 0x03550000),
        ("PARENTAL_LEVEL", 0x0404, 3),
        ("RESOLUTION", 0x0404, 63),
        ("SOUND_FORMAT", 0x0404, 1),
    ])
    return make_iso_image("PSGAMEVIEWER", [
        ("PS3_DISC.SFB", False, b"\x00SFB" + b"\x00" * 28),
        ("PS3_GAME", True, None),
        ("PS3_GAME/PARAM.SFO", False, sfo),
        ("PS3_GAME/ICON0.PNG", False, ICON_PNG),
        ("PS3_GAME/USRDIR", True, None),
        ("PS3_GAME/USRDIR/EBOOT.BIN", False, b"EBOOT PAYLOAD"),
        ("PS3_UPDATE", True, None),
        ("PS3_UPDATE/PS3UPDAT.PUP", False, b"PUP!"),
    ])


# ---------------------------------------------------------------- exFAT

def make_exfat():
    bps = 512          # 1 << 9
    spc = 2            # 1 << 1 -> cluster = 1024 bytes
    fat_lba = 128
    data_lba = 256
    root_cluster = 2
    clus_bytes = bps * spc

    param_json = json.dumps({
        "contentId": "UP9000-PPSA90002_00-PSGAMEVIEWER02",
        "titleId": "PPSA90002",
        "contentVersion": "01.00",
        "masterVersion": "01.00",
        "conceptId": "10000002",
        "applicationCategoryType": 0,
        "applicationDrmType": "paid",
        "requiredSystemSoftwareVersion": 0x0900 << 48,
        "sdkVersion": 0x0900 << 48,
        "localizedParameters": {
            "defaultLanguage": "en-US",
            "en-US": {"titleName": "PSGame-Viewer PS5 Image"},
        },
    }, separators=(",", ":")).encode()

    def name_entries(name):
        u16 = name.encode("utf-16-le")
        while len(u16) % 30:
            u16 += b"\x00\x00"
        out = b""
        for i in range(0, len(u16), 30):
            ent = bytearray(32)
            ent[0] = 0xC1
            ent[1] = i // 30 + 1
            ent[2:32] = u16[i:i + 30]
            out += bytes(ent)
        return out

    def file_set(name, first_cluster, size, is_dir=False):
        ent85 = bytearray(32)
        ent85[0] = 0x85
        ent85[1] = 2  # secondary count: stream + one name entry
        struct.pack_into("<H", ent85, 4, 0x10 if is_dir else 0x20)
        entc0 = bytearray(32)
        entc0[0] = 0xC0
        struct.pack_into("<H", entc0, 2, 0x03)  # nofat flag set
        struct.pack_into("<I", entc0, 20, first_cluster)
        struct.pack_into("<Q", entc0, 24, size)
        return bytes(ent85) + bytes(entc0) + name_entries(name)

    root_dir = pad_to(file_set("sce_sys", 3, clus_bytes, True), clus_bytes)
    scesys_dir = pad_to(file_set("param.json", 4, len(param_json))
                        + file_set("icon0.png", 5, len(ICON_PNG)), clus_bytes)

    boot = bytearray(bps)
    boot[0:3] = b"\xEB\x76\x90"
    boot[3:11] = b"EXFAT   "
    struct.pack_into("<I", boot, 80, fat_lba)
    struct.pack_into("<I", boot, 88, data_lba)
    struct.pack_into("<I", boot, 96, root_cluster)
    boot[108] = 9  # 512 bytes/sector
    boot[109] = 1  # 2 sectors/cluster
    boot[510] = 0x55
    boot[511] = 0xAA

    out = bytearray(boot)
    out += b"\x00" * (fat_lba * bps - len(out))
    fat = bytearray()
    for c in range(0, 8):
        fat += struct.pack("<I", 0xFFFFFFFF if 2 <= c <= 5 else 0x00000000)
    out += fat
    out += b"\x00" * (data_lba * bps - len(out))
    base = len(out)
    out += root_dir          # cluster 2
    out += scesys_dir        # cluster 3
    out += pad_to(param_json, clus_bytes)  # cluster 4
    out += pad_to(ICON_PNG, clus_bytes)    # cluster 5
    return bytes(out)


# ---------------------------------------------------------------- TRP

TRP_TROPHY_KEY = bytes([0x21, 0xF4, 0x1A, 0x6B, 0xAD, 0x8A, 0x1D, 0x3E,
                        0xCA, 0x7A, 0xD5, 0x86, 0xC1, 0x01, 0xB7, 0xA9])


def make_trp():
    """PS3 TROPHY.TRP in the real retail container layout (cross-checked
    against RPCS3's TRP.h, psdevwiki and three retail-file extractors):
    0x40-byte header — magic, u32 version, u64 total size, u32 count,
    u32 entry size (0x40), u32 dev flag, sha1[20], pad[16] — then count
    entries of 0x40 bytes: name[32], u64 offset @+0x20, u64 size @+0x28.

    TROPCONF.SFM is stored as PLAIN XML: the PS3 does not encrypt the
    conf — only the PS4 TROP.ESFM variant is AES-wrapped. (The 0.0.4
    loader tried to AES-decrypt every conf and real retail packs came
    up empty, so this fixture is the regression.) The trophy set mixes
    an English platinum, a HIDDEN Japanese bronze and an English gold
    so UTF-8 names, grades and the hidden flag are all covered. The
    pack mixes a 240x240 icon (PS4/PS5 style), two 128x128 icons (PS3
    style) and a 4x4 one to exercise every carving path."""
    conf = (
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<trophyconf>"
        "<trophy id=\"0\" ttype=\"P\" hidden=\"no\">"
        "<name>First Steps</name>"
        "<detail>Open the viewer</detail>"
        "</trophy>"
        "<trophy id=\"1\" ttype=\"B\" hidden=\"yes\">"
        "<name>戦国の初陣</name>"
        "<detail>最初の戦いを勝利せよ</detail>"
        "</trophy>"
        "<trophy id=\"2\" ttype=\"G\" hidden=\"no\">"
        "<name>Collector</name>"
        "<detail>View every platform</detail>"
        "</trophy>"
        "</trophyconf>").encode("utf-8")
    files = [
        ("TROPCONF.SFM", conf),
        ("ICON0.PNG", BIG_ICON_PNG),                                # 240x240
        ("ICON1.PNG", ICON_PNG),                                    # 4x4
        ("TROP001.PNG", png_bytes(128, 128, (0xF7, 0xD7, 0x4A, 0xFF))),
        ("TROP002.PNG", png_bytes(128, 128, (0xC0, 0xC0, 0xC0, 0xFF))),
    ]
    return build_trp_container(files)


def make_trp_esfm():
    """PS4-style trophy pack: TROP.ESFM is AES-CBC encrypted with the
    per-title key (AES-CBC of the NP communication id under the public
    trophy key), IV = first 16 bytes, PKCS#7 padding — keeps the
    decrypt and NP-id brute-force paths under test."""
    npid16 = b"NPWR99999_00".ljust(16, b"\x00")
    ckey = aes128_cbc_encrypt(TRP_TROPHY_KEY, bytes(16), npid16)
    xml = (b"<trophyconf><trophy id='0' ttype='P' hidden='no'>"
           b"<name>Platinum Run</name>"
           b"<detail>Unlock everything</detail>"
           b"</trophy>"
           b"<trophy id='1' ttype='S' hidden='no'>"
           b"<name>Silver Lining</name>"
           b"<detail>Almost there</detail>"
           b"</trophy></trophyconf>")
    pad = 16 - len(xml) % 16
    iv = bytes(range(0x40, 0x50))
    esfm = iv + aes128_cbc_encrypt(ckey, iv, xml + bytes([pad]) * pad)
    return build_trp_container([
        ("TROP.ESFM", esfm),
        ("ICON0.PNG", png_bytes(240, 240, (0x33, 0x99, 0xCC, 0xFF))),
    ])


def build_trp_container(files):
    """Assemble the retail TRP container: 0x40-byte header + one 0x40
    entry record per file + the payloads."""
    header_size = 0x40
    entry_size = 0x40
    body = bytearray()
    recs = b""
    for name, payload in files:
        off = header_size + entry_size * len(files) + len(body)
        rec = bytearray(entry_size)
        rec[0:len(name)] = name.encode()
        struct.pack_into(">Q", rec, 0x20, off)
        struct.pack_into(">Q", rec, 0x28, len(payload))
        recs += bytes(rec)
        body += payload
    total = header_size + entry_size * len(files) + len(body)
    hdr = bytearray(header_size)
    hdr[0:4] = b"\xDC\xA2\x4D\x00"
    struct.pack_into(">I", hdr, 0x04, 1)            # version
    struct.pack_into(">Q", hdr, 0x08, total)        # total file size
    struct.pack_into(">I", hdr, 0x10, len(files))   # entry count
    struct.pack_into(">I", hdr, 0x14, entry_size)   # entry size
    # dev flag @0x18, sha1[20] @0x1C and pad[16] @0x30 stay zero
    return bytes(hdr + recs + body)


# ---------------------------------------------------------------- UCP

def make_ucp():
    """PS5 trophy00.ucp: a tropmeta JSON plus trophy_1.png..trophy_12.png.
    The icon names are deliberately NOT zero-padded, so the display order
    needs a natural sort (trophy_2.png before trophy_10.png). Each icon
    carries a distinct amount of trailing padding so the order is
    observable from the blob sizes."""
    trophies = []
    files = []
    for i in range(1, 13):
        trophies.append({"id": str(i), "name": "Trophy %d" % i,
                         "detail": "Do thing %d" % i})
        files.append(("trophy_%d.png" % i,
                      png_bytes(4, 4, (i * 8, 0x8E, 0xF7, 0xFF))
                      + b"\x00" * i))
    meta = {"trophyNpCommId": "NPWR90000_00",
            "metadata": {"titleMetadata":
                         {"name": "PSGame-Viewer UCP Sample"},
                         "trophyMetadata": trophies}}
    files.insert(0, ("tropmeta_en-US.json", json.dumps(meta).encode()))

    header_size = 0x40
    rec_size = 64
    body = bytearray()
    recs = b""
    for name, payload in files:
        off = header_size + rec_size * len(files) + len(body)
        rec = bytearray(rec_size)
        struct.pack_into(">I", rec, 4, off)
        struct.pack_into(">I", rec, 12, len(payload))
        rec[32:32 + len(name)] = name.encode()
        recs += bytes(rec)
        body += payload
    hdr = bytearray(header_size)
    hdr[0:4] = b"\xB2\x28\xC6\x0A"
    return bytes(hdr + recs + body)


# ---------------------------------------------------------------- ffpfsc / ffpkg

PFSC_BLOCK = 0x10000


def make_exfat_trophy(pack_size=None):
    """exFAT image like make_exfat(), plus sce_sys/trophy/trophy00.trp so
    .ffpfsc fixtures carry a lazily-read trophy pack entry. `pack_size`
    pads the pack out with zeros (exceeds the old 5 MB read cap)."""
    bps = 512
    spc = 2
    fat_lba = 128
    data_lba = 256
    root_cluster = 2
    clus_bytes = bps * spc

    param_json = json.dumps({
        "contentId": "UP9000-PPSA90002_00-PSGAMEVIEWER02",
        "titleId": "PPSA90002",
        "contentVersion": "01.00",
        "masterVersion": "01.00",
        "conceptId": "10000002",
        "applicationCategoryType": 0,
        "applicationDrmType": "paid",
        "requiredSystemSoftwareVersion": 0x0900 << 48,
        "sdkVersion": 0x0900 << 48,
        "localizedParameters": {
            "defaultLanguage": "en-US",
            "en-US": {"titleName": "PSGame-Viewer PS5 Image"},
        },
    }, separators=(",", ":")).encode()
    trp = make_trp()
    if pack_size is not None and pack_size > len(trp):
        trp = trp + b"\x00" * (pack_size - len(trp))

    def name_entries(name):
        u16 = name.encode("utf-16-le")
        while len(u16) % 30:
            u16 += b"\x00\x00"
        out = b""
        for i in range(0, len(u16), 30):
            ent = bytearray(32)
            ent[0] = 0xC1
            ent[1] = i // 30 + 1
            ent[2:32] = u16[i:i + 30]
            out += bytes(ent)
        return out

    def file_set(name, first_cluster, size, is_dir=False):
        ent85 = bytearray(32)
        ent85[0] = 0x85
        ent85[1] = 2
        struct.pack_into("<H", ent85, 4, 0x10 if is_dir else 0x20)
        entc0 = bytearray(32)
        entc0[0] = 0xC0
        struct.pack_into("<H", entc0, 2, 0x03)  # nofat
        struct.pack_into("<I", entc0, 20, first_cluster)
        struct.pack_into("<Q", entc0, 24, size)
        return bytes(ent85) + bytes(entc0) + name_entries(name)

    root_dir = pad_to(file_set("sce_sys", 3, clus_bytes, True), clus_bytes)
    scesys_dir = pad_to(file_set("param.json", 4, len(param_json))
                        + file_set("icon0.png", 5, len(ICON_PNG))
                        + file_set("trophy", 6, clus_bytes, True), clus_bytes)
    trophy_dir = pad_to(file_set("trophy00.trp", 7, len(trp)), clus_bytes)

    boot = bytearray(bps)
    boot[0:3] = b"\xEB\x76\x90"
    boot[3:11] = b"EXFAT   "
    struct.pack_into("<I", boot, 80, fat_lba)
    struct.pack_into("<I", boot, 88, data_lba)
    struct.pack_into("<I", boot, 96, root_cluster)
    boot[108] = 9  # 512 bytes/sector
    boot[109] = 1  # 2 sectors/cluster
    boot[510] = 0x55
    boot[511] = 0xAA

    out = bytearray(boot)
    out += b"\x00" * (fat_lba * bps - len(out))
    fat = bytearray()
    for c in range(0, 8):
        fat += struct.pack("<I", 0xFFFFFFFF if 2 <= c <= 7 else 0x00000000)
    out += fat
    out += b"\x00" * (data_lba * bps - len(out))
    out += root_dir          # cluster 2
    out += scesys_dir         # cluster 3
    out += pad_to(param_json, clus_bytes)  # cluster 4
    out += pad_to(ICON_PNG, clus_bytes)    # cluster 5
    out += trophy_dir                       # cluster 6
    out += pad_to(trp, len(trp) + (clus_bytes - len(trp) % clus_bytes) % clus_bytes)  # cluster 7..
    return bytes(out)


def make_ufs2():
    """Minimal UFS2 image (FreeBSD layout): root inode 2 with sce_sys/
    param.json + icon0.png + icon1.png. 256 KiB, one cylinder group."""
    bsize = 4096
    fsize = 512
    frag = 8
    inopb = bsize // 0x100        # 16 inodes per block
    ipg = 32
    fpg = 1024                    # frags per group (whole image)
    image_blocks = 64             # 262144 bytes

    param_json = json.dumps({
        "contentId": "HP9000-PPSA90003_00-PSGAMEVIEWER03",
        "titleId": "PPSA90003",
        "contentVersion": "01.00",
        "masterVersion": "01.00",
        "conceptId": "10000003",
        "applicationCategoryType": 0,
        "applicationDrmType": "paid",
        "requiredSystemSoftwareVersion": 0x0900 << 48,
        "sdkVersion": 0x0900 << 48,
        "localizedParameters": {
            "defaultLanguage": "en-US",
            "en-US": {"titleName": "PSGame-Viewer PS5 Image"},
        },
    }, separators=(",", ":")).encode()

    # block addresses (frag units): root 184, sce_sys 192, files 200..
    root_blk, scesys_blk = 184, 192
    file_blocks = {"param.json": 200, "icon0.png": 208, "icon1.png": 216}

    def dirent(ino, dtype, name):
        namelen = len(name)
        reclen = (8 + namelen + 1 + 3) & ~3  # DIRECTSIZ rounded to 4
        out = struct.pack("<IHBB", ino, reclen, dtype, namelen)
        out += name.encode() + b"\x00" * (reclen - 8 - namelen)
        return out

    DT_DIR, DT_REG = 4, 8
    root_dir = (dirent(2, DT_DIR, ".") + dirent(2, DT_DIR, "..")
                + dirent(3, DT_DIR, "sce_sys"))
    scesys_dir = (dirent(3, DT_DIR, ".") + dirent(2, DT_DIR, "..")
                  + dirent(4, DT_REG, "param.json")
                  + dirent(5, DT_REG, "icon0.png")
                  + dirent(6, DT_REG, "icon1.png"))

    def dinode(mode, size, db0):
        out = bytearray(0x100)
        struct.pack_into("<H", out, 0x00, mode)
        struct.pack_into("<Q", out, 0x10, size)
        struct.pack_into("<Q", out, 0x70, db0)  # di_db[0] (frag address)
        return bytes(out)

    inodes = {}
    inodes[2] = dinode(0o040755, bsize, root_blk)        # root dir
    inodes[3] = dinode(0o040755, bsize, scesys_blk)      # sce_sys
    inodes[4] = dinode(0o100644, len(param_json), file_blocks["param.json"])
    inodes[5] = dinode(0o100644, len(ICON_PNG), file_blocks["icon0.png"])
    inodes[6] = dinode(0o100644, len(ICON_PNG), file_blocks["icon1.png"])

    img = bytearray(b"\x00" * (image_blocks * bsize))
    # superblock at 64 KiB
    sb = bytearray(0x560)
    struct.pack_into("<i", sb, 0x08, 144)   # fs_sblkno
    struct.pack_into("<i", sb, 0x0C, 160)   # fs_cblkno
    struct.pack_into("<i", sb, 0x10, 168)   # fs_iblkno
    struct.pack_into("<i", sb, 0x14, 184)   # fs_dblkno
    struct.pack_into("<I", sb, 0x2C, 1)     # fs_ncg
    struct.pack_into("<i", sb, 0x30, bsize)
    struct.pack_into("<i", sb, 0x34, fsize)
    struct.pack_into("<i", sb, 0x38, frag)
    struct.pack_into("<I", sb, 0x78, inopb)
    struct.pack_into("<Q", sb, 0x3E8, 65536)  # fs_sblockloc
    struct.pack_into("<Q", sb, 0x438, image_blocks)  # fs_size (blocks)
    struct.pack_into("<I", sb, 0xB8, ipg)   # fs_ipg
    struct.pack_into("<i", sb, 0xBC, fpg)   # fs_fpg
    struct.pack_into("<I", sb, 0x55C, 0x19540119)  # FS_UFS2_MAGIC
    img[65536:65536 + len(sb)] = sb

    # inode table at frag 168 (byte 86016), slots of 256 bytes
    ino_base = 168 * fsize
    for num, blob in inodes.items():
        img[ino_base + num * 0x100:ino_base + num * 0x100 + 0x100] = blob

    # directory and file blocks (frag addresses * fsize)
    img[root_blk * fsize:root_blk * fsize + bsize] = pad_to(root_dir, bsize)
    img[scesys_blk * fsize:scesys_blk * fsize + bsize] = pad_to(scesys_dir, bsize)
    img[file_blocks["param.json"] * fsize:
        file_blocks["param.json"] * fsize + bsize] = pad_to(param_json, bsize)
    img[file_blocks["icon0.png"] * fsize:
        file_blocks["icon0.png"] * fsize + bsize] = pad_to(ICON_PNG, bsize)
    img[file_blocks["icon1.png"] * fsize:
        file_blocks["icon1.png"] * fsize + bsize] = pad_to(ICON_PNG, bsize)
    return bytes(img)


def pfsc_encode(inner):
    """inner bytes -> (stored PFSC payload, stored_blocks, logical_len)."""
    blocks = -(-len(inner) // PFSC_BLOCK)
    logical = blocks * PFSC_BLOCK
    padded = inner + b"\x00" * (logical - len(inner))
    stored_list = []
    for i in range(blocks):
        blk = padded[i * PFSC_BLOCK:(i + 1) * PFSC_BLOCK]
        comp = zlib.compress(blk, 6)
        stored_list.append(comp if len(comp) < PFSC_BLOCK else blk)
    data_off = 0x10000
    offsets = [data_off]
    for s in stored_list:
        offsets.append(offsets[-1] + len(s))
    header = bytearray(data_off)
    struct.pack_into("<iiiiqqQq", header, 0, 0x43534650, 0, 6,
                     PFSC_BLOCK, PFSC_BLOCK, 0x400, data_off, logical)
    struct.pack_into("<%dQ" % len(offsets), header, 0x400, *offsets)
    payload = bytes(header) + b"".join(stored_list)
    payload += b"\x00" * ((-len(payload)) % PFSC_BLOCK)
    return payload, len(payload) // PFSC_BLOCK, logical


def fpt_hash(s):
    """mkpfs flat_path_table hash (case-sensitive Java-style 31 hash)."""
    h = 0
    for ch in s:
        h = (ord(ch) + 31 * h) & 0xFFFFFFFF
    return h


def make_ffpfsc(inner_name, inner, compress=True):
    """PFS image in the reference mkpfs shape: root holds flat_path_table
    (internal 8-byte file) and uroot; uroot holds '.', '..' and the single
    inner file, optionally PFSC block-compressed."""
    bs = 0x10000
    blocks = -(-len(inner) // bs)
    logical = blocks * bs
    if compress:
        payload, stored_blocks, logical = pfsc_encode(inner)
        flags = 0x11           # COMPRESSED | READONLY
        size_field = len(inner)          # di_size
        logical_field = logical          # di_size_compressed (mkpfs semantics)
    else:
        payload = inner + b"\x00" * (logical - len(inner))
        payload += b"\x00" * ((-len(payload)) % bs)
        stored_blocks = len(payload) // bs
        flags = 0x10           # READONLY
        size_field = len(inner)
        logical_field = len(inner)

    header_blk, ino_blk, root_blk, fpt_blk, uroot_blk = 0, 1, 2, 3, 4
    payload_blk = 5
    total_blocks = payload_blk + stored_blocks

    def dirent(ino, dtype, name):
        namelen = len(name)
        entsize = (16 + namelen + 1 + 7) & ~7   # mkpfs: >= 17, 8-aligned
        out = struct.pack("<Iiii", ino, dtype, namelen, entsize)
        out += name.encode() + b"\x00" * (entsize - 16 - namelen)
        return out

    F, D, DOT, DOTDOT = 2, 3, 4, 5
    # root dirents: flat_path_table + uroot (no dot entries, like mkpfs)
    root_dir = (dirent(1, F, "flat_path_table")
                + dirent(2, D, "uroot"))
    # uroot dirents: '.', '..', inner file (".." points at uroot itself,
    # like the reference images)
    uroot_dir = (dirent(2, DOT, ".") + dirent(2, DOTDOT, "..")
                 + dirent(3, F, inner_name))
    # flat_path_table payload: (hash("/name"), file inode) pairs
    fpt_data = struct.pack("<II", fpt_hash("/" + inner_name), 3)

    def dinode(mode, flags_, size, logical, blocks_, db0):
        out = bytearray(0xA8)
        struct.pack_into("<H", out, 0x00, mode)
        struct.pack_into("<I", out, 0x04, flags_)
        struct.pack_into("<Q", out, 0x08, size)
        struct.pack_into("<Q", out, 0x10, logical)
        struct.pack_into("<I", out, 0x60, blocks_)
        struct.pack_into("<i", out, 0x64, db0)
        for i in range(1, 12):
            struct.pack_into("<i", out, 0x64 + 4 * i, -1)
        return bytes(out)

    inodes = b"".join([
        dinode(0x416D, 0x20010, bs, bs, 1, root_blk),   # 0: root dir
        dinode(0x816D, 0x20010, 8, 8, 1, fpt_blk),      # 1: flat_path_table
        dinode(0x416D, 0x10, bs, bs, 1, uroot_blk),     # 2: uroot
        dinode(0x816D, flags, size_field, logical_field,
               stored_blocks, payload_blk),              # 3: inner file
    ])

    hdr = bytearray(bs)
    struct.pack_into("<q", hdr, 0x00, 2)            # version (PS5)
    struct.pack_into("<q", hdr, 0x08, 20130315)     # magic
    struct.pack_into("<H", hdr, 0x1C, 0)            # mode
    struct.pack_into("<I", hdr, 0x20, bs)           # block size
    struct.pack_into("<q", hdr, 0x28, 1)            # nblock
    struct.pack_into("<q", hdr, 0x30, 4)            # inode count
    struct.pack_into("<q", hdr, 0x38, total_blocks) # ndblock
    struct.pack_into("<q", hdr, 0x40, 1)            # inode block count

    img = bytearray(b"\x00" * (total_blocks * bs))
    img[0:bs] = hdr
    img[bs:bs + len(inodes)] = inodes
    img[2 * bs:2 * bs + len(root_dir)] = root_dir
    img[3 * bs:3 * bs + len(fpt_data)] = fpt_data
    img[4 * bs:4 * bs + len(uroot_dir)] = uroot_dir
    img[5 * bs:5 * bs + len(payload)] = payload
    return bytes(img)


# ---------------------------------------------------------------- main

def write_file(rel_dir, name, data):
    os.makedirs(rel_dir, exist_ok=True)
    with open(os.path.join(rel_dir, name), "wb") as f:
        f.write(data)


def make_ps3_folder(out_dir):
    """PS3 JB folder fixture in the exact on-disk layout: PS3_DISC.SFB
    at the root, PS3_GAME/{PARAM.SFO, ICON0.PNG, PIC1.PNG, LICDIR,
    TROPDIR/NPWR07867_00/TROPHY.TRP, USRDIR/...}. Regression case for
    the folder viewer: the WHOLE tree must be indexed (0.0.4 listed
    only the PNGs and the pack) and the plain-XML retail conf with
    Japanese names must load into the Trophies tab."""
    root = os.path.join(out_dir, "ps3_folder")
    shutil.rmtree(root, ignore_errors=True)
    game = os.path.join(root, "PS3_GAME")
    sfo = build_sfo([
        ("APP_VER", 0x0204, "01.00"),
        ("CATEGORY", 0x0204, "GD"),
        ("CONTENT_ID", 0x0204, "JM0000-BLJM61080_00-SBASARA4000000"),
        ("PARENTAL_LEVEL", 0x0404, 17),
        ("PS3_SYSTEM_VER", 0x0404, 0x04530000),
        ("RESOLUTION", 0x0404, 63),
        ("SOUND_FORMAT", 0x0404, 155),
        ("TITLE", 0x0204, "戦国BASARA4"),
        ("TITLE_ID", 0x0204, "BLJM61080"),
        ("VERSION", 0x0204, "01.00"),
    ])
    write_file(root, "PS3_DISC.SFB", b"\x00SFB" + bytes(500))
    write_file(game, "PARAM.SFO", sfo)
    write_file(game, "ICON0.PNG", BIG_ICON_PNG)
    write_file(game, "PIC1.PNG", ICON_PNG)
    write_file(os.path.join(game, "LICDIR"), "LIC.DAT",
               b"\x00LIC" + bytes(64))
    write_file(os.path.join(game, "TROPDIR", "NPWR07867_00"),
               "TROPHY.TRP", make_trp())
    write_file(os.path.join(game, "USRDIR"), "EBOOT.BIN",
               b"\x00EBOOT" + bytes(4000))
    write_file(os.path.join(game, "USRDIR", "nativePS3", "rom"),
               "data.bin", b"\x01" * 512)
    return root


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    exfat_trophy = make_exfat_trophy()
    ufs2 = make_ufs2()
    fixtures = {
        "ps4_cnt.pkg": make_ps4_cnt(),
        "ps5_fih.pkg": make_ps5_fih(),
        "ps5_lih.pkg": make_ps5_lih(),
        "ps3_debug.pkg": make_ps3_pkg(retail=False),
        "ps3_retail.pkg": make_ps3_pkg(retail=True),
        "ps2.iso": make_ps2_iso(),
        "ps3.iso": make_ps3_iso(),
        "ps5.exfat": make_exfat(),
        "trophy.trp": make_trp(),
        "trophy_esfm.trp": make_trp_esfm(),
        "trophy.ucp": make_ucp(),
        # regression fixture for the exFAT read cap: a >5 MB trophy pack
        # must still be readable end to end
        "ps5_bigpack.exfat": make_exfat_trophy(6 << 20),
        "ffpkg.ffpkg": ufs2,
        "ffpfsc_exfat.ffpfsc": make_ffpfsc("LOOP00002.exfat", exfat_trophy,
                                          compress=True),
        "ffpfsc_raw.ffpfsc": make_ffpfsc("LOOP00002.exfat", exfat_trophy,
                                         compress=False),
        "ffpfsc_ffpkg.ffpfsc": make_ffpfsc("PPSA90003-app.ffpkg", ufs2,
                                           compress=True),
    }
    for name, data in fixtures.items():
        with open(os.path.join(here, name), "wb") as f:
            f.write(data)
        print(f"wrote {name} ({len(data)} bytes)")
    make_ps3_folder(here)
    print("wrote ps3_folder/ (PS3 JB folder fixture)")


if __name__ == "__main__":
    main()
