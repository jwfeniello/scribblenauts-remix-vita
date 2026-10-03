#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
cache="${XDG_CACHE_HOME:-$HOME/.cache}/scrib-vita"
mkdir -p "$cache" "$project_dir/.tools/jadx"
if [[ ! -f "$project_dir/.tools/jadx/lib/jadx-1.5.6-all.jar" ]]; then
    curl -fL --retry 3 -o "$cache/jadx.zip" \
      'https://github.com/skylot/jadx/releases/download/v1.5.6/jadx-1.5.6.zip'
    unzip -q -o "$cache/jadx.zip" -d "$project_dir/.tools/jadx"
fi
if [[ ! -x "$cache/jre/bin/java" ]]; then
    curl -fL --retry 3 -o "$cache/jre.tar.gz" \
      'https://api.adoptium.net/v3/binary/latest/21/ga/linux/x64/jre/hotspot/normal/eclipse'
    mkdir -p "$cache/jre"
    tar xzf "$cache/jre.tar.gz" --strip-components=1 -C "$cache/jre"
fi
"$cache/jre/bin/java" -Xmx1g -cp "$project_dir/.tools/jadx/lib/*" \
    jadx.cli.JadxCLI --no-res -j 4 -d "$project_dir/analysis/java" "$project_dir/../classes.dex"
