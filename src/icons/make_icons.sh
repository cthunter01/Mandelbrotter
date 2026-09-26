#!/usr/bin/env bash
# Regenerates the application icon with Mandelbrotter's own renderer: the whole set in the classic palette on
# a rounded tile. Needs a release build, ImageMagick 7 (magick) and Python 3 with Pillow (for the .icns).
#
#   src/icons/make_icons.sh [path/to/Mandelbrotter]   (default: build/clang-release/bin/Mandelbrotter)
#
# Writes, next to this script:
#   mandelbrotter.png    256 px: the Linux window icon (embedded) and the icon the .desktop file names
#   mandelbrotter.ico    16 to 256 px: the Windows executable and window icon (src/Mandelbrotter.rc)
#   mandelbrotter.icns   16 to 1024 px: the macOS bundle icon (src/CMakeLists.txt)
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
# ImageMagick cannot write .icns; Pillow can, with every size macOS asks for (including the @2x ones).
python3 -c 'import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2])' \
    "$work/master.png" "$here/mandelbrotter.icns"
echo "wrote mandelbrotter.png, mandelbrotter.ico and mandelbrotter.icns into $here"
