# Scribblenauts Remix for PS Vita

Scribblenauts Remix is a mobile remake combining content from Scribblenauts and Super Scribblenauts for iOS and Android devices. The original games were developed by 5TH Cell, with the mobile adaptation by Iron Galaxy Studios.

This is a **heavily AI-assisted** port of the Android version of Scribblenauts Remix to the PS Vita. It started as an experiment I didn't plan on posting, but it's been quite fun and has worked surprisingly well.

You'll need the game data from **Scribblenauts Remix v6.9**. Enjoy!


## What's working

- Rendering through VitaGL at 960x544.
- Left stick to move Maxwell and right stick to move the camera.
- Touchscreen controls for menus, objects, and the notebook.
- Vita keyboard integration and START for the original Android Back action.
- Original background music and sound effects, with a saved sound setting.
- Custom bubble, LiveArea, and launch artwork.
- Runtime logging disabled and no checksum scan on each launch.


## Installation

You need a homebrew-enabled PS Vita with VitaShell, `kubridge.skprx` installed and loaded, and `libshacccg.suprx` installed in `ur0:data/` or `ur0:data/external/`.

1. Download [Scribblenauts Remix.vpk](https://github.com/jwfeniello/scribblenauts-remix-vita/raw/refs/heads/main/releases/Scribblenauts%20Remix.vpk) and install it with VitaShell. You can also build the VPK using the instructions below. The application name is **Scribblenauts Remix** and its title ID is `SCRB00001`.
2. Extract your own Android v6.9 APK and its matching expansion files.
3. Copy `lib/armeabi/libScribAndroid.so` from the APK to `ux0:data/scribblenauts/libScribAndroid.so`. Use this ARM library, not another architecture or game version.
4. Copy `main.51.com.wb.goog.scribbleremix.obb` to `ux0:data/scribblenauts/1p` and `patch.51.com.wb.goog.scribbleremix.obb` to `ux0:data/scribblenauts/1i`. Rename the files as shown; keep their contents intact.
5. Copy the APK's `res/raw/` directory, including all 41 Ogg audio files, to `ux0:data/scribblenauts/res/raw/`.
6. Launch **Scribblenauts Remix**.

The resulting data layout is:

```text
ux0:data/scribblenauts/
  libScribAndroid.so
  1p
  1i
  res/
    raw/
      jng_win.ogg
      mus_*.ogg
      sfx_*.ogg
```

Saves are created under `ux0:data/scribblenauts/saves/`. The APK, native game library, expansion files, and audio assets are not included in this repository or the VPK.

## Building

Use Linux or Ubuntu through WSL. The scripts use a separate SoftFP VitaSDK and keep the existing standard VitaSDK untouched. Install Git, CMake, Make, a host C compiler, Python 3.11 or newer, curl, tar, xz, bzip2, and patch first.

```sh
git clone --recurse-submodules https://github.com/jwfeniello/scribblenauts-remix-vita.git
cd scribblenauts-remix-vita
bash scripts/setup-sdk.sh
bash scripts/build.sh
```

The VPK is written to `build/scribblenauts_vita.vpk`. The game data is required to play, but is not required to compile the loader. `SCRIB_VITASDK` overrides the SDK location. The build script applies the included FalsoJNI and VitaGL patches to its build copy.

For checks on the host PC:

```sh
python3 scripts/test_data.py
bash scripts/test_sticks.sh
# Requires libvorbis-dev and your extracted APK's res/raw directory:
bash scripts/test_audio.sh /path/to/extracted-apk/res/raw
```

Runtime diagnostics are off by default. Use `SCRIB_DIAGNOSTICS=ON bash scripts/build.sh` for a development build with logs and profiling. Debug symbols remain available in the local ELF for crash analysis.

[Development notes](docs/DEVELOPMENT.md) cover the loader, graphics fixes, packaging, and FTP workflow. [Android porting lessons](ANDROID_PORTING_LESSONS.md) record what we learned while building the port.

## Credits

- **5TH Cell** for the original Scribblenauts games and **[Iron Galaxy Studios](https://www.irongalaxystudios.com/about)** for the mobile adaptation.
- **Warner Bros.** for publishing Scribblenauts Remix.
- **[soloader-boilerplate](https://github.com/v-atamanenko/soloader-boilerplate)** by Volodymyr Atamanenko, based on work by TheFloW and Rinnegatamante.
- **[so_util](https://github.com/Rinnegatamante/so_util)**, **[FalsoJNI](https://github.com/v-atamanenko/FalsoJNI)**, **[VitaGL](https://github.com/Rinnegatamante/vitaGL)**, and **[VitaSDK SoftFP](https://github.com/vitasdk-softfp)**.
- The original authors of the included compatibility code and dependencies; their notices and licenses are retained.

The loader's existing MIT license is in [LICENSE](LICENSE). Dependencies retain their own licenses. The Scribblenauts name and artwork belong to their respective owners. This is an unofficial fan port.
