#!/usr/bin/env bash
# Uploads the shipped runtime assets (dist/PPSA98542/assets, built from pc_port/assets, which the PC build copies next to the
# executable) into the title folder on the console, where the game finds them at
# /app0/assets when nothing overrides them under /data/SilentHill. Only files that are
# missing are sent.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PS5_HOST="${PS5_HOST:-192.168.1.85}"
FTP_PORT="${FTP_PORT:-2121}"
ROOT="/data/homebrew/PPSA98542/assets"
ASSETS="$HERE/../dist/PPSA98542/assets"

sent=0
failed=0
while IFS= read -r f; do
    rel="${f#"$ASSETS"/}"
    dir="$(dirname "$rel")"
    url_dir="ftp://$PS5_HOST:$FTP_PORT$ROOT/"
    [ "$dir" != "." ] && url_dir="$url_dir$dir/"
    if curl -s --max-time 10 "$url_dir" | awk '{print $NF}' | grep -qxF "$(basename "$rel")"; then
        continue
    fi
    if curl -s --ftp-create-dirs -T "$f" "ftp://$PS5_HOST:$FTP_PORT$ROOT/$rel"; then
        sent=$((sent + 1))
    else
        failed=$((failed + 1))
        echo "asset upload failed: $rel" >&2
    fi
done < <(find "$ASSETS" -type f | sort)
echo "assets: $sent uploaded, $failed failed"
