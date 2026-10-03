"""Package the locally built loader and the user's prepared game files."""
import argparse
import json
from pathlib import Path
import shutil
import zipfile
from prepare_data import prepare, sha256
from audit_binary import audit
from package_livearea import validate_art, FILES as LIVEAREA_FILES, ART

INSTRUCTIONS = """Scribblenauts Remix Vita - build 0.9

Build 0.9 adds the original music and sound effects, music looping, and the
saved in-game sound setting. Audio decoding/mixing runs on a separate thread.
Normal builds have no loader, Android, JNI, or VitaGL logging, frame profiling,
or screenshot-request polling. There is no launch-time checksum scan.
Physical sticks and the supplied LiveArea artwork are retained.

Requirements on the Vita:
- Homebrew-enabled PS Vita with VitaShell.
- kubridge.skprx installed and loaded.
- libshacccg.suprx at ur0:data/libshacccg.suprx (or ur0:data/external/).

Install:
1. Install scribblenauts_vita.vpk using VitaShell.
2. Copy the contents of this bundle's ux0 folder to the Vita's ux0 drive.
   The native library and files named 1i and 1p must end up in
   ux0:data/scribblenauts/ . Keep res/raw and its 41 original Ogg files too.
3. Launch 'Scribblenauts Remix'. Give the first launch time to load.
4. Use the left stick for Maxwell and the right stick for the camera.
   Use the touchscreen for menus, objects, and the notebook.
   START sends Android's Back action. The notebook opens the Vita keyboard.
5. The game's sound option mutes music and effects and is saved between runs.

This build does not create loader.log or vitaGL.log. Errors that prevent
startup still appear on screen. Keep any Vita crash dump if one occurs.
Online services are disabled. Full suspend/resume support and broader
save/load and gameplay testing remain pending.

The loader does not contact game servers. It uses your local game files.
The VPK contains the loader and LiveArea artwork; the data folder is required.
"""


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-zip", type=Path, required=True)
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    report = audit(project.parent, project)
    if report["unmapped_imports"] or report["missing_exports"]:
        raise SystemExit("Binary audit failed")
    vpk = project / "build/scribblenauts_vita.vpk"
    validate_art()
    with zipfile.ZipFile(vpk) as archive:
        if archive.testzip() is not None:
            raise SystemExit("VPK CRC failure")
        if archive.read("eboot.bin")[:4] != b"SCE\0":
            raise SystemExit("VPK is missing a valid SELF executable")
        archive.getinfo("sce_sys/param.sfo")
        for source, target in LIVEAREA_FILES.items():
            if archive.read(target) != (ART / source).read_bytes():
                raise SystemExit(f"VPK artwork is stale: {target}")
    dist = project / "dist"
    prepare(project.parent, args.data_zip, dist / "ux0/data/scribblenauts")
    shutil.copy2(vpk, dist / vpk.name)
    shutil.copy2(vpk, dist / "Scribblenauts Remix.vpk")
    (dist / "INSTALL.txt").write_text(INSTRUCTIONS, encoding="utf-8")
    bundle = dist / "scribblenauts-vita-test.zip"
    entries = [dist / vpk.name, dist / "INSTALL.txt"] + sorted((dist / "ux0").rglob("*"))
    with zipfile.ZipFile(bundle, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=6, strict_timestamps=False) as archive:
        for path in entries:
            if path.is_file(): archive.write(path, path.relative_to(dist).as_posix())
    with zipfile.ZipFile(bundle) as archive:
        if archive.testzip() is not None:
            raise SystemExit("Bundle CRC failure")
    manifest = {"bundle": bundle.name, "sha256": sha256(bundle),
                "app_name": "Scribblenauts Remix", "livearea_revision": 2,
                "vpk_sha256": sha256(vpk), "game_sha256": report["sha256"],
                "status": "Build 0.9: original music and effects confirmed working on physical Vita with smooth movement; runtime logging disabled"}
    (dist / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Verified test bundle: {bundle} ({bundle.stat().st_size:,} bytes)")
