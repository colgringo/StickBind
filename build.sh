#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

CC32="${CC32:-i686-w64-mingw32-gcc}"
CC64="${CC64:-x86_64-w64-mingw32-gcc}"

build_one() {
    local cc="$1" arch="$2" out_exe="$3" out_dll="$4"
    if ! command -v "$cc" >/dev/null 2>&1; then
        printf 'SKIP %s: compiler not found (%s)\n' "$arch" "$cc" >&2
        return 1
    fi
    "$cc" -municode -O2 -s -static -static-libgcc -mwindows \
        -o "$out_exe" StickBindLauncher.c
    "$cc" -shared -O2 -s -static-libgcc \
        -o "$out_dll" StickBind.c StickBind.def -luser32 -lm
    printf 'Built %s + %s\n' "$out_exe" "$out_dll"
    return 0
}

ok=0
if build_one "$CC32" "32-bit" StickBindLauncher32.exe SB32.dll; then
    ok=$((ok + 1))
fi
if build_one "$CC64" "64-bit" StickBindLauncher64.exe SB64.dll; then
    ok=$((ok + 1))
fi

if [ "$ok" -eq 0 ]; then
    cat >&2 <<'MSG'
No MinGW compiler found.
  32-bit: i686-w64-mingw32-gcc    (Fedora: mingw32-gcc)
  64-bit: x86_64-w64-mingw32-gcc  (Fedora: mingw64-gcc)
Override with CC32=... CC64=... ./build.sh
MSG
    exit 1
fi
