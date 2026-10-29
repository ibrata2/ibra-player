#!/usr/bin/env bash
# Assemble a self-contained iBRA PLAYER folder from an MSYS2 UCRT64 build.
# Usage: deploy.sh <build-dir> <stage-dir>
set -euo pipefail
BUILD="$1"
STAGE="$2"
PREFIX="${MINGW_PREFIX:-/ucrt64}"
HERE="$(cd "$(dirname "$0")" && pwd)"

rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$BUILD/iBRA PLAYER.exe" "$STAGE/"

# Qt plugins needed at runtime
QTPLUG="$PREFIX/share/qt6/plugins"
mkdir -p "$STAGE/platforms" "$STAGE/imageformats" "$STAGE/tls" "$STAGE/styles"
cp "$QTPLUG/platforms/qwindows.dll" "$STAGE/platforms/"
for f in qjpeg qico qgif qsvg qwebp; do
  [ -f "$QTPLUG/imageformats/$f.dll" ] && cp "$QTPLUG/imageformats/$f.dll" "$STAGE/imageformats/" || true
done
cp "$QTPLUG"/tls/*.dll "$STAGE/tls/" 2>/dev/null || true
cp "$QTPLUG"/styles/*.dll "$STAGE/styles/" 2>/dev/null || true
printf '[Paths]\nPlugins = .\n' > "$STAGE/qt.conf"

# DLLs that must always come from Windows itself. In particular opengl32.dll:
# MSYS2's Mesa provides one, and shipping it replaces the real GPU driver.
is_excluded() {
  case "${1,,}" in
    opengl32.dll|libgallium_wgl.dll|libglapi.dll|libgallium*.dll|dxil.dll|vulkan-1.dll|d3dcompiler_47.dll) return 0 ;;
  esac
  owner=$(pacman -Qqo "$PREFIX/bin/$1" 2>/dev/null || true)
  [ "$owner" = "mingw-w64-ucrt-x86_64-mesa" ] && return 0
  return 1
}

# Resolve every DLL dependency that lives inside the MSYS2 prefix (recursive)
collect() {
  ntldd -R "$1" 2>/dev/null | awk '{print $3}' | tr '\\' '/' | grep -i "/ucrt64/bin/" || true
}
declare -A seen
queue=("$STAGE/iBRA PLAYER.exe" "$STAGE"/platforms/*.dll "$STAGE"/imageformats/*.dll "$STAGE"/tls/*.dll "$STAGE"/styles/*.dll)
for target in "${queue[@]}"; do
  [ -f "$target" ] || continue
  while IFS= read -r dll; do
    [ -z "$dll" ] && continue
    unix=$(cygpath -u "$dll")
    name=$(basename "$unix")
    if is_excluded "$name"; then continue; fi
    if [ -z "${seen[$name]:-}" ] && [ -f "$unix" ]; then
      seen[$name]=1
      cp "$unix" "$STAGE/"
    fi
  done < <(collect "$target")
done
echo "Bundled ${#seen[@]} libraries"
for bad in opengl32.dll libgallium_wgl.dll libglapi.dll; do
  if [ -f "$STAGE/$bad" ]; then echo "::error::$bad must not be shipped"; exit 1; fi
done

# Licenses of every bundled MSYS2 package + our own
mkdir -p "$STAGE/licenses"
for dll in "$STAGE"/*.dll; do
  pkg=$(pacman -Qqo "$PREFIX/bin/$(basename "$dll")" 2>/dev/null || true)
  [ -z "$pkg" ] && continue
  real=${pkg#mingw-w64-ucrt-x86_64-}
  if [ -d "$PREFIX/share/licenses/$real" ]; then
    mkdir -p "$STAGE/licenses/$real"
    cp -r "$PREFIX/share/licenses/$real/." "$STAGE/licenses/$real/"
  fi
  echo "$real $(pacman -Q "$pkg" | awk '{print $2}')" >> "$STAGE/licenses/packages.txt"
done
sort -u -o "$STAGE/licenses/packages.txt" "$STAGE/licenses/packages.txt"
cp "$HERE/../../../LICENSE" "$STAGE/LICENSE.txt"
cp "$HERE/THIRD_PARTY_NOTICES.txt" "$STAGE/"
cp "$HERE/README-Windows.txt" "$STAGE/README.txt"
du -sh "$STAGE"
