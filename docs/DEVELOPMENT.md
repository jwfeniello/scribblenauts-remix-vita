# Development notes

This project loads the supplied Android `libScribAndroid.so` with TheFloW/Rinnegatamante's `so_util`, FalsoJNI, and VitaGL. **Build 0.9 adds original music and sound effects and disables runtime logging by default.** It builds on the opening playground/menu, physical sticks, and faster startup confirmed on a PS Vita in build 0.8. The user confirmed music, effects, and smooth movement on the physical Vita in build 0.9. The user confirmed that both sticks work smoothly. A 14-second hardware test in build 0.7 measured approximately 29.6–29.8 FPS during movement, camera control, and simultaneous stick input, with the native joysticks engaged. Build 0.8's hardware run also stayed near 30 FPS during movement. Routine touch logging was causing the previous movement slowdown. Scene loading can still stall, and these measurements do not cover every level. The matrix-stack errors and accumulating transforms seen in builds 0.3–0.5 remain fixed. Touch callbacks reach the game, and the Vita keyboard has opened and returned an empty input. Creating objects and save/load behavior still need gameplay testing.

## Test on your Vita

Use `dist/scribblenauts-vita-test.zip`, refreshed with build 0.9 and the locally prepared game data. Launches use quick file checks with no checksum scan. The named installer is `dist/Scribblenauts Remix.vpk`; the build output is `build/scribblenauts_vita.vpk`. Subsequent development updates replace only the installed executable over FTP.

1. Have a homebrew-enabled Vita with VitaShell, `kubridge.skprx` loaded, and `libshacccg.suprx` installed in `ur0:data/` or `ur0:data/external/`.
2. Install `scribblenauts_vita.vpk` with VitaShell. Title ID: `SCRB00001`.
3. Copy the contents of the bundle's `ux0` directory to the Vita's `ux0` drive. The files `1i`, `1p`, and `libScribAndroid.so` must be under `ux0:data/scribblenauts/`.
4. Launch **Scribblenauts Remix**. The left stick controls Maxwell and the right stick controls the camera through the game's virtual joysticks. The touchscreen handles menus, object interaction, and the notebook. START invokes the original Android Back action. Opening the notebook is wired to the Vita keyboard.
5. Keep `res/raw/` and its 41 original Ogg files under `ux0:data/scribblenauts/`. The in-game sound option controls music/effects and persists in `sound.enabled`.

Music and sound effects are implemented. Additional gameplay-button mappings and full suspend/resume support are still pending. Online services, store UI, social integration, and Android system dialogs are inactive. Saves are directed into `ux0:data/scribblenauts/saves/`; save/load behavior needs testing.

## What is implemented

- Version-specific native loader with fast file-size checks at startup and original-instruction checks before patching. Full native-library hashes are verified by the PC audit/packaging tools, not during Vita launches.
- Pre-initialization import audit; all 186 imports are represented in the compatibility table. This is symbol coverage, not proof of ABI or behavioral compatibility.
- OpenGL ES 1.x calls routed through VitaGL, with framebuffer aliases and tracked renderbuffer width/height/format queries.
- Four fixed-function texture stages, with separate texture matrices, coordinate streams, combiners, and shader-cache entries. The cache is private to `ux0:data/scribblenauts/shader_cache/`.
- Android startup sequence reconstructed from the supplied DEX and native disassembly, including external package/index paths and screen dimensions.
- Native initialization and a render loop paced to the Android renderer's approximately 30 Hz update interval.
- Physical left/right sticks mapped to the native virtual joysticks with a radial dead zone, bounded diagonal movement, separate touch IDs, and release on neutral input or keyboard/screen transitions.
- Touch callbacks, START/Back, offline startup callbacks, and Vita IME text entry.
- Original Android music/SFX ID mapping, streamed Vorbis music, cached effects, overlapping voices, looping, and persistent mute. A dedicated worker resamples original mono assets to 48 kHz and outputs them through Vita audio.
- Logging, frame profiling, draw tracing, and screenshot request polling are disabled in normal builds. Fatal startup errors still appear on screen; diagnostics can be enabled explicitly for development.
- Local data preparation with full index validation, hashes, and six malformed-data regression checks.

No source decompilation of the entire game is required to begin this loader approach. Ghidra may be helpful later for investigating a specific crash.

## Build on this Windows machine

The build uses Ubuntu in WSL. It installs a separate SoftFP SDK at `$HOME/.local/share/scrib-vitasdk-softfp`; the pre-existing `/usr/local/vitasdk` is not modified.

From Ubuntu, in this directory:

```sh
bash scripts/setup-sdk.sh
bash scripts/build.sh
bash scripts/test_sticks.sh
bash scripts/test_audio.sh
```

`scripts/test_audio.sh` requires host `libvorbis-dev` and runs address/undefined-behavior sanitizers against the mixer and original assets. `SCRIB_DIAGNOSTICS=ON bash scripts/build.sh` opts into development logs/profiling; the default is OFF even though the local ELF retains debug symbols.

`SCRIB_VITASDK` overrides the SDK directory. Compilation takes place under `$HOME/.cache/scrib-vita/build-project` to avoid Windows filesystem paths and timestamps in make. Outputs are copied to `build/`:

- `scribblenauts_vita.vpk`: installable loader with logging disabled by default.
- `scribblenauts_vita`: ARM ELF with debug information for crash analysis.
- `eboot.bin`: packaged Vita executable.
- `build.log`: compiler and packager output.

The build uses the pinned `lib/vitagl` submodule, not the SDK's graphics archive. `patches/falsojni-utf8.patch` fixes an upstream allocation that otherwise sizes a UTF-8 buffer using a UTF-16 character count. `patches/vitagl-four-texture-stages.patch` adds the four texture stages required by this game's 2D renderer and fixes the related matrix, attribute-mask, and shader-cache handling. `build.sh` applies both patches to a clean build copy when necessary; it also accepts already patched submodules. Four-texture rendering with Phong lighting is outside this game-specific patch's scope.

From PowerShell, in this directory:

```powershell
python scripts/audit_binary.py
python scripts/test_data.py
python scripts/package_test.py --data-zip '..\..\scribblenauts-remix-v6.9.zip'
```

The packaging script checks the VPK and data ZIP and creates the local test bundle. It does not publish or upload files. `dist/`, `analysis/`, build products, and local tools are ignored by Git. The locally prepared test ZIP contains game data and is not a public release artifact.

## Inspected inputs

- Native file: `../lib/armeabi/libScribAndroid.so` (ELF32 ARMv5TE, retained DWARF information).
- SHA1: `cf5d89fa375da8f6ec4b48fe8ea3f309ac777396`.
- SHA256: `799040d510079ff3da458039483f78a1fd9b7e9a43794197525575694dc83e3c`.
- Main OBB: `main.51.com.wb.goog.scribbleremix.obb`, 155,347,587 bytes; copied as `1p`.
- Patch OBB: `patch.51.com.wb.goog.scribbleremix.obb`, 119,988 bytes; copied as `1i`.
- The index describes 29,995 entries with monotonic offsets ending exactly at the main package size.

Five game-code patches change the conditional instruction `0x0a000059` to `0xea000059`, retaining each branch's original target and skipping only the Android Breakpad constructor/destructor block. All five original instructions are checked before any patch is applied. The PC packaging tools verify the supplied library hash. No original binary is changed on disk.

| JNI wrapper | Instruction offset | Branch target |
| --- | --- | --- |
| nativeResume | `0x000f0d74` | `0x000f0ee0` |
| nativePause | `0x000f0fb0` | `0x000f111c` |
| nativeGetText | `0x000f1428` | `0x000f1594` |
| nativeInit | `0x000f1a30` | `0x000f1b9c` |
| nativeRender | `0x000f67b8` | `0x000f6924` |

The 0.2 crash dump stopped at native offset `0x3f98e4`, immediately after Breakpad's inline Linux `sigaltstack` system call. The VitaGL error in 0.1 was a loader bug: this pinned VitaGL version returns a resolution-fallback flag from `vglInitExtended`, so zero is normal for 960x544.

Builds 0.3–0.5 exposed a second rendering issue. The game selects texture units 0–3, while the original VitaGL configuration allocated only two texture matrices. Its out-of-range `glActiveTexture` comparison aliased the modelview matrix and changed the active matrix mode during a draw. `glPopMatrix` then popped the texture stack instead of restoring the sprite transform. Build 0.6 provides four complete texture stages, bounds the comparison, fixes the third coordinate attribute, and preserves all four coordinate bits when submitting vertex streams. Startup tests report four texture units, zero matrix-mode failures, and a successful matrix push/pop round trip. The patch was checked against a clean copy of the pinned VitaGL source.

Build 0.7 filters routine Android debug/info messages before formatting and only forces a storage flush for errors. Earlier builds logged and flushed several messages for every touch movement, with roughly 92 ms between recent movement events in the captured 0.6 log. Automatic PNG captures and per-draw error checks are disabled. The render loop retains its 30 FPS limit.

Build 0.8 removes the per-launch SHA1 scan at the user's request. The 47.9 MB library check had occupied about 38 seconds on every launch. The Vita now only checks file metadata before loading; full hashes remain part of PC data preparation and packaging. Existing gameplay clocks are applied at the start of initialization. In the recorded hardware run, file checks took 0.001 seconds, the first rendered frame appeared 6.60 seconds after process start (previously 44.65 seconds), and the boot-complete callback arrived at 23.88 seconds (previously 65.60 seconds). The game's original intro and asset loading still run, and their timing can vary with input and storage.

Only in an explicitly enabled diagnostic build, creating `ux0:data/scribblenauts/capture.request` requests a PNG capture; it is consumed at a frame boundary. Capturing briefly pauses rendering and those frames are excluded from performance samples. `loader.log` includes matrices for captured draws. For verbose Android traces, create `verbose-log.enabled` in the same directory before launching; remove it and relaunch for normal performance. VitaGL can still report unsupported `GL_UNPACK_ALIGNMENT` requests during RGBA texture uploads; these are separate from the fixed stack errors.

In a diagnostic build, for a repeatable hardware check, create `stick-test.request` in the game data directory while both virtual joysticks are available. The loader runs a 14-second sequence of idle, Maxwell movement in both directions, camera movement in both directions, simultaneous sticks, and release. Touching the screen or moving a physical control cancels the sequence. `PERF` records show average FPS, input/game/debug/swap times, draw counts, and native joystick engagement; `input_mask` uses bit 0 for the left stick, bit 1 for the right stick, and bit 2 for the touchscreen. These measurements describe the scene being tested, not all levels.

## LiveArea artwork

LiveArea revision 2 adds the supplied Maxwell bubble icon, Scribblenauts Remix logo, a scene adapted around the centered launch tile, and a matching launch background. The app display name is now **Scribblenauts Remix**, and the user confirmed the new artwork looks good on the Vita. The images in `extras/livearea/` use the sizes demonstrated by the supplied `pretty_livearea` sample and indexed PNG-8 with at most 128 colors, following the [VitaSDK image guidance](https://github.com/vitasdk/samples/blob/master/README.md#notes-on-images).

The final LiveArea images and an asset-provenance record are included in the repository. Original working images are kept locally and excluded from Git. The `prepare_livearea.ps1` script expects those working images in `extras/livearea/source/`. The scene adaptation used the built-in image generation tool. The supplied logo and icon are retained as original artwork and resized for the shell. Run `scripts/prepare_livearea.ps1` with ImageMagick installed to reproduce the final sizes and encoding. The layout preview is written to `analysis/livearea/livearea-preview.png`; the Vita supplies the actual gate border and Start button.

`python scripts/package_livearea.py` updates an existing VPK's artwork and title while preserving its executable byte-for-byte. This provides an artwork-only packaging path without a compiler rebuild. `python scripts/deploy_livearea.py` backs up and verifies the app assets plus this title's artwork under `ur0:appmeta/SCRB00001/`, then updates both over FTP. It also uploads an installer to `ux0:data/scribblenauts/scribblenauts-livearea.vpk`. Close the old LiveArea page and reopen the bubble after updating. If the shell retains the old icon or title, reinstall that VPK in VitaShell to refresh the registered metadata. Backups and SHA-256 records are stored under `analysis/hardware/`.

## FTP development loop

The tested Vita Companion setup uses FTP on port 1337 and the older command service on port 1338. Supply your own device address with `--host`. Keep it awake; dismiss crash dialogs if either service becomes unreachable.

```powershell
python scripts/vita.py logs --host YOUR_VITA_IP
python scripts/vita.py deploy --host YOUR_VITA_IP
```

`deploy` saves the prior executable and matching new debug ELF locally, sends `destroy` (closes running applications), uploads and reads back the new executable to verify SHA-256, retains a remote backup, and sends `launch SCRB00001`. A launch reply is not proof of a successful boot; inspect the screen and listen for music/effects. Normal builds do not generate logs. No plugin replacement or remote VPK installation is involved. Use `--host` if the Vita's address changes.

Logs and deployment records are under `analysis/hardware/`. `scripts/inspect_core.py PATH.psp2dmp` prints crash registers and stack words directly from a compressed Vita dump, using the note layouts documented by [vita-parse-core](https://github.com/xyzz/vita-parse-core). Stack words are candidate addresses, not a reconstructed backtrace.

Further runtime work includes validating Bionic libc/stdio structure compatibility, renderer framebuffer behavior, memory usage, and any Java callbacks that the hardware run reveals. Diagnostic builds report missing renderbuffer query types. Do not interpret a successful link as a playable port.

## Foundation and credits

Based on [soloader-boilerplate](https://github.com/v-atamanenko/soloader-boilerplate), with [so_util](https://github.com/Rinnegatamante/so_util), [FalsoJNI](https://github.com/v-atamanenko/FalsoJNI), [VitaGL](https://github.com/Rinnegatamante/vitaGL), and [VitaSDK SoftFP](https://github.com/vitasdk-softfp). Their existing licenses and attribution are retained. The provided scratch folder's VitaGL/VitaSDK documentation was also consulted.
