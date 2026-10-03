"""Refresh artwork and the display title in an existing VPK, preserving its executable."""
import hashlib
from pathlib import Path
import struct
import zipfile
import xml.etree.ElementTree as ET

PROJECT = Path(__file__).resolve().parents[1]
ART = PROJECT / "extras/livearea"
FILES = {
    "icon0.png": "sce_sys/icon0.png",
    "pic0.png": "sce_sys/pic0.png",
    "bg0.png": "sce_sys/livearea/contents/bg0.png",
    "startup.png": "sce_sys/livearea/contents/startup.png",
    "template.xml": "sce_sys/livearea/contents/template.xml",
}
SIZES = {"icon0.png": (128, 128), "pic0.png": (960, 544),
         "bg0.png": (840, 500), "startup.png": (280, 158)}


def set_title(sfo):
    data = bytearray(sfo)
    magic, version, keys, values, count = struct.unpack_from("<5I", data)
    assert magic == 0x46535000
    fields = {}
    for i in range(count):
        entry = 20 + i * 16
        key_off, fmt, length, capacity, offset = struct.unpack_from("<HHIII", data, entry)
        key = bytes(data[keys + key_off:]).split(b"\0", 1)[0].decode()
        fields[key] = (entry, fmt, capacity, values + offset, length)
    _, _, _, offset, length = fields["TITLE_ID"]
    assert bytes(data[offset:offset + length]).rstrip(b"\0") == b"SCRB00001"
    for key in ("TITLE", "STITLE"):
        entry, fmt, capacity, offset, _ = fields[key]
        value = b"Scribblenauts Remix\0"
        assert fmt == 0x204 and len(value) <= capacity
        data[offset:offset + capacity] = value.ljust(capacity, b"\0")
        struct.pack_into("<I", data, entry + 4, len(value))
    return bytes(data)


def validate_art():
    for name, size in SIZES.items():
        data = (ART / name).read_bytes()
        assert data[:8] == b"\x89PNG\r\n\x1a\n"
        assert struct.unpack_from(">II", data, 16) == size, name
        assert data[24:26] == bytes((8, 3)), f"{name}: expected indexed PNG-8"
        assert len(data) < 420 * 1024, name
    template = (ART / "template.xml").read_bytes()
    assert len(template) < 32768 and b"\r\n" in template
    root = ET.fromstring(template)
    assert root.tag == "livearea" and root.attrib["style"] == "a1"
    for node in root.findall(".//image") + root.findall(".//startup-image"):
        assert (ART / node.text.strip()).is_file()


def package():
    validate_art()
    vpk = PROJECT / "build/scribblenauts_vita.vpk"
    temporary = vpk.with_suffix(".vpk.tmp")
    replacements = {target: (ART / source).read_bytes() for source, target in FILES.items()}
    with zipfile.ZipFile(vpk) as original:
        expected_eboot = hashlib.sha256(original.read("eboot.bin")).hexdigest()
        replacements["sce_sys/param.sfo"] = set_title(original.read("sce_sys/param.sfo"))
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as updated:
            for item in original.infolist():
                updated.writestr(item, replacements.pop(item.filename, None) or original.read(item.filename))
            for name, data in replacements.items():
                updated.writestr(name, data)
    with zipfile.ZipFile(temporary) as check:
        assert check.testzip() is None
        assert hashlib.sha256(check.read("eboot.bin")).hexdigest() == expected_eboot
        for source, target in FILES.items():
            assert check.read(target) == (ART / source).read_bytes()
    temporary.replace(vpk)
    print(f"Updated LiveArea and title in {vpk}")
    print(f"Preserved game executable SHA256: {expected_eboot}")


if __name__ == "__main__":
    package()
