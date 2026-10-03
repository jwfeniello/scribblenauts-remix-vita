#!/usr/bin/env bash
set -euo pipefail
# Keep this port's soft-float SDK separate from an existing standard VitaSDK.
sdk="${SCRIB_VITASDK:-$HOME/.local/share/scrib-vitasdk-softfp}"
cache="${XDG_CACHE_HOME:-$HOME/.cache}/scrib-vita"
mkdir -p "$cache"
if [[ ! -x "$sdk/bin/arm-vita-eabi-gcc" ]]; then
    curl -fL --retry 3 -o "$cache/sdk.tar.bz2" \
      https://github.com/vitasdk-softfp/autobuilds/releases/download/master-linux-v2.23/vitasdk-x86_64-linux-gnu-2026-03-29_13-36-57.tar.bz2
    mkdir -p "$sdk"
    tar xjf "$cache/sdk.tar.bz2" --strip-components=1 -C "$sdk"
fi
mkdir -p "$sdk/.scrib-packages"
for package in zlib bzip2 libzip libpng libogg libvorbis flac opus mpg123 lame libsndfile opensles taihen kubridge vitaShaRK libmathneon SceShaccCgExt; do
    if [[ ! -f "$sdk/.scrib-packages/$package.installed" ]]; then
        curl -fsSL --retry 3 -o "$cache/$package.tar.xz" \
          "https://github.com/vitasdk-softfp/packages/releases/download/master/$package.tar.xz"
        tar xJf "$cache/$package.tar.xz" -C "$sdk/arm-vita-eabi"
        touch "$sdk/.scrib-packages/$package.installed"
    fi
done
printf 'SoftFP SDK ready: %s\n' "$sdk"
