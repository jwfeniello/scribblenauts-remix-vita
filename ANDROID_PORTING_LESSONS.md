Lessons from porting the Android version of **Scribblenauts Remix v6.9** to a physical PS Vita, through build 0.9 on October 3, 2026. The examples below come from this game's binary, the dependencies we used, and our hardware tests.

- **Start by identifying where the game actually runs.** This game contains an ARM native library, `libScribAndroid.so`, that we could load through a compatibility layer. VitaGL handled graphics, while the port still needed library loading, relocations, Android imports, Java callbacks, input, filesystem handling, and audio.

- **Match the calling convention across the port and its dependencies.** We used a separate SoftFP VitaSDK and built VitaGL with the matching ABI. Checking that both sides are ARM is only part of the compatibility work; arguments and return values also need to cross function boundaries correctly.

- **Ghidra does not have to be the first step.** The supplied library retained symbols and DWARF information. Import/export inspection, disassembly, and the decompiled Java code provided enough information to start the loader. A decompiler becomes useful when a particular function or crash needs deeper investigation.

- **Read the Java side early.** `GameplayActivity` revealed the initialization callbacks and settings behavior. `AudioController` contained the exact music and sound-effect mappings. Those classes explained behavior that would have taken longer to reconstruct from native assembly alone.

- **An import audit measures coverage, not correctness.** We accounted for 186 imports, including 44 OpenGL imports, and checked 18 required exports. That established which functions the loader could resolve. Behavior, argument layouts, ownership, and Bionic compatibility still needed inspection and hardware testing. See [the binary audit](scripts/audit_binary.py).

- **Reproduce the Android startup sequence and callback timing.** The game needed package paths, save paths, screen dimensions, settings, and boot-completion callbacks. Java normally queued some of these actions; our JNI handlers queue them for the main loop too, avoiding calls back into the game while it is already inside a callback.

- **JNI string conversion deserves careful review.** We fixed a FalsoJNI allocation that sized a UTF-8 destination using a UTF-16 character count. Porting string APIs requires tracking the destination's byte length as well as the source's character count. The reproducible fix is in [the UTF-8 patch](patches/falsojni-utf8.patch).

- **Native Android libraries can contain direct Linux system calls.** An early crash occurred in Android Breakpad immediately after an inline `sigaltstack` call. Resolving imported symbols could not replace that instruction. We bypassed the specific crash-reporter constructor/destructor blocks in five JNI wrappers.

- **Make binary patches specific and check the original instructions.** Our patches retain the original branch targets and check all five expected instructions before changing any of them. The original library stays unchanged on disk. The offsets belong to the inspected game version; [patch.c](source/patch.c) records the exact changes.

- **Understand the asset containers before rearranging them.** This game's two OBB files form a package/index pair, used by the port as `1p` and `1i`. The index contains 29,995 entries. Checking counts, offsets, and the final package size on the PC gave us a reliable data-preparation step while retaining the game's existing asset reader.

- **Check filesystem assumptions at the Android/Vita boundary.** The game's directory walker dropped Vita's drive prefix, so the loader prepares its save directories explicitly. Game data and saves live under `ux0:data/scribblenauts/`, separately from the installed application. A loader update therefore does not need to replace the save directory.

- **Read the implementation of the dependency version being used.** We initially misinterpreted `vglInitExtended`'s result. In the pinned VitaGL version, zero was normal for the requested 960x544 resolution because the result represented a resolution fallback. Assuming familiar success/failure semantics caused a false initialization error.

- **Supporting more texture units requires changes throughout the renderer.** The game used four GLES texture stages; the original configuration allocated two texture matrices. A complete fix needed matching matrices, coordinate streams, combiners, attribute masks, and shader-cache handling. Changing the advertised texture-unit count alone would have left those structures inconsistent.

- **Moving or drifting artwork can expose corrupted graphics state.** The Maxwell icon's accumulating movement came from texture-unit selection corrupting matrix state. The active matrix mode and subsequent push/pop behavior helped identify the cause. We checked matrix round trips and kept the fix in [the four-texture-stage patch](patches/vitagl-four-texture-stages.patch).

- **Existing virtual sticks can provide a useful route to physical controls.** We mapped Vita sticks into the game's own joystick touch handling. Reading the native joystick geometry and reversing its coordinate transform kept the synthetic touches aligned with the controls. Touching the center for one frame before moving allowed the game to acquire the stick reliably.

- **Synthetic input needs a full press, move, and release lifecycle.** Each physical stick has its own touch ID, separate from front-touch IDs. Dead zones, bounded diagonal movement, neutral release, and release during keyboard or screen transitions are all part of the implementation. [The stick helper](source/stick_touch.c) isolates that behavior for testing.

- **Measure logging and storage work during frame-rate drops.** Movement generated repeated touch messages and storage flushes, contributing to the drop from roughly 30 FPS at rest to 9-10 FPS while moving. Reducing that work brought the tested movement/camera sequence to approximately 29.6-29.8 FPS in build 0.7. Those measurements describe the tested scene.

- **Separate diagnostic output from debug symbols and validation.** Build 0.9 disables loader, Android, JNI, and VitaGL logging, plus profiling and capture polling, by default. The PC still keeps an ELF with debug information, and startup failures still display an error. Disabling output did not require disabling VitaGL's normal error checks. Diagnostics remain available through `SCRIB_DIAGNOSTICS=ON`.

- **Time startup checks individually.** Scanning the 47.9 MB library for its checksum cost about 38 seconds per launch on this Vita. Moving full hash verification to PC preparation/deployment and retaining quick file checks reduced the recorded first-frame time from 44.65 seconds to 6.60 seconds. The original intro and asset loading still contribute to the remaining startup time.

- **Preserve the original audio IDs and their semantics.** The Java controller defined 21 music IDs and 25 effect IDs backed by 41 unique Ogg files. Some IDs deliberately reference the same asset. We share their decoded data while retaining separate effect voices, including the original behavior of restarting an effect when its ID is triggered again.

- **Stream long music and cache short effects.** Decoding every music track into PCM would use much more memory than keeping one streaming decoder. Short effects were small enough to decode once and reuse. Both paths run on a dedicated worker so the render loop does not perform audio decoding or audio-file reads.

- **Sample rate, loop boundaries, and mixing need explicit handling.** The original music is mono at 32 kHz and the effects are mono at 44.1 kHz; our Vita output is 48 kHz. The mixer uses a persistent resampling phase, handles loop boundaries, and saturates overlapping samples to prevent integer wraparound. Muting advances playback silently, matching the original volume-based behavior.

- **Keep audio communication with the game thread small.** The callbacks publish music changes, effect requests, and mute state under a short lock. The worker copies that state and performs decoding, mixing, output, and settings writes after releasing the lock. This keeps expensive audio work out of the render thread's critical section. See [audio.c](source/audio.c).

- **Use host tests for isolated logic and hardware for the complete result.** Stick tests exercise touch-state transitions. Audio tests use the original assets with address and undefined-behavior sanitizers and check duration, looping, overlap, retriggering, clipping, mute, and stop behavior. The physical Vita and the user's listening/input tests established that music, effects, and smooth movement worked together.

- **Make dependency fixes reproducible.** We keep the VitaGL and FalsoJNI changes as patches against their pinned sources. Testing a clean clone exposed a subtle problem: `patch --batch --reverse --dry-run` could silently retry forwards and report success on unpatched files. Adding `--force` to the reverse probe stops that fallback. Verify the resulting source against the tested implementation as well as checking that compilation succeeds.

- **Build in a controlled environment.** On this Windows machine, building in WSL's Linux filesystem avoided problems with paths containing spaces and filesystem timestamps. The separate SoftFP SDK also kept this project's toolchain from replacing the existing standard VitaSDK. [The build script](scripts/build.sh) copies the finished artifacts back to the project.

- **Check FTP and remote commands independently.** FTP on port 1337 and Vita Companion commands on port 1338 are separate services. During testing, one could respond while the other failed. Waking the Vita and dismissing a crash dialog restored access in earlier sessions; a successful upload by itself never established that the game launched.

- **Use the installed Vita Companion command dialect.** This installation accepted newline-terminated `destroy` and `launch SCRB00001` commands. `help` and `quit` were not recognized. `destroy` closes running applications across the development session. Remote input, screenshots, and remote VPK installation were not established by this workflow.

- **Expect small FTP-server compatibility differences.** The installed server supported `LIST` but rejected `NLST`. It also returned `226 File deleted` for a successful deletion, which Python's stricter `FTP.delete()` treated as unexpected. Absolute paths such as `/ux0:/...` also prevented directory changes from being interpreted relative to the previous location.

- **Make executable replacement recoverable and verify what reached the device.** Our deployment saves the previous executable and matching new debug ELF, uploads to a temporary filename, verifies the uploaded bytes, then promotes the file and launches it. The application mount sometimes needed a short retry after `destroy`. These deployment checks run from the PC and add no work to ordinary game launches.

- **Treat LiveArea presentation as part of packaging.** Replacing `eboot.bin` updates the program but does not refresh the icon or registered title. We packaged the artwork and `param.sfo` together and inspected the title's cache under `ur0:appmeta/`. The working assets used indexed PNGs at 128x128 for the icon, 960x544 for `pic0`, 840x500 for the LiveArea background, and 280x158 for the startup tile. Reinstalling the full VPK was the refresh path offered for a stale bubble icon.

- **Record what was actually confirmed on hardware.** By build 0.9, the user had confirmed the artwork, both sticks, music, effects, and smooth movement. We also verified that the removed logs were not recreated. Full suspend/resume support, broader save/load behavior, and every level remain outside those confirmations. [The project README](README.md) records the implementation and remaining work.
