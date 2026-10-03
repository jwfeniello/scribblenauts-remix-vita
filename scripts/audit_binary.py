"""Read-only ELF/DEX inventory and checks against the port's compatibility tables."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def elf_inventory(path):
    data = path.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01" or struct.unpack_from("<H", data, 18)[0] != 40:
        raise ValueError("Expected ELF32 little-endian ARM")
    shoff = struct.unpack_from("<I", data, 32)[0]
    entsize, count, strings_idx = struct.unpack_from("<HHH", data, 46)
    sections = [struct.unpack_from("<10I", data, shoff + i * entsize) for i in range(count)]
    names = sections[strings_idx]
    strings = data[names[4]:names[4] + names[5]]
    def cstring(blob, offset):
        return blob[offset:blob.index(b"\0", offset)].decode("utf-8", "replace")
    named = {cstring(strings, s[0]): s for s in sections}
    dynsym = named[".dynsym"]
    string_section = sections[dynsym[6]]
    symbols = data[string_section[4]:string_section[4] + string_section[5]]
    imports, exports = [], {}
    for offset in range(dynsym[4], dynsym[4] + dynsym[5], dynsym[9]):
        name, address, size, info, other, section = struct.unpack_from("<IIIBBH", data, offset)
        if not name:
            continue
        name = cstring(symbols, name)
        if section == 0:
            imports.append({"name": name, "weak": info >> 4 == 2})
        else:
            exports[name] = {"address": address, "size": size}
    return {"sha1": hashlib.sha1(data).hexdigest(), "sha256": hashlib.sha256(data).hexdigest(),
            "imports": sorted(imports, key=lambda s: s["name"]), "exports": exports,
            "debug_info": ".debug_info" in named}


def dex_methods(path):
    data = path.read_bytes()
    if data[:4] != b"dex\x0a":
        raise ValueError("Expected a DEX file")
    def uleb(offset):
        value = shift = 0
        while True:
            byte = data[offset]
            offset += 1
            value |= (byte & 127) << shift
            if byte < 128:
                return value, offset
            shift += 7
    count, offset = struct.unpack_from("<II", data, 56)
    strings = []
    for i in range(count):
        _, start = uleb(struct.unpack_from("<I", data, offset + i * 4)[0])
        strings.append(data[start:data.index(b"\0", start)].decode("utf-8", "replace"))
    count, offset = struct.unpack_from("<II", data, 64)
    types = [strings[struct.unpack_from("<I", data, offset + i * 4)[0]] for i in range(count)]
    count, offset = struct.unpack_from("<II", data, 72)
    protos = []
    for i in range(count):
        _, result, params = struct.unpack_from("<III", data, offset + i * 12)
        args = "" if not params else "".join(types[struct.unpack_from("<H", data, params + 4 + j * 2)[0]]
                    for j in range(struct.unpack_from("<I", data, params)[0]))
        protos.append("(" + args + ")" + types[result])
    count, offset = struct.unpack_from("<II", data, 88)
    methods = []
    for i in range(count):
        cls, proto, name = struct.unpack_from("<HHI", data, offset + i * 8)
        if types[cls].startswith("Lcom/game/scrib/"):
            methods.append({"class": types[cls], "name": strings[name], "signature": protos[proto]})
    return methods


def audit(apk_dir, project):
    inventory = elf_inventory(apk_dir / "lib/armeabi/libScribAndroid.so")
    mappings = set(re.findall(r'\{\s*"([^"\n]+)"\s*,', (project / "source/dynlib.c").read_text()))
    inventory["unmapped_imports"] = [s["name"] for s in inventory["imports"] if s["name"] not in mappings]
    inventory["gl_imports"] = [s["name"] for s in inventory["imports"] if s["name"].startswith("gl")]
    source = "\n".join(p.read_text() for p in (project / "source").glob("*.c"))
    required = set(re.findall(r'"(Java_com_game_scrib_[A-Za-z0-9_]+|JNI_OnLoad)"', source))
    inventory["missing_exports"] = sorted(required - inventory["exports"].keys())
    inventory["java_methods"] = dex_methods(apk_dir / "classes.dex")
    print(f"{len(inventory['imports'])} imports; {len(inventory['gl_imports'])} OpenGL imports; {len(required)} required exports")
    print("Unmapped imports:", inventory["unmapped_imports"])
    print("Missing exports:", inventory["missing_exports"])
    return inventory


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk-dir", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    report = audit(args.apk_dir, project)
    (project / "analysis").mkdir(exist_ok=True)
    (project / "analysis/binary-audit.json").write_text(json.dumps(report, indent=2) + "\n")
    raise SystemExit(bool(report["unmapped_imports"] or report["missing_exports"]))
