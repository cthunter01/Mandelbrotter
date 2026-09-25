#!/usr/bin/env bash
# Regenerates the application icon with Mandelbrotter's own renderer: the whole set in the classic palette on
# a rounded tile. Needs a release build and ImageMagick 7 (magick).
#
#   src/icons/make_icons.sh [path/to/Mandelbrotter]   (default: build/clang-release/bin/Mandelbrotter)
#
# Writes, next to this script:
#   mandelbrotter.png   256 px, embedded as the Linux window icon (src/CMakeLists.txt)
#   mandelbrotter.ico   16 to 256 px, the Windows executable and window icon (src/Mandelbrotter.rc)
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
exe=${1:-$here/../../build/clang-release/bin/Mandelbrotter}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

"$exe" --render "$work/render.png" --size 1024x1024 --center -0.62,0 --zoom 1.0 --iterations 1000 \
    --palette classic --supersample 4

# Rounded tile with a 5% margin and a thin dark edge, so it reads on light and dark backgrounds.
magick "$work/render.png" -alpha set \
    \( -size 1024x1024 xc:none -fill white -draw "roundrectangle 48,48 975,975 190,190" \) \
    -compose DstIn -composite \
    \( -size 1024x1024 xc:none -fill none -stroke 'rgba(0,0,0,0.55)' -strokewidth 6 \
       -draw "roundrectangle 50,50 973,973 188,188" \) \
    -compose Over -composite -depth 8 "$work/master.png"

# -strip drops the timestamps, so an unchanged icon regenerates byte for byte.
magick "$work/master.png" -filter Lanczos -resize 256x256 -strip "$here/mandelbrotter.png"
sizes=(256 64 48 40 32 24 20 16)
args=()
for size in "${sizes[@]}"; do
    args+=(\( -clone 0 -resize "${size}x${size}" \))
done
magick "$work/master.png" -filter Lanczos "${args[@]}" -delete 0 -strip "$here/mandelbrotter.ico"
echo "wrote $here/mandelbrotter.png and $here/mandelbrotter.ico"
