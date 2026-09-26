#!/usr/bin/env bash
# Regenerates the README's pictures with Mandelbrotter itself: the banner and an animation of every demo
# flight. Every animation frame is rendered to completion (--flight-frames), so the dives stay sharp instead of
# showing the coarse first passes the window draws while it moves. Needs a release build, ImageMagick 7
# (magick) and ffmpeg (for the GIFs' palettes).
#
#   docs/readme/make_media.sh [path/to/Mandelbrotter]   (default: build/clang-release/bin/Mandelbrotter)
#
# Writes banner.jpg and demo-<flight>.gif next to this script. Takes a few minutes.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
exe=${1:-$here/../../build/clang-release/bin/Mandelbrotter}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The banner: a minibrot deep in Seahorse Valley, the end of the seahorse-dive flight at zoom 4e5.
"$exe" --render "$work/banner.png" --size 1600x480 --supersample 4 --palette classic \
    --center -0.743643887037158704752191506114774,0.131825904205311970493132056385139 --zoom 4e5
magick "$work/banner.png" -strip -quality 88 "$here/banner.jpg"

# Each flight is sampled at about 75 frames (frames per second of flight time below), played back at 12 fps
# with the last frame held for 2 s. 400x225, 128 colours and a light ordered dither keep a GIF near 3 MB:
# deep-zoom frames differ everywhere, so GIF's frame-to-frame compression cannot help.
flights=(seahorse-dive:2 antenna-minibrot:3 elephant-valley:3 feigenbaum:4 julia-sweep:2 palette-sweep:6)
for entry in "${flights[@]}"; do
    id=${entry%%:*}
    fps=${entry##*:}
    "$exe" --flight "$id" --flight-frames "$work/$id" --fps "$fps" --size 400x225 --supersample 2 >/dev/null
    ffmpeg -loglevel error -y -framerate 12 -i "$work/$id/frame-%04d.png" \
        -vf "tpad=stop_mode=clone:stop_duration=2,split[a][b];[a]palettegen=max_colors=128:stats_mode=full[p];[b][p]paletteuse=dither=bayer:bayer_scale=4" \
        -loop 0 "$here/demo-$id.gif"
    echo "wrote demo-$id.gif"
done
echo "wrote banner.jpg and the demo GIFs into $here"
