"""Validate the supplied OBB pair and stage local files for the Vita loader."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zipfile


def validate_index(index, package_size):
    if len(index) < 12 or len(index) % 4:
        raise ValueError("Invalid package index length")
    words = struct.unpack(f"<{len(index) // 4}I", index)
    count, *offsets = words
    if count < 1 or len(offsets) != count + 1:
        raise ValueError("Index entry count does not match its length")
    if offsets[0] != 0 or offsets[-1] != package_size:
        raise ValueError("Index does not match the complete package size")
    if any(a > b for a, b in zip(offsets, offsets[1:])):
        raise ValueError("Index offsets are not monotonic")
    return count


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def prepare(apk_dir, data_zip, output):
    native = apk_dir / "lib/armeabi/libScribAndroid.so"
    if not native.is_file():
        raise ValueError(f"Missing ARM library: {native}")
    with native.open("rb") as stream:
        head = stream.read(20)
    if head[:6] != b"\x7fELF\x01\x01" or head[18:20] != b"\x28\x00":
        raise ValueError("Expected a little-endian ELF32 ARM library")
    with zipfile.ZipFile(data_zip) as archive:
        main = [i for i in archive.infolist() if i.filename.endswith("main.51.com.wb.goog.scribbleremix.obb")]
        patch = [i for i in archive.infolist() if i.filename.endswith("patch.51.com.wb.goog.scribbleremix.obb")]
        if len(main) != 1 or len(patch) != 1:
            raise ValueError("Expected exactly one version 51 main/patch OBB pair")
        index = archive.read(patch[0])
        count = validate_index(index, main[0].file_size)
        output.mkdir(parents=True, exist_ok=True)
        # Fixed destination names: archive member paths never control extraction.
        for info, name in [(main[0], "1p"), (patch[0], "1i")]:
            temporary = output / (name + ".partial")
            with archive.open(info) as source, temporary.open("wb") as target:
                shutil.copyfileobj(source, target, 1024 * 1024)
            temporary.replace(output / name)
    shutil.copy2(native, output / native.name)
    raw = output / "res/raw"
    raw.mkdir(parents=True, exist_ok=True)
    for source in sorted((apk_dir / "res/raw").glob("*.ogg")):
        shutil.copy2(source, raw / source.name)
    manifest = {
        "package_entries": count,
        "status": "Prepared data; runtime compatibility is not yet validated",
        "files": {str(p.relative_to(output)).replace("\\", "/"): {
            "size": p.stat().st_size, "sha256": sha256(p)
        } for p in sorted(output.rglob("*")) if p.is_file() and p.name != "data-manifest.json"},
    }
    (output / "data-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Validated {count:,} indexed files; staged {len(manifest['files'])} files in {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk-dir", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--data-zip", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "dist/ux0/data/scribblenauts")
    args = parser.parse_args()
    prepare(args.apk_dir.resolve(), args.data_zip.resolve(), args.output.resolve())
